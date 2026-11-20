// SPDX-License-Identifier: GPL-3.0-or-later
// Magnetar tests: the real Backend and DeviceWorker against the pretend keyboard, the theme
// and translator, and the whole QML window loaded offscreen.
#include "backend.h"
#include "fake_transport.h"
#include "theme.h"
#include "translator.h"

#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtTest>

#include <atomic>
#include <condition_variable>
#include <mutex>

using namespace akko;
using namespace akko::test;

namespace {

// One pretend keyboard shared by every session the backend opens, like the real board.
struct FakeBoard {
    std::mutex mutex;
    FakeKeyboard keyboard;
    std::vector<Bytes> written;       // every report sent, across sessions
    std::string failWith;             // when set, opening a session throws this
    bool gated = false;               // when set, opening a session waits for release()
    std::condition_variable gate;
    std::atomic<int> sessions{0};

    void release()
    {
        {
            std::lock_guard lock(mutex);
            gated = false;
        }
        gate.notify_all();
    }

    int writesOf(std::uint8_t cmd, std::uint8_t sub)
    {
        std::lock_guard lock(mutex);
        return int(std::count_if(written.begin(), written.end(),
                                 [&](const Bytes &w) { return w[1] == cmd && w[2] == sub; }));
    }
};

class FakeSession : public Session {
public:
    explicit FakeSession(FakeBoard &board) : board_(board)
    {
        transport_.respond = [this](const Bytes &w) {
            std::lock_guard lock(board_.mutex);
            board_.written.push_back(w);
            return board_.keyboard(w);
        };
    }
    Transport &transport() override { return transport_; }
    std::string path() const override { return "/dev/hidraw-test"; }

private:
    FakeBoard &board_;
    FakeTransport transport_;
};

SessionFactory fakeSessions(FakeBoard &board)
{
    return [&board]() -> std::unique_ptr<Session> {
        std::unique_lock lock(board.mutex);
        board.gate.wait(lock, [&] { return !board.gated; });
        if (!board.failWith.empty())
            throw Error(board.failWith);
        ++board.sessions;
        return std::make_unique<FakeSession>(board);
    };
}

QString readText(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

}  // namespace

class AppTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QQuickStyle::setStyle("Material");
    }

    // ---- backend -----------------------------------------------------------
    void startsByReadingTheKeyboard()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.ready());
        QVERIFY(b.connected());
        QCOMPARE(b.profile(), 1);
        QCOMPARE(b.profileCount(), 3);
        QCOMPARE(b.effect(), 2);
        QCOMPARE(b.color(), QString("#00ff00"));
        QCOMPARE(b.brightness(), 20);
        QCOMPARE(b.speed(), 4);
        QCOMPARE(b.firmware(), QString("v1.08"));
        QCOMPARE(b.devicePath(), QString("/dev/hidraw-test"));
        QCOMPARE(board.writesOf(0x04, 0x01), 0);  // reading never writes
    }

    void changesAreReadBack()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.ready());

        QVERIFY(b.setColor("#ff0000"));
        QTRY_COMPARE(b.color(), QString("#ff0000"));
        b.setRainbow(true);
        QTRY_VERIFY(b.rainbow());
        QVERIFY(b.setColor("0af"));  // a colour also leaves rainbow, like the CLI
        QTRY_COMPARE(b.color(), QString("#00aaff"));
        QVERIFY(!b.rainbow());
        b.setEffect(5);
        QTRY_COMPARE(b.effect(), 5);
        b.setSpeed(1);
        QTRY_COMPARE(b.speed(), 1);
        b.setProfile(2);
        QTRY_COMPARE(b.profile(), 2);

        const int writes = int(board.written.size());
        QVERIFY(!b.setColor("not a colour"));
        b.setProfile(9);   // out of range: ignored
        b.setEffect(0x14); // unknown effect: ignored
        b.refresh();
        QTRY_VERIFY(int(board.written.size()) > writes);
        QCOMPARE(board.writesOf(0x10, 0x00), 1);
        QCOMPARE(board.writesOf(0x04, 0x02), 1);
    }

    void lightsRememberTheirBrightness()
    {
        FakeBoard board;
        QTemporaryDir dir;
        const QString state = dir.filePath("akkoctl/state.json");
        Backend b(fakeSessions(board), state);
        QTRY_VERIFY(b.ready());

        b.setBrightness(7);
        QTRY_COMPARE(b.brightness(), 7);
        QCOMPARE(readText(state), QString("{\"lastBrightness\": 7}"));  // shared with akkoctl
        b.setLightsOn(false);
        QTRY_VERIFY(!b.lightsOn());
        b.setLightsOn(true);
        QTRY_COMPARE(b.brightness(), 7);
        b.toggleLights();
        b.toggleLights();  // two quick toggles both happen
        QTRY_COMPARE(board.writesOf(0x04, 0x01), 5);
        QTRY_COMPARE(b.brightness(), 7);
    }

    void sliderDragsSendOnlyTheLastValue()
    {
        FakeBoard board;
        board.gated = true;  // hold the first session open while the "drag" happens
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        for (int v = 1; v <= 15; ++v)
            b.setBrightness(v);
        board.release();
        QTRY_COMPARE(b.brightness(), 15);
        QCOMPARE(board.writesOf(0x04, 0x01), 1);
    }

    void errorsAreShownThenRecovered()
    {
        FakeBoard board;
        board.failWith = "keyboard busy (another akkoctl is running)";
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_COMPARE(b.errorKind(), QString("busy"));
        QVERIFY(!b.connected());
        QCOMPARE(b.errorDetail(), QString("keyboard busy (another akkoctl is running)"));
        {
            std::lock_guard lock(board.mutex);
            board.failWith.clear();
        }
        b.refresh();
        QTRY_VERIFY(b.connected());
        QVERIFY(b.errorKind().isEmpty());
        QVERIFY(b.ready());
    }

    void retriesOnItsOwn()
    {
        FakeBoard board;
        board.failWith = "keyboard not found (is it plugged in?)";
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_COMPARE(b.errorKind(), QString("notFound"));
        {
            std::lock_guard lock(board.mutex);
            board.failWith.clear();  // "plugged in"
        }
        QTRY_VERIFY_WITH_TIMEOUT(b.connected(), 5000);  // the 3 s retry, no refresh() call
    }

    void errorKinds()
    {
        QCOMPARE(Backend::classifyError("keyboard not found (is it plugged in?)"), QString("notFound"));
        QCOMPARE(Backend::classifyError("no permission for /dev/hidraw3 (udev rule missing?)"), QString("permission"));
        QCOMPARE(Backend::classifyError("keyboard busy (another akkoctl is running)"), QString("busy"));
        QCOMPARE(Backend::classifyError("no response to command 84 01"), QString("noResponse"));
        QCOMPARE(Backend::classifyError("keyboard disconnected"), QString("noResponse"));
        QCOMPARE(Backend::classifyError("cannot open /dev/hidraw3: No such device"), QString("other"));
    }

    // ---- theme and language --------------------------------------------------
    void themeFollowsOmarchyAccent()
    {
        QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.filePath("theme")));
        QFile colors(dir.filePath("theme/colors.toml"));
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("background = \"#000000\"\naccent = \"#7aa2f7\"\n");
        colors.close();
        Theme theme(dir.path(), nullptr, "/nonexistent-hyprctl");
        QCOMPARE(theme.accent(), QString("#7aa2f7"));
        QCOMPARE(Theme::readAccent(dir.filePath("missing.toml")), QString("#FFD60A"));
    }

    void translatorFollowsOmarchyLanguage()
    {
        QTemporaryDir dir;
        QFile language(dir.filePath("language"));
        QVERIFY(language.open(QIODevice::WriteOnly));
        language.write("fr\n");
        language.close();
        Translator t(dir.filePath("language"), ":/i18n");
        QCOMPARE(t.language(), QString("fr"));
        QCOMPARE(Translator::readLanguage(dir.filePath("nothing")), QString("en"));
    }

    // ---- the window ----------------------------------------------------------
    void windowLoadsWithoutWarnings()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend backend(fakeSessions(board), dir.filePath("state.json"));
        Theme theme(dir.path(), nullptr, "/nonexistent-hyprctl");
        Translator translator(dir.filePath("language"), ":/i18n");

        QQmlApplicationEngine engine;
        QList<QQmlError> warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &w) { warnings += w; });
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.load(QUrl("qrc:/Main.qml"));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QTRY_VERIFY(backend.ready());

        auto *firmware = window->findChild<QQuickItem *>("firmwareLabel");
        QVERIFY(firmware);
        QTRY_COMPARE(firmware->property("text").toString(), QString("v1.08"));
        auto *banner = window->findChild<QQuickItem *>("connectionBanner");
        QVERIFY(banner);
        QTRY_VERIFY(!banner->isVisible());

        QString text;
        for (const auto &w : warnings)
            text += w.toString() + "\n";
        QVERIFY2(warnings.isEmpty(), qPrintable(text));
    }
};

int runAppTests(int argc, char **argv)
{
    AppTests t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_app.moc"
