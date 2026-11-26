// SPDX-License-Identifier: GPL-3.0-or-later
// CLI tests. The main check replays fixtures/python-cli.jsonl: each line is one run of the
// Python akkoctl against a pretend keyboard, with what it printed, the commands it sent and
// what it left behind. The C++ CLI must do exactly the same.
#include "cli.h"
#include "fake_transport.h"
#include "lights.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace akko;
using namespace akko::test;

namespace {

QString hex(const Bytes &b)
{
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char *>(b.data()), qsizetype(b.size())).toHex());
}

// "cc ss" plus payload bytes, the format the fixture uses for each command sent.
QString describeSent(const Bytes &report)
{
    QStringList parts;
    const std::size_t len = report[6];
    for (std::size_t i = 1; i <= 2; ++i)
        parts << QString::asprintf("%02x", report[i]);
    for (std::size_t i = 0; i < len; ++i)
        parts << QString::asprintf("%02x", report[7 + i]);
    return parts.join(' ');
}

QString readAll(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

// One akkoctl run against `fake`: what it printed (the error line on failure) and what it sent.
struct Run {
    QString out;
    QStringList sent;
    bool failed = false;
};

Run runOn(FakeKeyboard &fake, const std::vector<std::string> &args)
{
    const cli::Parsed p = cli::parseArgs(args);
    if (!p.command)
        return {QString::fromStdString(p.message), {}, true};
    FakeTransport t;
    t.respond = std::ref(fake);
    Keyboard kb(t);
    QTemporaryDir dir;
    Run r;
    try {
        r.out = QString::fromStdString(cli::execute(kb, *p.command, (dir.path() + "/state.json").toStdString()));
    } catch (const Error &e) {
        r.out = QString::fromStdString(cli::errorJson(e.what()));
        r.failed = true;
    }
    for (const auto &w : t.written)
        r.sent << describeSent(w);
    return r;
}

}  // namespace

