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
};

int runAppTests(int argc, char **argv)
{
    AppTests t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_app.moc"
