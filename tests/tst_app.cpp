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
#include <QColor>
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

// Finds an item by objectName in the visual tree (Repeater delegates aren't QObject children).
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (!root)
        return nullptr;
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *found = findItem(child, name))
            return found;
    return nullptr;
}

}  // namespace

// The window on the Keys page with a fake keyboard, read and ready.
struct KeysWindow {
    FakeBoard board;
    QTemporaryDir dir;
    Backend backend{fakeSessions(board), dir.filePath("state.json")};
    Theme theme{dir.path(), nullptr, "/nonexistent-hyprctl"};
    Translator translator{dir.filePath("language"), ":/i18n"};
    QQmlApplicationEngine engine;
    QList<QQmlError> warnings;
    QQuickWindow *window = nullptr;
    QQuickItem *page = nullptr, *view = nullptr;

    bool open()
    {
        QObject::connect(&engine, &QQmlEngine::warnings, [this](const QList<QQmlError> &w) { warnings += w; });
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("backend", &backend);
        engine.load(QUrl("qrc:/Main.qml"));
        window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window)
            return false;
        window->setProperty("page", 0);
        page = window->findChild<QQuickItem *>("keysPage");
        view = window->findChild<QQuickItem *>("keysKeyboard");
        return page && view;
    }
    QQuickItem *item(const char *name) const { return window->findChild<QQuickItem *>(name); }
    void click(int key, Qt::KeyboardModifiers mods = Qt::NoModifier) const
    {
        QMetaObject::invokeMethod(view, "keyClicked", Q_ARG(int, key), Q_ARG(int, int(mods)));
    }
    QString cap(int key) const { return findItem(view, "key_" + QByteArray::number(key))->findChild<QQuickItem *>()->property("text").toString(); }
    QString warningText() const
    {
        QString text;
        for (const auto &w : warnings)
            text += w.toString() + "\n";
        return text;
    }
};

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

    void settingsAreWrittenAndReadBack()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.ready());
        QVERIFY(!b.winKeyLock());
        QVERIFY(b.comboOptimization());
        QVERIFY(b.adaptiveCalibration());
        QCOMPARE(b.pollingRate(), 1000);
        QCOMPARE(b.pollingRates(), (QVariantList{125, 250, 500, 1000, 2000, 4000, 8000}));

        b.setWinKeyLock(true);
        QTRY_VERIFY(b.winKeyLock());
        b.setComboOptimization(false);
        QTRY_VERIFY(!b.comboOptimization());
        b.setAdaptiveCalibration(false);
        QTRY_VERIFY(!b.adaptiveCalibration());
        QCOMPARE(board.writesOf(0x04, 0x15), 1);
        QCOMPARE(board.writesOf(0x04, 0x18), 1);
        QCOMPARE(board.writesOf(0x04, 0x19), 1);

        QVERIFY(!b.setPollingRate(300));  // not a rate the board has
        b.refresh();
        QTRY_VERIFY(board.writesOf(0x84, 0x17) >= 3);
        QCOMPARE(board.writesOf(0x04, 0x17), 0);
    }

    void pollingRateWaitsForTheRestart()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.keymapReady());
        const int keymapReads = board.writesOf(0x83, 0x00);

        QVERIFY(b.setPollingRate(500));
        QTRY_VERIFY(b.restarting());
        QCOMPARE(b.pollingRate(), 500);  // the read-back, before the board goes away
        QCOMPARE(board.writesOf(0x04, 0x17), 1);
        {
            std::lock_guard lock(board.mutex);
            board.failWith = "keyboard not found (is it plugged in?)";  // off USB for a moment
        }
        QTest::qWait(Backend::kRestartWaitMs + 500);  // at least one failed attempt
        QVERIFY(b.restarting());
        QVERIFY(b.connected());
        QVERIFY(b.errorKind().isEmpty());  // no error banner while it restarts
        {
            std::lock_guard lock(board.mutex);
            board.failWith.clear();  // back
        }
        QTRY_VERIFY(!b.restarting());
        QVERIFY(b.connected());
        QCOMPARE(b.pollingRate(), 500);
        QVERIFY(board.writesOf(0x83, 0x00) > keymapReads);  // everything read again
    }

    void calibrationShowsDepthAndSavesOnFinish()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.ready());
        QCOMPARE(b.calibrationSaved(), -1);
        {
            // F12 (0d) pressed to 2204, then 2219: 10500 → 2204 of a stored 10500 → 1800 is 95 %.
            std::lock_guard lock(board.mutex);
            board.keyboard.calibrationReports.push_back(reply(0x94, 0x02, {0x00, 0x0d, 0xac, 0x41, 0x08, 0x9c}));
            board.keyboard.calibrationReports.push_back(reply(0x94, 0x02, {0x00, 0x0d, 0x2c, 0x40, 0x08, 0xab}));
            board.keyboard.calibrationReports.push_back(reply(0x94, 0x02, {0x00, 0x0c, 0xa0, 0x00, 0x20, 0x00}));
        }
        b.startCalibration();
        QVERIFY(b.calibrating());
        QTRY_COMPARE(b.calibrationDepth().value("13").toInt(), 95);
        QTRY_COMPARE(b.calibrationDepth().value("12").toInt(), 26);  // 0x2000 = 8192: a quarter of the way
        QCOMPARE(b.calibrationFull(), 1);
        QCOMPARE(board.writesOf(0x94, 0x05), 10);

        b.finishCalibration();
        QVERIFY(!b.calibrating());
        QTRY_COMPARE(b.calibrationSaved(), 2);
        QCOMPARE(board.writesOf(0x94, 0x04), 1);
        QCOMPARE(board.writesOf(0x94, 0x05), 20);  // read again after Finish
        std::lock_guard lock(board.mutex);
        QCOMPARE(board.keyboard.calibration[0x0d].second, std::uint16_t(0x089c));
        QCOMPARE(board.keyboard.calibration[0x0c].second, std::uint16_t(0x2000));
    }

    void calibrationFinishesWhenTheAppCloses()
    {
        FakeBoard board;
        QTemporaryDir dir;
        {
            Backend b(fakeSessions(board), dir.filePath("state.json"));
            QTRY_VERIFY(b.ready());
            b.startCalibration();
            QTRY_VERIFY(board.writesOf(0x94, 0x00) >= 1);
        }
        QCOMPARE(board.writesOf(0x94, 0x04), 1);
    }

    void keymapIsReadOnceAndKeptUpToDate()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.keymapReady());
        QCOMPARE(b.keymap().size(), 3);
        QCOMPARE(b.keymap()[0].toMap().value("1").toMap().value("code").toString(), QString("00 00 00 29"));
        QCOMPARE(board.writesOf(0x83, 0x00), 10);

        // A plain refresh (window focus) doesn't read the keymap again; Ctrl+R does.
        b.refresh();
        QTRY_VERIFY(board.writesOf(0x84, 0x01) >= 2);
        QCOMPARE(board.writesOf(0x83, 0x00), 10);
        b.reload();
        QTRY_COMPARE(board.writesOf(0x83, 0x00), 20);

        // A write sends 03 LL, reads the key back and shows the read-back.
        QVERIFY(b.setKeyAction(2, 0x62, b.comboCode(0x01, {0x19})));
        QTRY_COMPARE(b.keymap()[2].toMap().value(QString::number(0x62)).toMap().value("code").toString(),
                     QString("00 01 19 00"));
        QCOMPARE(board.writesOf(0x03, 0x02), 1);
        {
            std::lock_guard lock(board.mutex);
            QVERIFY(board.keyboard.keymap[2][0x62] == comboAction(0x01, std::vector<std::uint8_t>{0x19}));
        }
        QVERIFY(b.restoreKeyDefault(2, 0x62));
        QTRY_COMPARE(b.keymap()[2].toMap().value(QString::number(0x62)).toMap().value("code").toString(),
                     QString("00 00 00 49"));

        // The same action on several keys: one 03 LL per key.
        QVERIFY(b.setKeyActions(1, {0x62, 0x63, 0x64}, b.keyCode(0x04)));
        QTRY_VERIFY([&] {
            for (int key : {0x62, 0x63, 0x64})
                if (b.keymap()[1].toMap().value(QString::number(key)).toMap().value("code").toString() != "00 00 00 04")
                    return false;
            return true;
        }());
        QCOMPARE(board.writesOf(0x03, 0x01), 3);
        QVERIFY(!b.setKeyActions(1, {0x62, 0x7f}, b.keyCode(0x04)));  // one bad key: nothing
        QVERIFY(!b.setKeyActions(1, {}, b.keyCode(0x04)));

        // Switching profile reads the new profile's keymap.
        const int reads = board.writesOf(0x83, 0x00);
        b.setProfile(2);
        QTRY_COMPARE(board.writesOf(0x83, 0x00), reads + 10);

        // Bad input sends nothing.
        QVERIFY(!b.setKeyAction(3, 0x62, "00 00 00 04"));
        QVERIFY(!b.setKeyAction(0, 0x7f, "00 00 00 04"));
        QVERIFY(!b.setKeyAction(0, 0x62, "nonsense"));
        QCOMPARE(b.comboCode(0, {0x04, 0x05}), QString());
        QCOMPARE(b.mouseCode(4), QString());
    }

    void keyColorsAreSetAndReadBack()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.colorsReady());
        QCOMPARE(b.keyColors().size(), kKeyCount);
        QCOMPARE(b.keyColors().value("13").toString(), QString("#000000"));
        QCOMPARE(board.writesOf(0x86, 0x00), 8);

        // Nine keys, one colour: two 06 00 frames (8 keys at most each), then the read-back shows.
        const QVariantList keys{0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05};
        QVERIFY(b.setKeyColors(keys, "#0080ff"));
        QTRY_COMPARE(b.keyColors().value("13").toString(), QString("#0080ff"));
        QCOMPARE(board.writesOf(0x06, 0x00), 2);
        QCOMPARE(b.keyColors().value("5").toString(), QString("#0080ff"));
        QCOMPARE(b.keyColors().value("1").toString(), QString("#000000"));
        {
            std::lock_guard lock(board.mutex);
            QVERIFY((board.keyboard.colors[0x0d] == std::array<std::uint8_t, 3>{0x00, 0x80, 0xff}));
        }
        QCOMPARE(board.writesOf(0x04, 0x01), 0);  // the effect is left alone

        // Bad input sends nothing.
        const int writes = int(board.written.size());
        QVERIFY(!b.setKeyColors({0x0d}, "blue"));
        QVERIFY(!b.setKeyColors({0x7f}, "#ffffff"));
        QVERIFY(!b.setKeyColors({}, "#ffffff"));
        QCOMPARE(int(board.written.size()), writes);
        QCOMPARE(b.customEffect(), 0x13);
    }

    void travelIsChangedAndUndone()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.travelReady());
        QCOMPARE(b.travel().size(), kKeyCount);
        auto f12 = [&] { return b.travel().value(QString::number(0x0d)).toMap(); };
        QCOMPARE(f12().value("actuation").toDouble(), 0.6);
        QCOMPARE(board.writesOf(0x93, 0x00), 8);

        // One change on two keys: 13 00 once (one packet), then the read-back shows.
        QVERIFY(b.changeTravel({0x0d, 0x0c}, {{"actuation", 0.42}}));
        QTRY_COMPARE(f12().value("actuation").toDouble(), 0.42);
        QCOMPARE(board.writesOf(0x13, 0x00), 1);
        QVERIFY(b.canUndoTravel());
        {
            std::lock_guard lock(board.mutex);
            QCOMPARE(board.keyboard.travel[0x0c].actuation, std::uint16_t(84));
            QCOMPARE(board.keyboard.travel[0x01].actuation, std::uint16_t(120));
        }

        // Rapid Trigger on from 0 / 0 gets 0.005 / 0.005; a later sensitivity change keeps it on.
        QVERIFY(b.changeTravel({0x0d}, {{"rapidTrigger", true}}));
        QVERIFY(b.changeTravel({0x0d}, {{"press", 0.17}, {"release", 0.285}}));
        QTRY_COMPARE(f12().value("release").toDouble(), 0.285);
        QVERIFY(f12().value("rapidTrigger").toBool());
        QCOMPARE(f12().value("press").toDouble(), 0.17);

        // Undo puts back the values from before the last change only.
        b.undoTravel();
        QTRY_COMPARE(f12().value("release").toDouble(), 0.005);
        QVERIFY(f12().value("rapidTrigger").toBool());
        QTRY_VERIFY(!b.canUndoTravel());

        // Dead zones: one 16 00 frame per key.
        const int before = board.writesOf(0x16, 0x00);
        QVERIFY(b.changeTravel({0x01, 0x02, 0x03}, {{"deadZones", true}, {"deadTop", 0.185}}));
        QTRY_COMPARE(board.writesOf(0x16, 0x00), before + 3);
        QTRY_COMPARE(b.travel().value("2").toMap().value("deadTop").toDouble(), 0.185);

        // Bad input sends nothing.
        QVERIFY(!b.changeTravel({0x0d}, {{"actuation", 0.05}}));              // under 0.1 mm
        QVERIFY(!b.changeTravel({0x0d}, {{"actuation", 4.0}}));               // over 3.5 mm
        QVERIFY(!b.changeTravel({0x0d}, {{"press", 1.5}}));                   // over our cap
        QVERIFY(!b.changeTravel({0x0d}, {{"actuation", 1.0}, {"press", 0.1}}));  // two parts
        QVERIFY(!b.changeTravel({0x7f}, {{"actuation", 1.0}}));               // no such key
        QVERIFY(!b.changeTravel({}, {{"actuation", 1.0}}));
        QVERIFY(!b.changeTravel({0x0d}, {{"colour", 1.0}}));
    }

    void advancedKeysAreSavedAndRemoved()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.advancedReady());
        QVERIFY(b.advancedKeys().isEmpty());
        QCOMPARE(b.advancedLimits().value("maxHoldMs").toInt(), 1000);

        // Mod-Tap on P: hold R, tap T, 322 ms, as in akko-capture2.txt.
        const QVariantMap mt{{"type", "modTap"}, {"key", 0x26}, {"action", "00 00 00 15"},
                             {"tap", "00 00 00 17"}, {"holdMs", 322}};
        QVERIFY(b.advancedValid(mt));
        QVERIFY(b.saveAdvanced({mt}));
        QTRY_COMPARE(b.advancedKeys().size(), 1);
        QCOMPARE(b.advancedKeys()[0].toMap(), mt);
        {
            std::lock_guard lock(board.mutex);
            QCOMPARE(board.keyboard.advanced.at(0x26).first, std::uint8_t(0x02));
        }

        // DKS on Space: depths in mm, rows by stage; an unused row is empty.
        const QVariantList rows{QVariantMap{{"action", "00 00 00 05"}, {"from", 1}, {"to", 2}},
                                QVariantMap{{"action", "00 00 00 06"}, {"from", 3}, {"to", 3}},
                                QVariantMap{{"action", ""}, {"from", -1}, {"to", -1}},
                                QVariantMap{{"action", ""}, {"from", -1}, {"to", -1}}};
        const QVariantMap dks{{"type", "dks"}, {"key", 0x46}, {"depths", QVariantList{1.0, 3.0, 3.0, 1.0}}, {"rows", rows}};
        QVERIFY(b.saveAdvanced({dks}));
        QTRY_COMPARE(b.advancedKeys().size(), 2);
        QVariantMap read;
        for (const auto &v : b.advancedKeys())
            if (v.toMap().value("key").toInt() == 0x46)
                read = v.toMap();
        QCOMPARE(read.value("depths").toList().at(1).toDouble(), 3.0);
        const auto row0 = read.value("rows").toList().at(0).toMap();
        QCOMPARE(row0.value("from").toInt(), 1);
        QCOMPARE(row0.value("stages").toString(), QString("00 0e 00 00"));
        QCOMPARE(read.value("rows").toList().at(2).toMap().value("action").toString(), QString());

        // SOCD A + D, then Release on D: the pair goes, D gets the new key.
        QVERIFY(b.saveAdvanced({QVariantMap{{"type", "socd"}, {"key", 0x2b}, {"key2", 0x2d}, {"mode", "lastInput"}}}));
        QTRY_COMPARE(b.advancedKeys().size(), 3);
        QVERIFY(b.saveAdvanced({QVariantMap{{"type", "release"}, {"key", 0x2d}, {"action", "00 00 00 22"}}}));
        QTRY_VERIFY([&] {
            for (const auto &v : b.advancedKeys())
                if (v.toMap().value("type") == "release")
                    return true;
            return false;
        }());
        QCOMPARE(b.advancedKeys().size(), 3);

        // Removing clears the key on the board.
        QVERIFY(b.removeAdvanced({0x26}));
        QTRY_COMPARE(b.advancedKeys().size(), 2);
        {
            std::lock_guard lock(board.mutex);
            QVERIFY(!board.keyboard.advanced.contains(0x26));
        }

        // Bad records send nothing.
        const int writes = board.writesOf(0x12, 0x00);
        QVERIFY(!b.saveAdvanced({QVariantMap{{"type", "modTap"}, {"key", 0x26}, {"action", "00 00 00 15"}, {"tap", ""}, {"holdMs", 322}}}));
        QVERIFY(!b.saveAdvanced({QVariantMap{{"type", "toggle"}, {"key", 0x31}, {"action", "00 00 00 0e"}, {"holdMs", 5000}}}));
        QVERIFY(!b.saveAdvanced({QVariantMap{{"type", "socd"}, {"key", 0x2b}, {"key2", 0x2b}, {"mode", "neutral"}}}));
        QVERIFY(!b.saveAdvanced({QVariantMap{{"type", "teleport"}, {"key", 0x26}}}));
        QVERIFY(!b.saveAdvanced({QVariantMap{{"type", "release"}, {"key", 0x7f}, {"action", "00 00 00 22"}}}));
        QVERIFY(!b.removeAdvanced({0x7f}));
        QVERIFY(!b.saveAdvanced({}));
        QVERIFY(!b.saveAdvanced({mt, mt}));  // two records on one key
        QVERIFY(!b.removeAdvanced({}));
        QTest::qWait(50);
        QCOMPARE(board.writesOf(0x12, 0x00), writes);

        // The same Mod-Tap on three keys in one save, then two of them removed in one go.
        QVariantList three;
        for (int key : {0x26, 0x31, 0x38}) {
            QVariantMap r = mt;
            r["key"] = key;
            three.append(r);
        }
        QVERIFY(b.saveAdvanced(three));
        QTRY_COMPARE(b.advancedKeys().size(), 5);
        QVERIFY(b.removeAdvanced({0x26, 0x31}));
        QTRY_COMPARE(b.advancedKeys().size(), 3);
    }

    void macrosAreSavedAndDeleted()
    {
        FakeBoard board;
        QTemporaryDir dir;
        Backend b(fakeSessions(board), dir.filePath("state.json"));
        QTRY_VERIFY(b.macrosReady());
        QCOMPARE(b.macros().size(), 0);
        QCOMPARE(b.macroStorage().value("used").toInt(), 0);
        QCOMPARE(b.macroStorage().value("max").toInt(), 4096);

        auto step = [](bool press, int usage, int delay) {
            return QVariantMap{{"press", press}, {"usage", usage}, {"delay", delay}};
        };
        auto record = [&](const QString &name, int usage) {
            return QVariantMap{{"name", name},
                               {"steps", QVariantList{step(true, 0xe1, 50), step(true, usage, 30),
                                                      step(false, usage, 20), step(false, 0xe1, 50)}}};
        };

        // Added one after the other; the board keeps 1 ms on the last step.
        QVERIFY(b.macroValid(record("ab", 0x04)));
        QVERIFY(b.saveMacro(-1, record("ab", 0x04)));
        QTRY_COMPARE(b.macros().size(), 1);
        QVERIFY(b.saveMacro(-1, record("cd", 0x05)));
        QVERIFY(b.saveMacro(-1, record("ef", 0x06)));
        QTRY_COMPARE(b.macros().size(), 3);
        const auto first = b.macros()[0].toMap();
        QCOMPARE(first.value("name").toString(), QString("ab"));
        const auto steps = first.value("steps").toList();
        QCOMPARE(steps.size(), 4);
        QCOMPARE(steps[0].toMap().value("usage").toInt(), 0xe1);
        QCOMPARE(steps[1].toMap().value("delay").toInt(), 30);
        QCOMPARE(steps[2].toMap().value("press").toBool(), false);
        QCOMPARE(steps[3].toMap().value("delay").toInt(), 1);
        Macro ab{"ab", {{true, 50, 0xe1}, {true, 30, 0x04}, {false, 20, 0x04}, {false, 1, 0xe1}}};
        QCOMPARE(first.value("size").toInt(), int(macroSize(ab)));
        std::vector<Macro> onBoard;
        {
            std::lock_guard lock(board.mutex);
            onBoard = parseMacroBlob(board.keyboard.macros);
        }
        QCOMPARE(onBoard.size(), std::size_t(3));
        QVERIFY(onBoard[0] == ab);
        QCOMPARE(b.macroStorage().value("used").toInt(), int(macroStorageUsed(onBoard)));

        // Bound to keys: the code says which macro and how it loops.
        const QString runEf = b.macroCode(2, "times", 3), holdCd = b.macroCode(1, "whileHeld");
        QCOMPARE(runEf, QString("03 01 03 02"));
        QCOMPARE(holdCd, QString("03 04 00 01"));
        const auto info = b.actionInfo(runEf);
        QCOMPARE(info.value("kind").toString(), QString("macro"));
        QCOMPARE(info.value("value").toInt(), 2);
        QCOMPARE(info.value("loop").toString(), QString("times"));
        QCOMPARE(info.value("count").toInt(), 3);
        QVERIFY(b.setKeyAction(0, 0x62, runEf));
        QVERIFY(b.setKeyAction(1, 0x63, holdCd));
        auto keyCode = [&](int layer, int key) {
            return b.keymap()[layer].toMap().value(QString::number(key)).toMap().value("code").toString();
        };
        QTRY_COMPARE(keyCode(1, 0x63), holdCd);

        // Deleting "cd" unbinds its key and moves the key on "ef" down to its new index.
        QVERIFY(b.deleteMacro(1));
        QTRY_COMPARE(b.macros().size(), 2);
        QCOMPARE(b.macros()[1].toMap().value("name").toString(), QString("ef"));
        QTRY_COMPARE(keyCode(0, 0x62), QString("03 01 03 01"));
        QCOMPARE(keyCode(1, 0x63), b.unboundCode());

        // Editing replaces in place.
        QVERIFY(b.saveMacro(0, record("gh", 0x07)));
        QTRY_COMPARE(b.macros()[0].toMap().value("name").toString(), QString("gh"));
        QCOMPARE(b.macros().size(), 2);

        // Refused, nothing sent: bad records, a missing index, more than the storage.
        const int writes = board.writesOf(0x05, 0x00);
        QVERIFY(!b.macroValid(record("a", 0x04)));
        QVERIFY(!b.saveMacro(-1, record("a", 0x04)));
        QVERIFY(!b.saveMacro(-1, QVariantMap{{"name", "ok"}, {"steps", QVariantList{}}}));
        QVERIFY(!b.saveMacro(-1, QVariantMap{{"name", "ok"}, {"steps", QVariantList{step(true, 0, 50)}}}));
        QVERIFY(!b.saveMacro(-1, QVariantMap{{"name", "ok"}, {"steps", QVariantList{step(true, 0x100, 50)}}}));
        QVERIFY(!b.saveMacro(2, record("ij", 0x04)));
        QVERIFY(!b.saveMacro(-2, record("ij", 0x04)));
        QVariantList many;
        for (int i = 0; i < 2000; ++i)
            many.append(step(i % 2 == 0, 0x04, 50));
        QVERIFY(!b.saveMacro(-1, QVariantMap{{"name", "big"}, {"steps", many}}));
        QVERIFY(!b.deleteMacro(2));
        QVERIFY(!b.deleteMacro(-1));
        QTest::qWait(50);
        QCOMPARE(board.writesOf(0x05, 0x00), writes);

        // Bad binding input gives no code.
        QCOMPARE(b.macroCode(256, "times", 1), QString());
        QCOMPARE(b.macroCode(0, "times", 0), QString());
        QCOMPARE(b.macroCode(0, "times", 256), QString());
        QCOMPARE(b.macroCode(0, "forever"), QString());
        QCOMPARE(b.macroCode(0, "untilKeyPressed"), QString("03 02 00 00"));

        // Macros are per profile: a switch reads them again.
        const int reads = board.writesOf(0x85, 0x00);
        b.setProfile(2);
        QTRY_VERIFY(board.writesOf(0x85, 0x00) > reads);
    }

    void scanCodesGiveHidUsages()
    {
        Backend b([]() -> std::unique_ptr<Session> { throw Error("keyboard not found"); });
        auto usage = [&](int evdev) { return b.usageForScanCode(evdev + 8); };
        QCOMPARE(usage(30), 0x04);   // A
        QCOMPARE(usage(16), 0x14);   // Q
        QCOMPARE(usage(50), 0x10);   // M
        QCOMPARE(usage(2), 0x1e);    // 1
        QCOMPARE(usage(11), 0x27);   // 0
        QCOMPARE(usage(1), 0x29);    // Esc
        QCOMPARE(usage(57), 0x2c);   // Space
        QCOMPARE(usage(88), 0x45);   // F12
        QCOMPARE(usage(86), 0x64);   // ISO key
        QCOMPARE(usage(111), 0x4c);  // Delete
        QCOMPARE(usage(103), 0x52);  // Up
        QCOMPARE(usage(194), 0x73);  // F24
        // Modifiers: e0 LCtrl, e1 LShift, e2 LAlt, e3 LGui, e4 RCtrl, e5 RShift, e6 RAlt, e7 RGui.
        const int mods[] = {29, 42, 56, 125, 97, 54, 100, 126};
        for (int i = 0; i < 8; ++i)
            QCOMPARE(usage(mods[i]), 0xe0 + i);
        QCOMPARE(usage(0), 0);
        QCOMPARE(usage(84), 0);  // no key
        QCOMPARE(b.usageForScanCode(0), 0);
        QCOMPARE(b.usageForScanCode(100000), 0);
    }

    void actionInfoDescribesEveryKind()
    {
        FakeBoard board;
        Backend b(fakeSessions(board), QTemporaryDir().filePath("state.json"));
        const auto combo = b.actionInfo("00 03 05 04");
        QCOMPARE(combo.value("kind").toString(), QString("key"));
        QCOMPARE(combo.value("modifiers").toInt(), 3);
        QCOMPARE(combo.value("keys").toList(), (QVariantList{4, 5}));
        QCOMPARE(b.actionInfo("02 00 00 e9").value("value").toInt(), 0xe9);
        QCOMPARE(b.actionInfo("0d 01 00 00").value("kind").toString(), QString("layer"));
        QCOMPARE(b.actionInfo("ff ff ff ff").value("kind").toString(), QString("unbound"));
        QCOMPARE(b.actionInfo("09 00 00 02").value("value").toInt(), 2);
        QVERIFY(b.actionInfo("zz").isEmpty());
        QCOMPARE(b.factoryAction(1, 0x40).value("code").toString(), QString("02 00 00 e9"));
        QCOMPARE(b.layout().size(), 83);
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

    void themeFollowsWholeOmarchyPalette()
    {
        QTemporaryDir dir;
        // No colors.toml: today's dark greys.
        Theme theme(dir.path(), nullptr, "/nonexistent-hyprctl");
        QVERIFY(theme.dark());
        QCOMPARE(theme.background(), QColor("#0e0e10"));
        QCOMPARE(theme.foreground(), QColor("#ffffff"));
        QCOMPARE(theme.sidebar(), QColor("#141417"));
        QCOMPARE(theme.card(), QColor("#17171b"));
        QCOMPARE(theme.key(), QColor("#202026"));
        QCOMPARE(theme.error(), QColor("#ff8a80"));

        // A dark theme appears: keys parsed, surfaces between background and foreground.
        QSignalSpy spy(&theme, &Theme::changed);
        QVERIFY(QDir().mkpath(dir.filePath("theme")));
        auto write = [&](const QByteArray &text) {
            QFile colors(dir.filePath("theme/colors.toml"));
            QVERIFY(colors.open(QIODevice::WriteOnly | QIODevice::Truncate));
            colors.write(text);
        };
        write("mode = \"dark\"\naccent = \"#7aa2f7\"\nbackground = \"#1a1b26\"\n"
              "foreground = \"#a9b1d6\"\nred = \"#f7768e\"\ngreen = \"#9ece6a\"\n");
        QTRY_COMPARE(theme.background(), QColor("#1a1b26"));
        QVERIFY(spy.count() >= 1);
        QVERIFY(theme.dark());
        QCOMPARE(theme.foreground(), QColor("#a9b1d6"));
        QCOMPARE(theme.error(), QColor("#f7768e"));
        QCOMPARE(theme.success(), QColor("#9ece6a"));
        QCOMPARE(theme.warning(), QColor("#8a6a1c"));  // missing key: fallback
        auto lightness = [](const QColor &c) { return c.lightnessF(); };
        QVERIFY(lightness(theme.background()) < lightness(theme.card()));
        QVERIFY(lightness(theme.card()) < lightness(theme.key()));
        QVERIFY(lightness(theme.key()) < lightness(theme.keyBorder()));
        QVERIFY(lightness(theme.keyBorder()) < lightness(theme.foreground()));

        // Live switch to a light theme: Material goes light, surfaces darken toward the text.
        write("mode = \"light\"\naccent = \"#1e66f5\"\nbackground = \"#eff1f5\"\nforeground = \"#4c4f69\"\n");
        QTRY_VERIFY(!theme.dark());
        QCOMPARE(theme.accent(), QString("#1e66f5"));
        QCOMPARE(theme.background(), QColor("#eff1f5"));
        QVERIFY(lightness(theme.card()) < lightness(theme.background()));
        QVERIFY(lightness(theme.key()) < lightness(theme.card()));
        QCOMPARE(theme.error(), QColor("#ff8a80"));  // red gone: fallback

        // No mode key: judged by the background.
        write("background = \"#ffffff\"\nforeground = \"#000000\"\n");
        QTRY_COMPARE(theme.background(), QColor("#ffffff"));
        QVERIFY(!theme.dark());
        QCOMPARE(Theme::readValues(dir.filePath("theme/colors.toml")).value("foreground"), QString("#000000"));
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

    void everyStringIsTranslatedToFrench()
    {
        // Every qsTr("…") in the QML has a French entry, and French has no leftovers.
        QSet<QString> used;
        const QRegularExpression call("qsTr\\(\"((?:[^\"\\\\]|\\\\.)*)\"");  // qsTr("…"), escapes allowed
        QDirIterator it(APP_SRC_DIR, {"*.qml"});
        while (it.hasNext()) {
            const QString source = readText(it.next());
            for (auto m = call.globalMatch(source); m.hasNext();)
                used.insert(m.next().captured(1).replace("\\\"", "\""));
        }
        QVERIFY(used.size() > 50);
        const QJsonObject fr = QJsonDocument::fromJson(readText(":/i18n/fr.json").toUtf8()).object();
        QStringList missing, stale;
        for (const auto &s : used)
            if (fr.value(s).toString().isEmpty())
                missing << s;
        for (const auto &k : fr.keys())
            if (!used.contains(k))
                stale << k;
        QVERIFY2(missing.isEmpty(), qPrintable("missing in fr.json: " + missing.join(" | ")));
        QVERIFY2(stale.isEmpty(), qPrintable("unused in fr.json: " + stale.join(" | ")));
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

    void keysPageRemapsKeys()
    {
        KeysWindow w;
        QVERIFY(w.open());
        QTRY_VERIFY(w.backend.keymapReady() && w.backend.travelReady() && w.backend.advancedReady());
        auto *tab = w.item("actionTab");
        QVERIFY(tab);
        QTRY_COMPARE(w.cap(0x01), QString("Esc"));

        // Select Insert and give it F5.
        w.click(0x62);
        QCOMPARE(tab->property("selection").toList().size(), 1);
        QCOMPARE(tab->property("selection").toList().first().toInt(), 0x62);
        QMetaObject::invokeMethod(tab, "assign", Q_ARG(QVariant, w.backend.keyCode(0x3e)));
        QTRY_COMPARE(w.backend.keymap()[0].toMap().value("98").toMap().value("code").toString(), QString("00 00 00 3e"));
        QTRY_COMPARE(w.cap(0x62), QString("F5"));
        QCOMPARE(w.item("currentAction")->property("text").toString(), QString("F5"));

        // A plain click selects only that key; Ctrl and Shift add keys. A on all three.
        w.click(0x63, Qt::ControlModifier);
        w.click(0x64, Qt::ShiftModifier);
        QCOMPARE(tab->property("selection").toList().size(), 3);
        QMetaObject::invokeMethod(tab, "assign", Q_ARG(QVariant, w.backend.keyCode(0x04)));
        QTRY_VERIFY(w.cap(0x62) == "A" && w.cap(0x63) == "A" && w.cap(0x64) == "A");
        QCOMPARE(w.item("currentAction")->property("text").toString(), QString("A"));
        // Ctrl+click again takes a key out.
        w.click(0x64, Qt::ControlModifier);
        QCOMPARE(tab->property("selection").toList().size(), 2);

        // Restore default puts every selected key back.
        auto *restore = w.item("restoreButton");
        QVERIFY(restore->property("enabled").toBool());
        QMetaObject::invokeMethod(restore, "clicked");
        QTRY_VERIFY(w.cap(0x62) == "Ins" && w.cap(0x63) != "A");
        QCOMPARE(w.cap(0x64), QString("A"));

        // Moving the only Fn key asks first, and sends nothing until confirmed.
        w.click(0x48);
        const int writes = w.board.writesOf(0x03, 0x00);
        QMetaObject::invokeMethod(tab, "assign", Q_ARG(QVariant, w.backend.keyCode(0x04)));
        auto *confirm = w.window->findChild<QObject *>("confirmLayerKey");
        QVERIFY(confirm);
        QTRY_VERIFY(confirm->property("visible").toBool());
        QCOMPARE(w.board.writesOf(0x03, 0x00), writes);

        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }

    void keysPageSavesAdvancedKeys()
    {
        KeysWindow w;
        QVERIFY(w.open());
        QTRY_VERIFY(w.backend.keymapReady() && w.backend.travelReady() && w.backend.advancedReady());
        w.page->setProperty("tab", 2);
        auto *tab = w.item("advancedTab");
        QVERIFY(tab);
        QTRY_COMPARE(w.cap(0x26), QString("P"));

        // P, Mod-Tap, hold R, tap T: Save stays off until both actions are there.
        w.click(0x26);
        QMetaObject::invokeMethod(tab, "chooseType", Q_ARG(QVariant, "modTap"));
        auto *save = w.item("saveAdvanced");
        QVERIFY(save);
        QVERIFY(!save->property("enabled").toBool());
        QMetaObject::invokeMethod(tab, "setSlot", Q_ARG(QVariant, 0), Q_ARG(QVariant, w.backend.keyCode(0x15)));
        QVERIFY(!save->property("enabled").toBool());
        QMetaObject::invokeMethod(tab, "setSlot", Q_ARG(QVariant, 1), Q_ARG(QVariant, w.backend.keyCode(0x17)));
        QTRY_VERIFY(save->property("enabled").toBool());
        QMetaObject::invokeMethod(save, "clicked");
        QTRY_COMPARE(w.cap(0x26), QString("MT"));
        // The read-back becomes the draft: nothing left to save.
        QTRY_VERIFY(!tab->property("dirty").toBool());

        // The same Mod-Tap on P, J and Z: one save, three records.
        w.click(0x31, Qt::ControlModifier);
        w.click(0x38, Qt::ControlModifier);
        QVERIFY(tab->property("saved").isNull());  // P has one, J and Z don't
        QMetaObject::invokeMethod(tab, "chooseType", Q_ARG(QVariant, "modTap"));
        QMetaObject::invokeMethod(tab, "setSlot", Q_ARG(QVariant, 0), Q_ARG(QVariant, w.backend.keyCode(0x15)));
        QMetaObject::invokeMethod(tab, "setSlot", Q_ARG(QVariant, 1), Q_ARG(QVariant, w.backend.keyCode(0x17)));
        QTRY_VERIFY(save->property("enabled").toBool());
        QMetaObject::invokeMethod(save, "clicked");
        QTRY_VERIFY(w.cap(0x31) == "MT" && w.cap(0x38) == "MT");
        QCOMPARE(w.backend.advancedKeys().size(), 3);
        // Now they share one record: the editor shows it, nothing to save.
        QTRY_VERIFY(!tab->property("saved").isNull());
        QTRY_VERIFY(!tab->property("dirty").toBool());

        // SOCD needs exactly two keys.
        auto *socd = findItem(w.window->contentItem(), "type_socd");
        QVERIFY(socd);
        QVERIFY(!socd->property("enabled").toBool());
        w.click(0x2b);
        w.click(0x2d, Qt::ControlModifier);
        QVERIFY(socd->property("enabled").toBool());
        QMetaObject::invokeMethod(tab, "chooseType", Q_ARG(QVariant, "socd"));
        QCOMPARE(tab->property("draft").toMap().value("key2").toInt(), 0x2d);
        QTRY_VERIFY(save->property("enabled").toBool());
        QMetaObject::invokeMethod(save, "clicked");
        QTRY_COMPARE(w.backend.advancedKeys().size(), 4);

        // Remove on several keys: P and J go, Z stays.
        w.click(0x26);
        w.click(0x31, Qt::ControlModifier);
        auto *remove = w.item("removeAdvanced");
        QTRY_VERIFY(remove->property("visible").toBool());
        QMetaObject::invokeMethod(remove, "clicked");
        QTRY_COMPARE(w.backend.advancedKeys().size(), 2);
        QCOMPARE(w.cap(0x38), QString("MT"));

        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }

    void keysPageColoursKeys()
    {
        KeysWindow w;
        QVERIFY(w.open());
        QTRY_VERIFY(w.backend.keymapReady() && w.backend.colorsReady());
        w.page->setProperty("tab", 3);
        auto *tab = w.item("colorTab");
        QVERIFY(tab);
        auto *f12 = findItem(w.view, "key_13");
        QTRY_COMPARE(f12->property("color").value<QColor>(), QColor("#000000"));
        QCOMPARE(w.cap(0x0d), QString("F12"));

        // The effect isn't Custom: the page says so and offers to switch.
        auto *hint = w.item("customHint");
        QVERIFY(hint && hint->isVisible());

        // F12 and Esc blue: the drawing shows the board's read-back.
        w.click(0x0d);
        w.click(0x01, Qt::ControlModifier);
        QMetaObject::invokeMethod(tab, "apply", Q_ARG(QVariant, "#0000ff"));
        QTRY_COMPARE(f12->property("color").value<QColor>(), QColor("#0000ff"));
        QCOMPARE(findItem(w.view, "key_1")->property("color").value<QColor>(), QColor("#0000ff"));
        QCOMPARE(tab->property("shared").toString(), QString("#0000ff"));
        w.click(0x02, Qt::ControlModifier);
        QCOMPARE(tab->property("shared").toString(), QString());  // mixed

        // Switch to Custom: one 04 02 with 13, and the hint goes.
        auto *button = w.item("switchToCustom");
        QVERIFY(button);
        QMetaObject::invokeMethod(button, "clicked");
        QTRY_COMPARE(w.backend.effect(), 0x13);
        QTRY_VERIFY(!hint->isVisible());

        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }

    void settingsPageShowsTheReadBack()
    {
        KeysWindow w;
        QVERIFY(w.open());
        QTRY_VERIFY(w.backend.ready());
        w.window->setProperty("page", 3);
        QVERIFY(w.item("settingsPage")->isVisible());
        auto *winLock = w.item("winKeyLockSwitch");
        auto *combo = w.item("comboOptimizationSwitch");
        auto *rate = w.item("pollingRateBox");
        QVERIFY(winLock && combo && rate);
        QCOMPARE(winLock->property("checked").toBool(), false);
        QCOMPARE(combo->property("checked").toBool(), true);
        QCOMPARE(rate->property("displayText").toString(), QString("1000 Hz"));

        // A click writes, and the switch shows the board's read-back.
        winLock->setProperty("checked", true);
        QMetaObject::invokeMethod(winLock, "toggled");
        QTRY_VERIFY(w.backend.winKeyLock());
        QCOMPARE(winLock->property("checked").toBool(), true);
        QCOMPARE(w.board.writesOf(0x04, 0x15), 1);

        // The board keeps the old value: the switch goes back to it.
        {
            std::lock_guard lock(w.board.mutex);
            w.board.keyboard.ignoreBaseWrites = true;
        }
        combo->setProperty("checked", false);
        QMetaObject::invokeMethod(combo, "toggled");
        QTRY_VERIFY(!w.backend.errorKind().isEmpty());
        QCOMPARE(combo->property("checked").toBool(), true);
        {
            std::lock_guard lock(w.board.mutex);
            w.board.keyboard.ignoreBaseWrites = false;
        }
        w.backend.refresh();
        QTRY_VERIFY(w.backend.connected());

        // Polling rate: the box shows the new rate once the keyboard is back.
        const int i500 = w.backend.pollingRates().indexOf(500);
        QMetaObject::invokeMethod(rate, "activated", Q_ARG(int, i500));
        QTRY_VERIFY(w.backend.restarting());
        QTRY_VERIFY_WITH_TIMEOUT(!w.backend.restarting(), 5000);
        QCOMPARE(rate->property("displayText").toString(), QString("500 Hz"));

        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }

    void calibrationScreenStartsAndFinishesOnLeave()
    {
        KeysWindow w;
        QVERIFY(w.open());
        QTRY_VERIFY(w.backend.ready());
        w.window->setProperty("page", 3);
        auto *stack = w.item("settingsStack");
        QVERIFY(stack);
        const auto calibrationScreen = [&] { return qvariant_cast<QQuickItem *>(stack->property("currentItem")); };

        // Settings opens the calibration screen; Back closes it and finishes.
        QMetaObject::invokeMethod(w.item("openCalibrationButton"), "clicked");
        QTRY_COMPARE(calibrationScreen()->objectName(), QString("calibrationPage"));
        auto *button = findItem(calibrationScreen(), "calibrationButton");
        QVERIFY(button);
        QMetaObject::invokeMethod(button, "clicked");
        QVERIFY(w.backend.calibrating());
        QTRY_VERIFY(w.board.writesOf(0x94, 0x00) >= 1);
        QCOMPARE(button->property("text").toString(), QString("Finish"));
        QVERIFY(!w.item("profileBox")->isEnabled());
        QMetaObject::invokeMethod(findItem(calibrationScreen(), "calibrationBack"), "clicked");
        QVERIFY(!w.backend.calibrating());
        QTRY_COMPARE(w.board.writesOf(0x94, 0x04), 1);
        QTRY_COMPARE(w.backend.calibrationSaved(), 0);
        QTRY_COMPARE(stack->property("depth").toInt(), 1);
        QVERIFY(w.item("profileBox")->isEnabled());

        // Leaving Settings from the sidebar finishes too, and Settings comes back without it.
        QMetaObject::invokeMethod(w.item("openCalibrationButton"), "clicked");
        QTRY_COMPARE(calibrationScreen()->objectName(), QString("calibrationPage"));
        QMetaObject::invokeMethod(findItem(calibrationScreen(), "calibrationButton"), "clicked");
        QVERIFY(w.backend.calibrating());
        QTRY_VERIFY(w.board.writesOf(0x94, 0x00) >= 2);
        w.window->setProperty("page", 0);
        QVERIFY(!w.backend.calibrating());
        QTRY_COMPARE(w.board.writesOf(0x94, 0x04), 2);
        QCOMPARE(stack->property("depth").toInt(), 1);
        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }

    void keysPageChangesTravelAndKeepsTheSelection()
    {
        KeysWindow w;
        QVERIFY(w.open());
        QTRY_VERIFY(w.backend.keymapReady() && w.backend.travelReady() && w.backend.advancedReady());
        w.page->setProperty("tab", 1);
        QTRY_COMPARE(w.cap(0x0d), QString("0.60"));

        // Select F12 and Esc, then set 0.42 mm: one 13 00 for both keys.
        w.click(0x0d);
        w.click(0x01, Qt::ControlModifier);
        QCOMPARE(w.view->property("selectedKeys").toList().size(), 2);
        auto *actuation = w.window->findChild<QObject *>("actuationSlider");
        QVERIFY(actuation);
        QCOMPARE(actuation->property("shown").toDouble(), 0.6);
        QMetaObject::invokeMethod(actuation, "send", Q_ARG(QVariant, 0.42));
        QTRY_COMPARE(w.cap(0x0d), QString("0.42"));
        QCOMPARE(w.board.writesOf(0x13, 0x00), 1);
        // Sending the value the keys already have sends nothing.
        QMetaObject::invokeMethod(actuation, "send", Q_ARG(QVariant, 0.42));
        QTest::qWait(100);
        QCOMPARE(w.board.writesOf(0x13, 0x00), 1);

        // Undo from the tab.
        auto *undo = w.item("undoTravel");
        QTRY_VERIFY(undo->property("enabled").toBool());
        QMetaObject::invokeMethod(undo, "clicked");
        QTRY_COMPARE(w.cap(0x0d), QString("0.60"));

        // Another tab: same selection, the drawing shows that tab's labels.
        w.page->setProperty("tab", 0);
        QCOMPARE(w.item("actionTab")->property("selection").toList().size(), 2);
        QTRY_COMPARE(w.cap(0x0d), QString("F12"));
        // All / Invert / None.
        QMetaObject::invokeMethod(w.page, "invert");
        QCOMPARE(w.view->property("selectedKeys").toList().size(), w.backend.layout().size() - 2);
        QMetaObject::invokeMethod(w.page, "select", Q_ARG(QVariant, QVariantList{}));
        QCOMPARE(w.view->property("selectedKeys").toList().size(), 0);

        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }

    void keysPageSelectsWithABox()
    {
        KeysWindow w;
        QVERIFY(w.open());
        w.window->resize(1200, 1000);
        w.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(w.window));
        QTRY_VERIFY(w.backend.keymapReady() && w.backend.travelReady() && w.backend.advancedReady());
        QTRY_COMPARE(w.cap(0x01), QString("Esc"));

        auto keyRect = [&](int id) {
            auto *k = findItem(w.view, "key_" + QByteArray::number(id));
            return k->mapRectToScene(QRectF(0, 0, k->width(), k->height()));
        };
        auto centre = [&](int id) { return keyRect(id).center().toPoint(); };
        auto selection = [&] {
            QSet<int> s;
            for (const auto &v : w.page->property("selection").toList())
                s.insert(v.toInt());
            return s;
        };
        // Every key the box from one point to another touches, from the drawn caps.
        auto touched = [&](QPoint a, QPoint b) {
            const QRectF box = QRectF(a, b).normalized();
            QSet<int> s;
            for (const auto &k : w.backend.layout()) {
                const int id = k.toMap().value("id").toInt();
                const QRectF r = keyRect(id);  // a flat box still touches (QRectF::intersects says no)
                if (r.left() < box.right() && box.left() < r.right() && r.top() < box.bottom() && box.top() < r.bottom())
                    s.insert(id);
            }
            return s;
        };
        auto drag = [&](QPoint a, QPoint b, Qt::KeyboardModifiers mods = Qt::NoModifier) {
            QTest::mousePress(w.window, Qt::LeftButton, mods, a);
            QTest::mouseMove(w.window, (a + b) / 2);
            QTest::mouseMove(w.window, b);
            QTest::mouseRelease(w.window, Qt::LeftButton, mods, b);
        };

        // Esc to F12: the whole top row, and only it.
        const QSet<int> topRow = touched(centre(0x01), centre(0x0d));
        QVERIFY(topRow.size() >= 13 && topRow.contains(0x01) && topRow.contains(0x0d));
        drag(centre(0x01), centre(0x0d));
        QCOMPARE(selection(), topRow);
        QVERIFY(!findItem(w.window->contentItem(), "selectionBox")->isVisible());

        // A plain box replaces the selection; Ctrl+drag adds to it.
        const QPoint a = centre(0x01) + QPoint(0, int(keyRect(0x01).height() * 1.8));  // a row down
        const QPoint b = a + QPoint(int(keyRect(0x01).width() * 2.5), 0);
        const QSet<int> second = touched(a, b);
        QVERIFY(!second.isEmpty() && !second.intersects(topRow));
        drag(a, b);
        QCOMPARE(selection(), second);
        drag(centre(0x01), centre(0x0d), Qt::ControlModifier);
        QCOMPARE(selection(), second | topRow);

        // A press that barely moves is still a click: only that key.
        QTest::mousePress(w.window, Qt::LeftButton, Qt::NoModifier, centre(0x0d));
        QTest::mouseMove(w.window, centre(0x0d) + QPoint(2, 1));
        QTest::mouseRelease(w.window, Qt::LeftButton, Qt::NoModifier, centre(0x0d) + QPoint(2, 1));
        QCOMPARE(selection(), QSet<int>{0x0d});
        // Ctrl+click still adds one key.
        QTest::mouseClick(w.window, Qt::LeftButton, Qt::ControlModifier, centre(0x01));
        QCOMPARE(selection(), (QSet<int>{0x0d, 0x01}));

        // A drag may start in the margin around the keyboard and selects the keys it crosses.
        const QRectF top = keyRect(0x01);
        const QPoint outside = top.center().toPoint() - QPoint(0, int(top.height() / 2) + 8);  // just above Esc
        QVERIFY(!w.view->contains(w.view->mapFromScene(outside)));
        const QPoint end = centre(0x0d);
        const QSet<int> fromMargin = touched(outside, end);
        QVERIFY(fromMargin.contains(0x01) && fromMargin.contains(0x0d));
        drag(outside, end);
        QCOMPARE(selection(), fromMargin);
        QVERIFY(!findItem(w.window->contentItem(), "selectionBox")->isVisible());
        // A plain click in the margin selects nothing, even over a selection.
        QTest::mouseClick(w.window, Qt::LeftButton, Qt::NoModifier, outside);
        QCOMPARE(selection(), fromMargin);

        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }

    void macrosPageRecordsSavesAndBinds()
    {
        KeysWindow w;
        QVERIFY(w.open());
        QTRY_VERIFY(w.backend.keymapReady() && w.backend.macrosReady());
        w.window->setProperty("page", 1);
        auto *page = w.item("macrosPage");
        QVERIFY(page);
        auto call = [&](const char *fn, QVariantList args = {}) {
            QVariant r;
            switch (args.size()) {
            case 0: QMetaObject::invokeMethod(page, fn, Q_RETURN_ARG(QVariant, r)); break;
            case 1: QMetaObject::invokeMethod(page, fn, Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, args[0])); break;
            case 3:
                QMetaObject::invokeMethod(page, fn, Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, args[0]),
                                          Q_ARG(QVariant, args[1]), Q_ARG(QVariant, args[2]));
                break;
            }
            return r;
        };
        auto need = [&](const char *name) {
            auto *i = w.item(name);
            if (!i)
                qFatal("no item %s", name);
            return i;
        };
        auto steps = [&] { return page->property("draft").toMap().value("steps").toList(); };
        auto save = need("saveMacro");
        QVERIFY(save);

        // Recording: Shift held around A, with the real delays; a release that started before
        // Record and keys with no HID usage are left out. Scan codes are XKB (evdev + 8).
        call("startRecording");
        call("recordEvent", {false, 30 + 8, 900.0});   // A released, pressed before Record
        call("recordEvent", {true, 42 + 8, 1000.0});   // Shift
        call("recordEvent", {true, 30 + 8, 1120.0});   // A
        call("recordEvent", {true, 84 + 8, 1130.0});   // no key
        call("recordEvent", {false, 30 + 8, 1200.0});
        call("recordEvent", {false, 42 + 8, 1260.0});
        call("stopRecording");
        call("recordEvent", {true, 30 + 8, 1300.0});   // after Stop
        QCOMPARE(steps().size(), 4);
        QCOMPARE(steps()[0].toMap().value("usage").toInt(), 0xe1);
        QCOMPARE(steps()[0].toMap().value("delay").toInt(), 120);
        QCOMPARE(steps()[1].toMap().value("delay").toInt(), 80);
        QCOMPARE(steps()[2].toMap().value("press").toBool(), false);

        // Save waits for a name the board takes.
        QVERIFY(!save->property("enabled").toBool());
        call("setName", {"a"});
        QVERIFY(!save->property("enabled").toBool());
        call("setName", {"ab"});
        QVERIFY(save->property("enabled").toBool());
        QMetaObject::invokeMethod(save, "clicked");
        QTRY_COMPARE(w.backend.macros().size(), 1);
        // The new macro is selected and shows the read-back: nothing left to save.
        QTRY_COMPARE(page->property("index").toInt(), 0);
        QVERIFY(!page->property("dirty").toBool());
        const auto saved = w.backend.macros()[0].toMap().value("steps").toList();
        QCOMPARE(saved[1].toMap().value("delay").toInt(), 80);
        QCOMPARE(saved[3].toMap().value("delay").toInt(), 1);
        QCOMPARE(need("macroStorageLabel")->property("text").toString(), QString("23 / 4096 bytes used"));

        // Standard delay: every step waits 50 ms; Revert puts the board's delays back.
        page->setProperty("standard", true);
        QVERIFY(page->property("dirty").toBool());
        QCOMPARE(page->property("record").toMap().value("steps").toList()[1].toMap().value("delay").toInt(), 50);
        QMetaObject::invokeMethod(need("revertMacro"), "clicked");
        QVERIFY(!page->property("dirty").toBool());
        QVERIFY(!page->property("standard").toBool());
        // A re-read (window focus) keeps an edited draft.
        call("addKey", {0x05});
        QCOMPARE(steps().size(), 6);
        w.backend.refresh();
        QTest::qWait(100);
        QCOMPARE(steps().size(), 6);
        QMetaObject::invokeMethod(need("revertMacro"), "clicked");

        // Bound to Insert, played three times: the drawing and the Keys page name it.
        w.window->setProperty("page", 0);
        w.click(0x62);
        QMetaObject::invokeMethod(need("actionTab"), "assign", Q_ARG(QVariant, w.backend.macroCode(0, "times", 3)));
        QTRY_COMPARE(w.cap(0x62), QString("ab"));
        QCOMPARE(need("currentAction")->property("text").toString(), QString("Macro: ab ×3"));
        // The picker opens on Macros with that loop mode and count.
        QTRY_COMPARE(need("macroCount")->property("value").toInt(), 3);
        QVERIFY(need("macroCount")->isVisible());

        // Deleting it unbinds the key.
        w.window->setProperty("page", 1);
        QCOMPARE(w.window->findChild<QObject *>("deleteMacroDialog")->property("bound").toInt(), 1);
        call("remove");
        QTRY_COMPARE(w.backend.macros().size(), 0);
        QTRY_COMPARE(w.backend.keymap()[0].toMap().value("98").toMap().value("code").toString(), w.backend.unboundCode());
        QCOMPARE(page->property("index").toInt(), -1);

        QVERIFY2(w.warnings.isEmpty(), qPrintable(w.warningText()));
    }
};

int runAppTests(int argc, char **argv)
{
    AppTests t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_app.moc"