class CliTests : public QObject {
    Q_OBJECT

private slots:
    void matchesPythonTool_data()
    {
        QTest::addColumn<QJsonObject>("scenario");
        QFile f(QStringLiteral(FIXTURES_DIR "/python-cli.jsonl"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        int n = 0;
        while (!f.atEnd()) {
            const QByteArray line = f.readLine().trimmed();
            if (line.isEmpty())
                continue;
            const QJsonObject sc = QJsonDocument::fromJson(line).object();
            QStringList args;
            for (const auto &a : sc["args"].toArray())
                args << a.toString();
            const QString name = QString("%1: akkoctl %2").arg(++n).arg(args.join(' '));
            QTest::newRow(qPrintable(name)) << sc;
        }
        QVERIFY(n >= 30);
    }

    void matchesPythonTool()
    {
        QFETCH(QJsonObject, scenario);
        const QJsonObject before = scenario["before"].toObject();
        const QJsonObject after = scenario["after"].toObject();

        FakeKeyboard fake;
        fake.profile = std::uint8_t(before["profile"].toInt());
        fake.profileCount = std::uint8_t(before["count"].toInt());
        fake.silentProfileCount = before["silentCount"].toBool();
        const QByteArray light = QByteArray::fromHex(before["lighting"].toString().toLatin1());
        std::copy(light.begin(), light.end(), fake.lighting.begin());

        QTemporaryDir dir;
        const QString stateFile = dir.path() + "/akkoctl/state.json";
        if (!before["state"].isNull()) {
            QVERIFY(QDir().mkpath(dir.path() + "/akkoctl"));
            QFile s(stateFile);
            QVERIFY(s.open(QIODevice::WriteOnly));
            s.write(before["state"].toString().toUtf8());
        }

        std::vector<std::string> args;
        for (const auto &a : scenario["args"].toArray())
            args.push_back(a.toString().toStdString());
        const cli::Parsed parsed = cli::parseArgs(args);
        QVERIFY(parsed.command.has_value());

        FakeTransport t;
        t.respond = std::ref(fake);
        Keyboard kb(t);
        std::string out;
        int code = 0;
        try {
            out = cli::execute(kb, *parsed.command, stateFile.toStdString());
        } catch (const Error &e) {
            out = cli::errorJson(e.what());
            code = 1;
        }

        QCOMPARE(QString::fromStdString(out), scenario["stdout"].toString());
        QCOMPARE(code, scenario["exit"].toInt());

        // The writes go out in the same order with the same bytes. Reads that went
        // unanswered (82 0f on a silent keyboard) are retried, so compare distinct steps.
        QStringList sent;
        for (const auto &w : t.written) {
            const QString d = describeSent(w);
            if (sent.isEmpty() || sent.last() != d || d != "82 0f")
                sent << d;
        }
        QStringList expected;
        for (const auto &s : scenario["sent"].toArray())
            expected << s.toString();
        QCOMPARE(sent, expected);

        QCOMPARE(int(fake.profile), after["profile"].toInt());
        QCOMPARE(hex(Bytes(fake.lighting.begin(), fake.lighting.end())), after["lighting"].toString());
        if (after["state"].isNull())
            QVERIFY(!QFile::exists(stateFile));
        else
            QCOMPARE(readAll(stateFile), after["state"].toString());
    }

    void usageErrors_data()
    {
        QTest::addColumn<QStringList>("args");
        QTest::addColumn<int>("exitCode");
        QTest::newRow("help") << QStringList{"--help"} << 0;
        QTest::newRow("help after command") << QStringList{"profile", "-h"} << 0;
        QTest::newRow("unknown command") << QStringList{"blink"} << 2;
        QTest::newRow("missing number") << QStringList{"profile"} << 2;
        QTest::newRow("missing colour") << QStringList{"color"} << 2;
        QTest::newRow("not a number") << QStringList{"brightness", "ten"} << 2;
        QTest::newRow("extra argument") << QStringList{"on", "now"} << 2;
        QTest::newRow("extra number") << QStringList{"profile", "1", "2"} << 2;
        QTest::newRow("keymap without layer") << QStringList{"keymap"} << 2;
        QTest::newRow("no such layer") << QStringList{"keymap", "fn2"} << 2;
        QTest::newRow("keymap bad verb") << QStringList{"keymap", "fn", "clear"} << 2;
        QTest::newRow("fn1 reset never captured") << QStringList{"keymap", "fn1", "reset"} << 2;
        QTest::newRow("key without key") << QStringList{"key", "default"} << 2;
        QTest::newRow("no such key") << QStringList{"key", "default", "68"} << 2;
        QTest::newRow("key not hex") << QStringList{"key", "default", "caps"} << 2;
        QTest::newRow("action too short") << QStringList{"key", "fn", "2a", "00 00 29"} << 2;
        QTest::newRow("action badly spaced") << QStringList{"key", "fn", "2a", "0000 0029"} << 2;
        // Forms never captured being written are refused before anything is sent.
        QTest::newRow("system key") << QStringList{"key", "fn", "2a", "07 00 00 04"} << 2;
        QTest::newRow("profile key") << QStringList{"key", "fn", "2a", "09 00 00 01"} << 2;
        QTest::newRow("select effect") << QStringList{"key", "fn", "2a", "08 09 01 00"} << 2;
        QTest::newRow("usage in byte 2 alone") << QStringList{"key", "fn", "2a", "00 00 04 00"} << 2;
        QTest::newRow("empty key") << QStringList{"key", "fn", "2a", "00000000"} << 2;
        QTest::newRow("mouse 6") << QStringList{"key", "fn", "2a", "01 06 01 00"} << 2;
        QTest::newRow("macro loop 3") << QStringList{"key", "fn", "2a", "03 03 00 00"} << 2;
        QTest::newRow("Fn2 key") << QStringList{"key", "fn", "2a", "0d 02 00 00"} << 2;
    }

    void usageErrors()
    {
        QFETCH(QStringList, args);
        QFETCH(int, exitCode);
        std::vector<std::string> a;
        for (const auto &s : args)
            a.push_back(s.toStdString());
        const cli::Parsed p = cli::parseArgs(a);
        QVERIFY(!p.command.has_value());
        QCOMPARE(p.exitCode, exitCode);
        QVERIFY(QString::fromStdString(p.message).startsWith("usage: akkoctl"));
    }

    void keyReadsAndSets()
    {
        FakeKeyboard fake;
        fake.profile = 1;

        // Read: profile, then the one key.
        Run r = runOn(fake, {"key", "default", "2a"});
        QCOMPARE(r.out, QString("{\"connected\": true, \"profile\": 2, \"layer\": \"default\", \"key\": \"2a\", "
                                "\"action\": \"00 00 00 39\", \"factory\": \"00 00 00 39\"}\n"));
        QCOMPARE(r.sent, (QStringList{"90 00", "83 00 00 2a"}));

        // Caps → Ctrl on Fn1, given as four arguments and with 0x: read, write, read back.
        r = runOn(fake, {"key", "FN1", "0x2A", "00", "01", "00", "00"});
        QVERIFY(!r.failed);
        QVERIFY(r.out.contains("\"layer\": \"fn1\", \"key\": \"2a\", \"action\": \"00 01 00 00\", "
                               "\"factory\": \"00 00 00 39\"}"));
        QCOMPARE(r.sent, (QStringList{"90 00", "83 02 00 2a", "03 02 00 2a 00 01 00 00", "83 02 00 2a"}));
        QCOMPARE(fake.keymap[2][0x2a], (Action{0x00, 0x01, 0x00, 0x00}));

        // The same action again: read only, nothing written.
        r = runOn(fake, {"key", "fn1", "2a", "00010000"});
        QCOMPARE(r.sent, (QStringList{"90 00", "83 02 00 2a"}));

        // Factory puts back the Default copy on Fn1.
        r = runOn(fake, {"key", "fn1", "2a", "factory"});
        QCOMPARE(r.sent, (QStringList{"90 00", "83 02 00 2a", "03 02 00 2a 00 00 00 39", "83 02 00 2a"}));
        QCOMPARE(fake.keymap[2][0x2a], factoryAction(2, 0x2a));

        // Factory may write a GUESS form the board shipped with (Fn+` = 07 00 00 04).
        fake.keymap[1][0x0e] = kUnbound;
        r = runOn(fake, {"key", "fn", "0e", "factory"});
        QCOMPARE(r.sent.value(2), QString("03 01 00 0e 07 00 00 04"));
        QCOMPARE(fake.keymap[1][0x0e], (Action{0x07, 0x00, 0x00, 0x04}));

        r = runOn(fake, {"key", "default", "62", "unbound"});
        QCOMPARE(r.sent.value(2), QString("03 00 00 62 ff ff ff ff"));

        // A read-back that differs is an error line.
        fake.ignoreKeyWrites = true;
        r = runOn(fake, {"key", "default", "62", "00 00 00 4a"});
        QVERIFY(r.failed);
        QCOMPARE(r.out, QString("{\"connected\": false, \"error\": \"the keyboard kept ff ff ff ff instead of "
                                "00 00 00 4a\"}\n"));
    }

    void keyChecksTheMacroExists()
    {
        FakeKeyboard fake;  // no macros
        const Run r = runOn(fake, {"key", "default", "62", "03 01 01 00"});
        QVERIFY(r.failed);
        QVERIFY(r.out.contains("no macro 1 on this profile (it has 0)"));
        for (const auto &s : r.sent)
            QVERIFY(!s.startsWith("03 "));
    }

    void keymapReadsAndResets()
    {
        FakeKeyboard fake;
        fake.keymap[0][0x62] = kUnbound;
        Run r = runOn(fake, {"keymap", "default"});
        QVERIFY(!r.failed);
        QVERIFY(r.out.startsWith("{\"connected\": true, \"profile\": 1, \"layer\": \"default\", \"keys\": [{\"key\": "
                                 "\"01\", \"action\": \"00 00 00 29\", \"factory\": \"00 00 00 29\"}, {\"key\": \"02\""));
        QVERIFY(r.out.contains("{\"key\": \"62\", \"action\": \"ff ff ff ff\", \"factory\": \"00 00 00 49\"}"));
        QVERIFY(r.out.endsWith("{\"key\": \"4d\", \"action\": \"00 00 00 4f\", \"factory\": \"00 00 00 4f\"}]}\n"));
        QCOMPARE(r.out.count("\"key\""), kKeyCount);
        QCOMPARE(QJsonDocument::fromJson(r.out.toUtf8())["keys"].toArray().size(), kKeyCount);
        QCOMPARE(r.sent.size(), 11);  // 90 00, then 10 packets of 83 00
        for (const auto &s : r.sent)
            QVERIFY(!s.startsWith("03 "));

        // Reset: 11 00 01, then the whole layer read again.
        r = runOn(fake, {"keymap", "default", "reset"});
        QCOMPARE(r.sent.value(1), QString("11 00 01"));
        QCOMPARE(r.sent.size(), 12);
        QVERIFY(r.out.contains("{\"key\": \"62\", \"action\": \"00 00 00 49\""));
        QCOMPARE(fake.keymap[0][0x62], factoryAction(0, 0x62));
    }

    void numbersParseLikePython()
    {
        const auto number = [](const char *text) {
            const auto p = cli::parseArgs({"brightness", text});
            return p.command ? p.command->number : -999;
        };
        QCOMPARE(number("7"), 7);
        QCOMPARE(number(" 7 "), 7);
        QCOMPARE(number("+7"), 7);
        QCOMPARE(number("-3"), -3);
        QCOMPARE(number("007"), 7);
        QCOMPARE(number("99999999999999999999"), INT_MAX);
        QCOMPARE(number("7.5"), -999);
        QCOMPARE(number(""), -999);
    }

    void badHexIsAnErrorNotACrash()
    {
        // The Python tool crashed with a traceback here; the C++ one reports it like any bad colour.
        FakeTransport t;
        t.respond = FakeKeyboard{};
        Keyboard kb(t);
        QTemporaryDir dir;
        const auto p = cli::parseArgs({"color", "zzzzzz"});
        try {
            cli::execute(kb, *p.command, (dir.path() + "/state.json").toStdString());
            QFAIL("expected an error");
        } catch (const Error &e) {
            QCOMPARE(QString(e.what()), QString("bad colour 'zzzzzz', expected #rrggbb"));
        }
    }

    void jsonStringsLikePython()
    {
        // Expected values are what Python's json.dumps prints for the same text.
        QCOMPARE(QString::fromStdString(cli::jsonString("plain")), QString("\"plain\""));
        QCOMPARE(QString::fromStdString(cli::jsonString("a\"b\\c\nd\te")), QString("\"a\\\"b\\\\c\\nd\\te\""));
        QCOMPARE(QString::fromStdString(cli::jsonString("\x01")), QString("\"\\u0001\""));
        QCOMPARE(QString::fromStdString(cli::jsonString("couleur \xc3\xa9")), QString("\"couleur \\u00e9\""));
        QCOMPARE(QString::fromStdString(cli::jsonString("\xf0\x9f\x8e\xb9")), QString("\"\\ud83c\\udfb9\""));
        QCOMPARE(QString::fromStdString(cli::errorJson("keyboard busy (another akkoctl is running)")),
                 QString("{\"connected\": false, \"error\": \"keyboard busy (another akkoctl is running)\"}\n"));
    }

    void logLineFormat()
    {
        QTemporaryDir dir;
        const std::string log = (dir.path() + "/akkoctl/akkoctl.log").toStdString();
        cli::logCall(log, {}, "ok", 42.4);
        cli::logCall(log, {"profile", "2"}, "error: profile must be 1..3", 7);
        const QStringList lines = readAll(QString::fromStdString(log)).split('\n', Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 2);
        const QRegularExpression re("^\\d\\d:\\d\\d:\\d\\d\\.\\d{3} pid=\\d+ ppid=\\d+ (.*)$");
        auto m = re.match(lines[0]);
        QVERIFY(m.hasMatch());
        QCOMPARE(m.captured(1), QString("status -> ok (42 ms)"));
        m = re.match(lines[1]);
        QVERIFY(m.hasMatch());
        QCOMPARE(m.captured(1), QString("profile 2 -> error: profile must be 1..3 (7 ms)"));
    }

    void logRotatesPast256KiB()
    {
        QTemporaryDir dir;
        const QString log = dir.path() + "/akkoctl.log";
        {
            QFile f(log);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QByteArray(256 * 1024 + 1, 'x'));
        }
        cli::logCall(log.toStdString(), {"status"}, "ok", 1);
        QCOMPARE(QFileInfo(log + ".1").size(), qint64(256 * 1024 + 1));
        QVERIFY(QFileInfo(log).size() < 200);
    }

    void stateFileFollowsXdg()
    {
        qputenv("XDG_STATE_HOME", "/somewhere/state");
        QCOMPARE(QString::fromStdString(cli::defaultStateFile()), QString("/somewhere/state/akkoctl/state.json"));
        qunsetenv("XDG_STATE_HOME");
        QCOMPARE(QString::fromStdString(cli::defaultStateFile()),
                 QString(qgetenv("HOME")) + "/.local/state/akkoctl/state.json");
        QCOMPARE(QString::fromStdString(cli::logFileFor("/a/akkoctl/state.json")), QString("/a/akkoctl/akkoctl.log"));
    }

    // Saving the brightness keeps every other key, as the Python tool's json.dump did; a file that
    // isn't a JSON object starts over from {}.
    void stateKeepsOtherKeys_data()
    {
        QTest::addColumn<QString>("before");
        QTest::addColumn<QString>("after");
        QTest::newRow("none") << QString() << "{\"lastBrightness\": 7}";
        QTest::newRow("only") << "{\"lastBrightness\": 3}" << "{\"lastBrightness\": 7}";
        QTest::newRow("kept in place")
            << "{ \"a\" : [1, {\"b\": null}],\n \"lastBrightness\":3, \"s\": \"x\\\"y\\u00e9\", \"n\": -1.5e3 }\n"
            << "{\"a\": [1, {\"b\": null}], \"lastBrightness\": 7, \"s\": \"x\\\"y\\u00e9\", \"n\": -1.5e3}";
        QTest::newRow("appended") << "{\"theme\": \"dark\", \"x\": true}" << "{\"theme\": \"dark\", \"x\": true, \"lastBrightness\": 7}";
        QTest::newRow("repeated") << "{\"lastBrightness\": 1, \"a\": 2, \"lastBrightness\": 3}" << "{\"lastBrightness\": 7, \"a\": 2}";
        QTest::newRow("empty object") << "{}" << "{\"lastBrightness\": 7}";
        QTest::newRow("broken") << "{\"a\": 1,}" << "{\"lastBrightness\": 7}";
        QTest::newRow("trailing") << "{\"a\": 1} x" << "{\"lastBrightness\": 7}";
        QTest::newRow("not an object") << "[1, 2]" << "{\"lastBrightness\": 7}";
        QTest::newRow("bad number") << "{\"a\": 01}" << "{\"lastBrightness\": 7}";
    }

    void stateKeepsOtherKeys()
    {
        QFETCH(QString, before);
        QFETCH(QString, after);
        QTemporaryDir dir;
        const QString file = dir.path() + "/akkoctl/state.json";
        if (!before.isNull()) {
            QDir().mkpath(dir.path() + "/akkoctl");
            QFile f(file);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(before.toUtf8());
        }
        rememberBrightness(file.toStdString(), 7);
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(f.readAll()), after);
        QCOMPARE(lastBrightnessOr(file.toStdString(), 0), 7);
    }
};

int runCliTests(int argc, char **argv)
{
    CliTests t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_cli.moc"
