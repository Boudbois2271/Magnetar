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

// The frames under "## name" in vendor-advanced.txt; only the 12 00 writes when writesOnly.
std::vector<Bytes> advancedFrames(const std::string &name, bool writesOnly = false)
{
    std::ifstream in(FIXTURES_DIR "/vendor-advanced.txt");
    std::vector<Bytes> frames;
    bool inside = false;
    for (std::string line; std::getline(in, line);) {
        if (line.rfind("## ", 0) == 0)
            inside = line.substr(3) == name;
        else if (inside && !line.empty() && line[0] != '#') {
            Bytes f = parseHex(line);
            if (!writesOnly || f[0] == 0x12)
                frames.push_back(f);
        }
    }
    return frames;
}

// The record a captured 12 00 frame carries.
AdvancedKey advancedOf(const Bytes &frame)
{
    const auto a = decodeAdvanced(frame[2], std::span(frame).subspan(6, frame[5]));
    if (!a)
        throw Error("fixture frame does not decode");
    return *a;
}

// The i-th frame written, without the report ID.
std::vector<Bytes> colorFrames(const std::string &name)
{
    std::ifstream in(FIXTURES_DIR "/vendor-colors.txt");
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

    // ---- per-key colours ------------------------------------------------
    void readColorsLikeTheVendorApp()
    {
        // Replays the board's own answers from akko-capture2.txt.
        const auto answers = colorFrames("read-all-answers");
        QCOMPARE(answers.size(), std::size_t(8));
        FakeTransport t;
        std::size_t n = 0;
        t.respond = [&](const Bytes &) {
            Bytes r{0x09};
            r.insert(r.end(), answers[n].begin(), answers[n].end());
            ++n;
            return std::vector<Bytes>{r};
        };
        Keyboard kb(t);
        const auto colors = kb.readColors();
        const auto frames = colorFrames("read-all");
        QCOMPARE(t.written.size(), frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
            QCOMPARE(sent(t, i), frames[i]);
        QCOMPARE(colors.size(), std::size_t(kKeyCount));
        const std::array<std::uint8_t, 3> blue{0x00, 0x00, 0xff}, black{};
        QCOMPARE(colors[0].key, std::uint16_t(0x01));
        QVERIFY(colors[0].rgb == blue);
        QVERIFY(colors[1].rgb == black);
        const auto last = colors.back();
        QCOMPARE(last.key, std::uint16_t(0x65));
        QVERIFY(last.rgb == black);
    }

    void setColorsOnTheFRowLikeTheVendorApp()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const std::array<std::uint8_t, 3> red{0xff, 0x00, 0x00};
        // The vendor app's batches as the selection grew: Esc, F1-F4, F5-F12, then two more keys.
        const std::vector<std::vector<std::uint16_t>> batches{
            {0x01}, {0x02, 0x03, 0x04, 0x05}, {0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d}, {0x63}, {0x62}};
        for (const auto &ids : batches) {
            std::vector<KeyColor> keys;
            for (auto id : ids)
                keys.push_back({id, red});
            QVERIFY(kb.setColors(keys) == keys);
        }
        const auto frames = colorFrames("f-row");
        QCOMPARE(t.written.size(), frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
            QCOMPARE(sent(t, i), frames[i]);
        QVERIFY(board.colors[0x0d] == red);
    }

    void setColorsSplitsIntoFramesOfEightKeys()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        std::vector<KeyColor> keys;
        for (auto id : keyReadOrder())
            keys.push_back({id, {0x12, 0x34, 0x56}});
        QVERIFY(kb.setColors(keys) == keys);
        QCOMPARE(t.written.size(), std::size_t(2 * 11));  // 11 frames of up to 8 keys, each read back
        for (std::size_t i = 0; i < t.written.size(); i += 2) {
            QCOMPARE(t.written[i][1], std::uint8_t(0x06));
            QCOMPARE(t.written[i][4], std::uint8_t(1));  // a single packet
            QVERIFY(t.written[i][6] <= 8 * 5);
            QCOMPARE(t.written[i + 1][1], std::uint8_t(0x86));
        }
        QVERIFY(board.colors[0x65] == (std::array<std::uint8_t, 3>{0x12, 0x34, 0x56}));
    }

    void setColorsFailsWhenTheBoardKeepsTheOldColour()
    {
        FakeTransport t;
        FakeKeyboard board;
        board.ignoreColorWrites = true;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const KeyColor k{0x0d, {0xff, 0x00, 0x00}};
        QVERIFY_THROWS_EXCEPTION(Error, kb.setColors(std::span(&k, 1)));
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

    // ---- advanced keys ----------------------------------------------------
    void advancedFramesMatchVendorApp()
    {
        int checked = 0;
        for (const char *name : {"mpt-a", "mpt-abc", "socd-ad-last", "socd-qe-key1", "socd-zc-key2", "socd-fg-neutral",
                                 "dks-space", "mt-p", "tgl-j", "end-4", "dks-insert-hold-1", "dks-insert-hold-all",
                                 "dks-insert-b-c"})
            for (const auto &f : advancedFrames(name, true)) {
                if (f[2] == 0x00)
                    continue;  // clears are checked below
                const AdvancedKey a = advancedOf(f);
                QVERIFY2(advancedProblem(a).empty(), name);
                const auto payload = encodeAdvanced(a);
                const Frame built = buildFrame(0x12, 0x00, payload, 1, 0, f[2]);
                QCOMPARE(Bytes(built.begin(), built.end()), f);
                ++checked;
            }
        QCOMPARE(checked, 13);

        const AdvancedKey mt = advancedOf(advancedFrames("mt-p", true)[0]);
        QCOMPARE(mt.type, AdvancedType::ModTap);
        QCOMPARE(mt.key, std::uint16_t(0x26));  // P
        QVERIFY(mt.action == keyAction(0x15) && mt.tap == keyAction(0x17));  // hold R, tap T
        QCOMPARE(mt.holdMs, std::uint16_t(322));

        const AdvancedKey tgl = advancedOf(advancedFrames("tgl-j", true)[0]);
        QVERIFY(tgl.type == AdvancedType::Toggle && tgl.key == 0x31 && tgl.action == keyAction(0x0e));
        QCOMPARE(tgl.holdMs, std::uint16_t(965));

        const AdvancedKey end = advancedOf(advancedFrames("end-4", true)[0]);
        QVERIFY(end.type == AdvancedType::Release && end.key == 0x12 && end.action == keyAction(0x22));

        const AdvancedKey socd = advancedOf(advancedFrames("socd-zc-key2", true)[0]);
        QVERIFY(socd.key == 0x38 && socd.key2 == 0x3a && socd.mode == SocdMode::Key2);
        QVERIFY(advancedKeyIds(socd) == std::vector<std::uint16_t>({0x38, 0x3a}));

        const AdvancedKey mpt = advancedOf(advancedFrames("mpt-abc", true)[1]);
        QCOMPARE(mpt.points.size(), kMultiPointSlots);
        QVERIFY(mpt.points[0] == (PointSlot{keyAction(0x04), 0x7f}));   // A at 0.635 mm
        QVERIFY(mpt.points[2] == (PointSlot{keyAction(0x06), 0x27f}));  // C at 3.195 mm

        const AdvancedKey dks = advancedOf(advancedFrames("dks-insert-b-c", true)[0]);
        QVERIFY(dks.depths == (std::array<std::uint16_t, 4>{200, 600, 600, 200}));
        QVERIFY(dksStroke(dks.rows[0].stages) == (DksStroke{1, 2}));  // B held bottom → release from bottom
        QVERIFY(dksStroke(dks.rows[1].stages) == (DksStroke{3, 3}));  // C once at full release
        QVERIFY(dksStroke(dks.rows[2].stages) == DksStroke{});
    }

    void dksStrokes()
    {
        using S = std::array<std::uint8_t, 4>;
        QVERIFY(dksStages({0, 1}) == (S{0x0e, 0, 0, 0}));
        QVERIFY(dksStages({0, 3}) == (S{0x06, 0x04, 0x0c, 0}));
        QVERIFY(dksStages({1, 2}) == (S{0, 0x0e, 0, 0}));
        QVERIFY(dksStages({2, 2}) == (S{0, 0, 0x0a, 0}));
        QVERIFY(dksStages({}) == S{});
        QVERIFY(dksStages({2, 1}) == S{});  // backwards: nothing
        for (int from = 0; from < 4; ++from)
            for (int to = from; to < 4; ++to)
                QVERIFY(dksStroke(dksStages({from, to})) == (DksStroke{from, to}));
        QVERIFY(!dksStroke(S{0x02, 0, 0, 0}).has_value());     // never captured: not ours to name
        QVERIFY(!dksStroke(S{0x0a, 0, 0x0a, 0}).has_value());
    }

    void decodeRejectsShortPayloads()
    {
        const Bytes mt{0x00, 0x26, 0, 0, 0, 0x15};
        QVERIFY(!decodeAdvanced(0x02, mt).has_value());
        QVERIFY(!decodeAdvanced(0x07, Bytes{0x00, 0x26}).has_value());  // unknown type
        const Bytes mpt{0x00, 0x38, 0x03, 0, 0, 0, 0x04, 0x00, 0x7f};  // says 3, holds 1
        QVERIFY(!decodeAdvanced(0x05, mpt).has_value());
    }

    void setAdvancedLikeTheVendorApp()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const auto frames = advancedFrames("mt-p");
        const AdvancedKey mt = advancedOf(frames[0]);
        const auto after = kb.setAdvanced(mt);
        // First a look at what the board has (an empty list), then the write and the vendor's
        // read-back: the list, then the key.
        QCOMPARE(t.written.size(), std::size_t(1 + 3));
        QCOMPARE(sent(t, 0), frames[1]);
        for (std::size_t i = 0; i < frames.size(); ++i)
            QCOMPARE(sent(t, 1 + i), frames[i]);
        QCOMPARE(after.size(), std::size_t(1));
        QVERIFY(after[0] == mt);
        QVERIFY(kb.readAdvancedKeys() == after);
    }

    void savingOverAnAdvancedKeyClearsItFirst()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        kb.setAdvanced(advancedOf(advancedFrames("mpt-a", true)[0]));
        t.written.clear();
        const auto writes = advancedFrames("mpt-abc", true);  // clear, then the new record
        kb.setAdvanced(advancedOf(writes[1]));
        std::vector<Bytes> ours;
        for (std::size_t i = 0; i < t.written.size(); ++i)
            if (sent(t, i)[0] == 0x12)
                ours.push_back(sent(t, i));
        QVERIFY(ours == writes);
        QCOMPARE(board.advanced.at(0x38).second, encodeAdvanced(advancedOf(writes[1])));
    }

    void socdClearsBothKeys()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        const auto after = kb.setAdvanced(advancedOf(advancedFrames("socd-ad-last", true)[0]));
        QCOMPARE(after.size(), std::size_t(1));  // one record for the pair
        QCOMPARE(board.advanced.size(), std::size_t(2));
        t.written.clear();
        QVERIFY(kb.clearAdvanced(0x2d).empty());  // either key removes the pair
        std::vector<Bytes> ours;
        for (std::size_t i = 0; i < t.written.size(); ++i)
            if (sent(t, i)[0] == 0x12)
                ours.push_back(sent(t, i));
        QVERIFY(ours == advancedFrames("socd-ad-restore", true));
        QVERIFY(board.advanced.empty());

        // A new record on one key of a pair takes the whole pair away first.
        kb.setAdvanced(advancedOf(advancedFrames("socd-ad-last", true)[0]));
        AdvancedKey end = makeAdvanced(AdvancedType::Release, 0x2d);
        end.action = keyAction(0x22);
        const auto now = kb.setAdvanced(end);
        QCOMPARE(now.size(), std::size_t(1));
        QVERIFY(now[0] == end);
        QVERIFY(!board.advanced.contains(0x2b));
    }

    void setAdvancedOnSeveralKeys()
    {
        FakeTransport t;
        FakeKeyboard board;
        t.respond = std::ref(board);
        Keyboard kb(t);
        // P already has a Release: it is cleared before the batch is written.
        AdvancedKey end = makeAdvanced(AdvancedType::Release, 0x26);
        end.action = keyAction(0x22);
        kb.setAdvanced(end);
        t.written.clear();

        std::vector<AdvancedKey> batch;
        for (std::uint16_t key : {0x26, 0x31, 0x38}) {
            AdvancedKey mt = makeAdvanced(AdvancedType::ModTap, key);
            mt.action = keyAction(0x15);
            mt.tap = keyAction(0x17);
            mt.holdMs = 322;
            batch.push_back(mt);
        }
        const auto after = kb.setAdvanced(batch);
        QCOMPARE(after.size(), std::size_t(3));
        for (const auto &a : batch)
            QVERIFY(std::find(after.begin(), after.end(), a) != after.end());
        std::vector<Bytes> ours;
        for (std::size_t i = 0; i < t.written.size(); ++i)
            if (sent(t, i)[0] == 0x12)
                ours.push_back(sent(t, i));
        QCOMPARE(ours.size(), std::size_t(1 + 3));  // P's clear, then the three records
        QCOMPARE(ours[0][2], std::uint8_t(0x00));
        // The list is read once before and once after, not per key.
        std::size_t lists = 0;
        for (std::size_t i = 0; i < t.written.size(); ++i)
            if (sent(t, i)[0] == 0x92 && sent(t, i)[5] == 0)
                ++lists;
        QCOMPARE(lists, std::size_t(2));

        const std::uint16_t keys[] = {0x26, 0x38};
        const auto left = kb.clearAdvanced(keys);
        QCOMPARE(left.size(), std::size_t(1));
        QCOMPARE(left[0].key, std::uint16_t(0x31));

        // Two records on one key: refused before anything is sent.
        t.written.clear();
        batch.resize(2);
        batch[1].key = batch[0].key;
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(batch));
        QVERIFY(t.written.empty());
    }

    void setAdvancedRefusesBadRecords()
    {
        FakeTransport t;
        t.respond = FakeKeyboard{};
        Keyboard kb(t);
        AdvancedKey tgl = makeAdvanced(AdvancedType::Toggle, 0x31);
        tgl.action = keyAction(0x0e);
        tgl.holdMs = 50;
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(tgl));
        tgl.holdMs = 965;
        tgl.key = 0x50;  // not a key on this board
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(tgl));

        AdvancedKey dks = advancedOf(advancedFrames("dks-space", true)[0]);
        std::swap(dks.depths[0], dks.depths[1]);
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(dks));
        dks = advancedOf(advancedFrames("dks-space", true)[0]);
        dks.rows[0].action = kNoAction;  // stages without an action
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(dks));

        AdvancedKey socd = advancedOf(advancedFrames("socd-fg-neutral", true)[0]);
        socd.key2 = socd.key;
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(socd));
        AdvancedKey mpt = advancedOf(advancedFrames("mpt-a", true)[0]);
        mpt.points.pop_back();
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(mpt));
        QVERIFY(t.written.empty());
    }

    void setAdvancedFailsWhenTheBoardKeepsTheOldRecord()
    {
        FakeTransport t;
        FakeKeyboard board;
        board.ignoreAdvancedWrites = true;
        t.respond = std::ref(board);
        Keyboard kb(t);
        QVERIFY_THROWS_EXCEPTION(Error, kb.setAdvanced(advancedOf(advancedFrames("end-4", true)[0])));
    }

    void advancedListFollowsExtraPackets()
    {
        // The oldest capture answered the list read with 4 packets; we ask for the rest and
        // skip what isn't a key.
        FakeTransport t;
        t.respond = [](const Bytes &w) -> std::vector<Bytes> {
            Bytes r(kReportLen, 0);
            std::copy_n(w.begin(), 6, r.begin());
            if (w[6] != 0) {
                r[6] = 2;
                r[7] = w[7];
                r[8] = w[8];  // no advanced key
                return {r};
            }
            r[4] = 0x02;
            r[6] = 6;
            const std::uint8_t first[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x50}, second[] = {0x00, 0x0e, 0x00, 0x01, 0x00, 0x00};
            std::copy_n(w[5] == 0 ? first : second, 6, r.begin() + 7);
            return {r};
        };
        Keyboard kb(t);
        QVERIFY(kb.advancedKeyList() == std::vector<std::uint16_t>({0x01, 0x0e}));
        QCOMPARE(t.written.size(), std::size_t(2));
        QCOMPARE(sent(t, 1)[3], std::uint8_t(0x02));
        QCOMPARE(sent(t, 1)[4], std::uint8_t(0x01));
        QVERIFY(kb.readAdvancedKeys().empty());
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
