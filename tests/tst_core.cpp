// SPDX-License-Identifier: GPL-3.0-or-later
// Core library tests. No keyboard needed: the device is faked and sysfs is a temp tree.
#include "device.h"
#include "fake_transport.h"
#include "keyboard.h"

#include <QTemporaryDir>
#include <QtTest>

#include <fstream>
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
        QCOMPARE(cases.size(), std::size_t(5));
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
