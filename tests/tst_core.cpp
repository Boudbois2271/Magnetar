// SPDX-License-Identifier: GPL-3.0-or-later
// Core library tests. No keyboard needed: the device is faked and sysfs is a temp tree.
#include "device.h"
#include "fake_transport.h"
#include "keyboard.h"

#include <QTemporaryDir>
#include <QtTest>

#include <fstream>
#include <set>
#include <sstream>

using namespace akko;
using namespace akko::test;

namespace {

Bytes parseHex(const std::string &text)
{
    Bytes out;
    std::istringstream in(text);
    std::string tok;
    while (in >> tok)
        if (tok != "-")
            out.push_back(static_cast<std::uint8_t>(std::stoul(tok, nullptr, 16)));
    return out;
}

struct FrameCase {
    std::uint8_t cmd, sub;
    Bytes payload, frame;
};

std::vector<FrameCase> loadFrames(const QString &name)
{
    std::ifstream in((QStringLiteral(FIXTURES_DIR "/") + name).toStdString());
    std::vector<FrameCase> cases;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        const auto bar = line.find('|');
        const Bytes head = parseHex(line.substr(0, bar));
        cases.push_back({head[0], head[1], Bytes(head.begin() + 2, head.end()), parseHex(line.substr(bar + 1))});
    }
    return cases;
}

// The frames under "## name" in vendor-travel.txt.
std::vector<Bytes> travelFrames(const std::string &name)
{
    std::ifstream in(FIXTURES_DIR "/vendor-travel.txt");
    std::vector<Bytes> frames;
    bool inside = false;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("## ", 0) == 0)
            inside = line.substr(3) == name;
        else if (inside && !line.empty() && line[0] != '#')
            frames.push_back(parseHex(line));
    }
    return frames;
}

// The i-th frame written, without the report ID.
Bytes sent(const FakeTransport &t, std::size_t i)
{
    return Bytes(t.written.at(i).begin() + 1, t.written.at(i).end());
}

void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void addHidraw(const QString &root, const QString &name, const QByteArray &uevent, const QByteArray &rdesc)
{
    QVERIFY(QDir().mkpath(root + "/" + name + "/device"));
    writeFile(root + "/" + name + "/device/uevent", uevent);
    writeFile(root + "/" + name + "/device/report_descriptor", rdesc);
}

const QByteArray kAkkoUevent = "DRIVER=hid-generic\nHID_ID=0003:000038EE:00000016\nHID_NAME=TAC75 HE\n";
const QByteArray kVendorRdesc = QByteArray::fromHex("0660ff0961a101");
const QByteArray kKeyboardRdesc = QByteArray::fromHex("05010906a101");

}  // namespace

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    // ---- frames ----------------------------------------------------------
    void framesMatchPythonTool()
    {
        const auto cases = loadFrames("python-frames.txt");
        QVERIFY(cases.size() >= 9);
        for (const auto &c : cases) {
            const Frame f = buildFrame(c.cmd, c.sub, c.payload);
            QCOMPARE(Bytes(f.begin(), f.end()), c.frame);
        }
    }

    void framesMatchVendorApp()
    {
        const auto cases = loadFrames("vendor-frames.txt");
        QCOMPARE(cases.size(), std::size_t(8));
        for (const auto &c : cases) {
            const Frame f = buildFrame(c.cmd, c.sub, c.payload);
            QCOMPARE(Bytes(f.begin(), f.end()), c.frame);
        }
    }

    void checksumRule()
    {
        for (const auto &c : loadFrames("python-frames.txt")) {
            unsigned sum = kReportId;
            for (auto b : c.frame)
                sum += b;
            QCOMPARE(sum & 0xFF, 0xFFu);
        }
    }

    void payloadTooLongIsRefused()
    {
        const Bytes ok(kMaxPayload, 0xAA), tooLong(kMaxPayload + 1, 0xAA);
        buildFrame(0x04, 0x01, ok);
        QVERIFY_THROWS_EXCEPTION(Error, buildFrame(0x04, 0x01, tooLong));
    }

    void replyMatching()
    {
        QVERIFY(isReplyTo(reply(0x90, 0x00), 0x90, 0x00));
        QVERIFY(!isReplyTo(reply(0x90, 0x00), 0x84, 0x01));
        Bytes wrongId = reply(0x90, 0x00);
        wrongId[0] = 0x01;
        QVERIFY(!isReplyTo(wrongId, 0x90, 0x00));
        Bytes shortReport = reply(0x90, 0x00);
        shortReport.pop_back();
        QVERIFY(!isReplyTo(shortReport, 0x90, 0x00));
    }

    // ---- transact --------------------------------------------------------
    void transactSendsReportIdAndFrame()
    {
        FakeTransport t;
        t.respond = FakeKeyboard{};
        Keyboard kb(t);
        kb.transact(0x90, 0x00);
        QCOMPARE(t.written.size(), std::size_t(1));
        QCOMPARE(t.written[0].size(), kReportLen);
        QCOMPARE(t.written[0][0], kReportId);
        const Frame f = buildFrame(0x90, 0x00);
        QVERIFY(std::equal(f.begin(), f.end(), t.written[0].begin() + 1));
        QCOMPARE(t.drains, 1);
    }

    void transactSkipsUnrelatedInput()
    {
        FakeTransport t;
        t.respond = [](const Bytes &) {
            Bytes keyEvent(kReportLen, 0);
            keyEvent[0] = 0x01;
            return std::vector<Bytes>{keyEvent, reply(0x84, 0x01), reply(0x90, 0x00, {0x02})};
        };
        Keyboard kb(t);
        QCOMPARE(kb.profile(), 2);
        QCOMPARE(t.written.size(), std::size_t(1));
    }

    void transactRetriesThenSucceeds()
    {
        FakeTransport t;
        int calls = 0;
        t.respond = [&](const Bytes &) {
            return ++calls < 3 ? std::vector<Bytes>{} : std::vector<Bytes>{reply(0x90, 0x00, {0x01})};
        };
        Keyboard kb(t);
        QCOMPARE(kb.profile(), 1);
        QCOMPARE(t.written.size(), std::size_t(3));
        QCOMPARE(t.drains, 3);  // stale input is dropped before every attempt
    }

    void transactGivesUpAfterRetries()
    {
        FakeTransport t;  // never answers
        Keyboard kb(t);
        try {
            kb.transact(0x90, 0x00);
            QFAIL("expected an error");
        } catch (const Error &e) {
            QCOMPARE(QString(e.what()), QString("no response to command 90 00"));
        }
        QCOMPARE(t.written.size(), std::size_t(3));  // first try + 2 retries, as in the Python tool
    }

    // ---- commands --------------------------------------------------------
    void profileReadAndWrite()
    {
        FakeTransport t;
        FakeKeyboard fake;
        fake.profile = 1;
        t.respond = std::ref(fake);
        Keyboard kb(t);
        QCOMPARE(kb.profile(), 1);
        kb.setProfile(2);
        QCOMPARE(fake.profile, std::uint8_t(2));
        const Frame f = buildFrame(0x10, 0x00, Bytes{0x02});
        QVERIFY(std::equal(f.begin(), f.end(), t.written.back().begin() + 1));
    }

    void profileCountAndFallbacks()
    {
        for (auto [reported, expected] : {std::pair{3, 3}, {1, 1}, {8, 8}, {0, 3}, {9, 3}}) {
            FakeTransport t;
            FakeKeyboard fake;
            fake.profileCount = static_cast<std::uint8_t>(reported);
            t.respond = std::ref(fake);
            Keyboard kb(t);
            QCOMPARE(kb.profileCount(), expected);
        }
        FakeTransport silent;
        Keyboard kb(silent);
        QCOMPARE(kb.profileCount(), 3);  // no answer to 82 0f is not an error
    }

    void lightingReadModifyWrite()
    {
        FakeTransport t;
        FakeKeyboard fake;
        t.respond = std::ref(fake);
        Keyboard kb(t);
        Lighting l = kb.lighting();
        QCOMPARE(l[Lighting::Brightness], std::uint8_t(20));
        QVERIFY(!l.rainbow());
        l[Lighting::Brightness] = 7;
        l[Lighting::ColorMode] = 7;
        kb.setLighting(l);
        QCOMPARE(fake.lighting[Lighting::Brightness], std::uint8_t(7));
        QVERIFY(kb.lighting().rainbow());
        QCOMPARE(fake.lighting[Lighting::Effect], std::uint8_t(0x02));  // untouched bytes survive
        QCOMPARE(fake.lighting[Lighting::Speed], std::uint8_t(0x04));
    }

    void effectSwitch()
    {
        FakeTransport t;
        FakeKeyboard fake;
        t.respond = std::ref(fake);
        Keyboard kb(t);
        kb.setEffect(0x13);
        const Frame f = buildFrame(0x04, 0x02, Bytes{0x13});
        QVERIFY(std::equal(f.begin(), f.end(), t.written.back().begin() + 1));
        QCOMPARE(kb.lighting()[Lighting::Effect], std::uint8_t(0x13));
    }

    void firmwareVersionReadsLowByteFirst()
    {
        FakeTransport t;
        t.respond = FakeKeyboard{};
        Keyboard kb(t);
        QCOMPARE(kb.firmwareVersion(), std::uint16_t(0x0108));
        QCOMPARE(QString::fromStdString(formatFirmware(0x0108)), QString("v1.08"));
        QCOMPARE(QString::fromStdString(formatFirmware(0x0210)), QString("v2.10"));
    }

    // ---- keymap ----------------------------------------------------------
    void layoutHasEveryKeyOnce()
    {
        QCOMPARE(layout().size(), std::size_t(kKeyCount));
        std::set<std::uint16_t> ids, read;
        for (const auto &k : layout())
            ids.insert(k.id);
        for (auto id : keyReadOrder())
            read.insert(id);
        QCOMPARE(ids.size(), std::size_t(kKeyCount));
        QVERIFY(ids == read);
        // No two keys overlap.
        for (const auto &a : layout())
            for (const auto &b : layout())
                if (a.id != b.id)
                    QVERIFY2(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + 4 <= b.y || b.y + 4 <= a.y,
                             qPrintable(QString("keys %1 and %2 overlap").arg(a.id).arg(b.id)));
    }

    void factoryKeymap()
    {
        QCOMPARE(QString::fromStdString(actionHex(factoryAction(0, 0x01))), QString("00 00 00 29"));  // Esc
        QCOMPARE(QString::fromStdString(actionHex(factoryAction(0, 0x48))), QString("0d 00 00 00"));  // Fn
        QCOMPARE(QString::fromStdString(actionHex(factoryAction(1, 0x40))), QString("02 00 00 e9"));  // Fn+. = Vol+
        QVERIFY(factoryAction(2, 0x62) == factoryAction(0, 0x62));  // Fn1 starts as Default
    }

    void actionEncodings()
    {
        // The SEEN forms of akko-protocol.md, Units.
        QCOMPARE(QString::fromStdString(actionHex(keyAction(0x29))), QString("00 00 00 29"));
        const std::uint8_t v[] = {0x19}, ab[] = {0x04, 0x05};
        QCOMPARE(QString::fromStdString(actionHex(comboAction(0x01, v))), QString("00 01 19 00"));
        QCOMPARE(QString::fromStdString(actionHex(comboAction(0x03, ab))), QString("00 03 05 04"));
        QCOMPARE(QString::fromStdString(actionHex(comboAction(0x80, {}))), QString("00 80 00 00"));
        QCOMPARE(QString::fromStdString(actionHex(mouseAction(2))), QString("01 02 01 00"));
        QCOMPARE(QString::fromStdString(actionHex(consumerAction(0xe9))), QString("02 00 00 e9"));
        QCOMPARE(QString::fromStdString(actionHex(layerAction(1))), QString("0d 01 00 00"));
        QVERIFY_THROWS_EXCEPTION(Error, comboAction(0x00, ab));

        const auto c = keyCombo(*parseAction("00 03 05 04"));
        QVERIFY(c);
        QCOMPARE(c->modifiers, std::uint8_t(0x03));
        QVERIFY((c->keys == std::vector<std::uint8_t>{0x04, 0x05}));
        QVERIFY((keyCombo(keyAction(0x29))->keys == std::vector<std::uint8_t>{0x29}));
        QVERIFY(!keyCombo(kUnbound));
        QCOMPARE(int(actionKind(kUnbound)), int(ActionKind::Unbound));
        QCOMPARE(int(actionKind(*parseAction("09 00 00 01"))), int(ActionKind::Profile));
        QVERIFY(!parseAction("00 00 00"));
        QVERIFY(!parseAction("00 00 00 zz"));
        QVERIFY(!parseAction("00 00 00 29 "));
    }

    void readLayerLikeTheVendorApp()
    {
        FakeTransport t;
        t.respond = FakeKeyboard{};
        Keyboard kb(t);
        const auto keys = kb.readLayer(0);
        QCOMPARE(keys.size(), std::size_t(kKeyCount));

        std::ifstream in(FIXTURES_DIR "/vendor-keymap-read.txt");
        std::vector<Bytes> expected;
        for (std::string line; std::getline(in, line);)
            if (!line.empty() && line[0] != '#')
                expected.push_back(parseHex(line));
        QCOMPARE(expected.size(), std::size_t(10));
        QCOMPARE(t.written.size(), expected.size());
        for (std::size_t i = 0; i < expected.size(); ++i)
            QCOMPARE(Bytes(t.written[i].begin() + 1, t.written[i].end()), expected[i]);
        for (const auto &[id, action] : keys)
            QVERIFY(action == factoryAction(0, id));
    }

    void readLayerRejectsShortAnswers()
    {
        FakeTransport t;
        FakeKeyboard kb0;
        t.respond = [&](const Bytes &w) {
            auto r = kb0(w);
            if (w[1] == 0x83 && w[5] == 9)
                r[0][7] = 0x7f;  // last packet names a key we didn't ask for
            return r;
        };
        Keyboard kb(t);
        QVERIFY_THROWS_EXCEPTION(Error, kb.readLayer(1));
        QVERIFY_THROWS_EXCEPTION(Error, kb.readLayer(3));
    }

    void setKeyWritesThenReadsBack()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const Action back = kb.setKey(2, 0x62, keyAction(0x04));
        QVERIFY(back == keyAction(0x04));
        QCOMPARE(t.written.size(), std::size_t(2));
        const auto vendor = loadFrames("vendor-frames.txt");
        QCOMPARE(Bytes(t.written[0].begin() + 1, t.written[0].end()), vendor[7].frame);  // 03 02 A on Insert
        QCOMPARE(t.written[1][1], std::uint8_t(0x83));
        QVERIFY(board.keymap[2][0x62] == keyAction(0x04));
        QVERIFY(kb.readKey(2, 0x62) == keyAction(0x04));
    }

    void setKeyFailsWhenTheBoardKeepsTheOldAction()
    {
        FakeTransport t;
        FakeKeyboard board;
        board.ignoreKeyWrites = true;
        t.respond = std::ref(board);
        Keyboard kb(t);
        QVERIFY_THROWS_EXCEPTION(Error, kb.setKey(0, 0x62, keyAction(0x04)));
    }

    // ---- per-key travel --------------------------------------------------
    void readTravelLikeTheVendorApp()
    {
        FakeTransport t;
        t.respond = FakeKeyboard{};
        Keyboard kb(t);
        const auto keys = kb.readTravel();
        QCOMPARE(keys.size(), std::size_t(kKeyCount));

        std::vector<Bytes> frames = travelFrames("read-actuation");
        for (const char *name : {"read-deadzones", "read-rt"})
            for (auto &f : travelFrames(name))
                frames.push_back(f);
        QCOMPARE(frames.size(), std::size_t(8 + 12 + 12));
        QCOMPARE(t.written.size(), frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
            QCOMPARE(sent(t, i), frames[i]);
        QCOMPARE(keys[0].key, std::uint16_t(0x01));
        QCOMPARE(keys[0].actuation, std::uint16_t(0x78));
        QCOMPARE(keys[0].deadTop, std::uint16_t(0x1a));
        QVERIFY(!keys[0].rapidTrigger && !keys[0].deadZones);
    }

    void setActuationOnAllKeysLikeTheVendorApp()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        auto keys = kb.readTravel();
        t.written.clear();
        for (auto &k : keys)
            k.actuation = 274;  // 1.37 mm
        const auto back = kb.setTravel(TravelPart::Actuation, keys);
        const auto frames = travelFrames("write-actuation-all-1.37");
        QCOMPARE(frames.size(), std::size_t(8));
        for (std::size_t i = 0; i < frames.size(); ++i)
            QCOMPARE(sent(t, i), frames[i]);
        QCOMPARE(t.written.size(), std::size_t(8 + 32));  // then reads everything back
        QVERIFY(back == keys);
        QCOMPARE(board.travel[0x65].actuation, std::uint16_t(274));
    }

    void setTravelOnF12LikeTheVendorApp()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const std::uint16_t f12[] = {0x0d};
        KeyTravel k = kb.readTravel(f12)[0];

        auto check = [&](TravelPart part, const char *fixture) {
            t.written.clear();
            const auto back = kb.setTravel(part, std::span(&k, 1));
            QCOMPARE(sent(t, 0), travelFrames(fixture)[0]);
            QCOMPARE(t.written.size(), std::size_t(4));  // the write, then 93 96 99 for F12
            QVERIFY(back[0] == k);
        };
        k.actuation = 84;  // 0.42 mm
        check(TravelPart::Actuation, "write-actuation-f12-0.42");
        QCOMPARE(sent(t, 1), travelFrames("read-actuation-f12")[0]);
        QCOMPARE(sent(t, 3), travelFrames("read-rt-f12")[0]);
        k.rapidTrigger = true;
        k.press = k.release = 1;
        check(TravelPart::RapidTrigger, "write-rt-f12-on");
        k.press = 0x22;
        k.release = 0x39;
        check(TravelPart::RapidTrigger, "write-rt-f12-0.17-0.29");
        k.rapidTrigger = false;
        check(TravelPart::RapidTrigger, "write-rt-f12-off");
        k.deadZones = true;
        check(TravelPart::DeadZones, "write-dz-f12-on");
        k.deadZones = false;
        k.deadTop = 0x25;
        k.deadBottom = 0x2e;
        check(TravelPart::DeadZones, "write-dz-f12-off");
    }

    void deadZonesGoOneKeyPerFrame()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const std::uint16_t ids[] = {0x01, 0x02, 0x0d};
        auto keys = kb.readTravel(ids);
        t.written.clear();
        for (auto &k : keys)
            k.deadZones = true;
        kb.setTravel(TravelPart::DeadZones, keys);
        for (std::size_t i = 0; i < 3; ++i) {
            QCOMPARE(t.written[i][1], std::uint8_t(0x16));
            QCOMPARE(t.written[i][4], std::uint8_t(1));  // packet count
            QCOMPARE(t.written[i][6], std::uint8_t(8));  // one entry
        }
        QVERIFY(board.travel[0x02].deadZones);
    }

    void setTravelRefusesOutOfRangeValues()
    {
        FakeTransport t;
        t.respond = FakeKeyboard{};
        Keyboard kb(t);
        KeyTravel k;
        k.key = 0x0d;
        k.actuation = 120;
        QVERIFY(travelProblem(k, TravelPart::RapidTrigger).empty());  // off with 0 / 0, as shipped
        for (auto bad : {std::uint16_t(19), std::uint16_t(701)}) {
            k.actuation = bad;
            QVERIFY_THROWS_EXCEPTION(Error, kb.setTravel(TravelPart::Actuation, std::span(&k, 1)));
        }
        k.rapidTrigger = true;  // on needs at least 0.005 mm
        QVERIFY_THROWS_EXCEPTION(Error, kb.setTravel(TravelPart::RapidTrigger, std::span(&k, 1)));
        k.press = k.release = 201;
        QVERIFY_THROWS_EXCEPTION(Error, kb.setTravel(TravelPart::RapidTrigger, std::span(&k, 1)));
        k.deadBottom = 101;
        QVERIFY_THROWS_EXCEPTION(Error, kb.setTravel(TravelPart::DeadZones, std::span(&k, 1)));
        QVERIFY(t.written.empty());
    }

    void setTravelFailsWhenTheBoardKeepsTheOldValues()
    {
        FakeTransport t;
        FakeKeyboard board;
        board.ignoreTravelWrites = true;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const std::uint16_t f12[] = {0x0d};
        KeyTravel k = kb.readTravel(f12)[0];
        k.actuation = 84;
        QVERIFY_THROWS_EXCEPTION(Error, kb.setTravel(TravelPart::Actuation, std::span(&k, 1)));
    }

    // ---- device discovery and lock ---------------------------------------
    void findHidrawPicksVendorInterface()
    {
        QTemporaryDir dir;
        const QString root = dir.path();
        addHidraw(root, "hidraw0", "HID_ID=0003:0000046D:0000C52B\n", kVendorRdesc);  // other device
        addHidraw(root, "hidraw1", kAkkoUevent, kKeyboardRdesc);                     // Akko, keys interface
        addHidraw(root, "hidraw2", kAkkoUevent, kVendorRdesc);                       // Akko, vendor interface
        QCOMPARE(QString::fromStdString(findHidraw(root.toStdString())), QString("/dev/hidraw2"));
    }

    void findHidrawUsesNameOrder()
    {
        QTemporaryDir dir;
        addHidraw(dir.path(), "hidraw2", kAkkoUevent, kVendorRdesc);
        addHidraw(dir.path(), "hidraw10", kAkkoUevent, kVendorRdesc);
        QCOMPARE(QString::fromStdString(findHidraw(dir.path().toStdString())), QString("/dev/hidraw10"));
    }

    void findHidrawReportsMissingKeyboard()
    {
        QTemporaryDir dir;
        addHidraw(dir.path(), "hidraw0", kAkkoUevent, kKeyboardRdesc);
        try {
            findHidraw(dir.path().toStdString());
            QFAIL("expected an error");
        } catch (const Error &e) {
            QCOMPARE(QString(e.what()), QString("keyboard not found (is it plugged in?)"));
        }
        QVERIFY_THROWS_EXCEPTION(Error, findHidraw((dir.path() + "/missing").toStdString()));
    }

    void lockIsExclusive()
    {
        QTemporaryDir dir;
        const std::string path = (dir.path() + "/akkoctl.lock").toStdString();
        {
            SessionLock first(path, 0);
            try {
                SessionLock second(path, 100);
                QFAIL("second lock should not be granted");
            } catch (const Error &e) {
                QCOMPARE(QString(e.what()), QString("keyboard busy (another akkoctl is running)"));
            }
        }
        SessionLock again(path, 0);  // released when the first one went away
    }
};

int runCoreTests(int argc, char **argv)
{
    CoreTests t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_core.moc"
