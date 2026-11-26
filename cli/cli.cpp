// SPDX-License-Identifier: GPL-3.0-or-later
#include "cli.h"
#include "lights.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <unistd.h>

namespace fs = std::filesystem;

namespace akko::cli {

namespace {

const char *const kUsage =
    "usage: akkoctl [-h] {status,profile,brightness,color,on,off,toggle,keymap,key} ...\n";

const char *const kHelp =
    "usage: akkoctl [-h] {status,profile,brightness,color,on,off,toggle,keymap,key} ...\n"
    "\n"
    "Akko TAC75 HE profile/lighting control\n"
    "\n"
    "commands:\n"
    "  status        print state as JSON (default)\n"
    "  profile N     switch onboard profile (1-based)\n"
    "  brightness N  set brightness 0-20\n"
    "  color VALUE   set color #rrggbb, or 'rainbow'\n"
    "  on            lighting on\n"
    "  off           lighting off\n"
    "  toggle        lighting toggle\n"
    "  keymap LAYER [reset]\n"
    "                print a layer of the active profile (default, fn, fn1);\n"
    "                reset puts default or fn back to factory\n"
    "  key LAYER KEY [ACTION]\n"
    "                print or set one key; KEY = hex key ID (e.g. 2a for Caps),\n"
    "                ACTION = 4 hex bytes ('00 00 00 29'), 'factory' or 'unbound'\n"
    "\n"
    "Every command prints the keyboard state as one line of JSON.\n";

const char *const kChoices = "status, profile, brightness, color, on, off, toggle, keymap, key";

Parsed usageError(const std::string &what)
{
    return {std::nullopt, std::string(kUsage) + "akkoctl: error: " + what + "\n", 2};
}

// Python's int(): surrounding whitespace, an optional sign, digits. Huge values saturate,
// which gives the same results here (brightness clamps, profile is out of range).
std::optional<int> parseInt(const std::string &text)
{
    std::size_t i = 0, end = text.size();
    while (i < end && std::isspace(static_cast<unsigned char>(text[i])))
        ++i;
    while (end > i && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    bool negative = false;
    if (i < end && (text[i] == '+' || text[i] == '-'))
        negative = text[i++] == '-';
    if (i == end)
        return std::nullopt;
    long long v = 0;
    for (; i < end; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i])))
            return std::nullopt;
        v = std::min<long long>(v * 10 + (text[i] - '0'), INT_MAX);
    }
    return static_cast<int>(negative ? -v : v);
}

std::string lower(std::string s)
{
    for (auto &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string statusJson(Keyboard &kb, const std::string &stateFile)
{
    // Same reads, in the same order, as the Python tool.
    const Lighting l = kb.lighting();
    const int profile = kb.profile() + 1;
    const int count = kb.profileCount();
    char buf[320];
    std::snprintf(buf, sizeof buf,
                  "{\"connected\": true, \"profile\": %d, \"profileCount\": %d, \"effect\": %d, "
                  "\"rainbow\": %s, \"color\": \"#%02x%02x%02x\", \"brightness\": %d, "
                  "\"maxBrightness\": %d, \"on\": %s, \"lastBrightness\": %d}\n",
                  profile, count, l[Lighting::Effect], l.rainbow() ? "true" : "false", l[Lighting::Red],
                  l[Lighting::Green], l[Lighting::Blue], l[Lighting::Brightness], kMaxBrightness,
                  l[Lighting::Brightness] > 0 ? "true" : "false", lastBrightnessOr(stateFile, kMaxBrightness));
    return buf;
}

// ---- keymap ----------------------------------------------------------------

const char *const kLayerNames[kLayerCount] = {"default", "fn", "fn1"};

std::optional<int> parseLayer(const std::string &text)
{
    const std::string l = lower(text);
    for (int i = 0; i < kLayerCount; ++i)
        if (l == kLayerNames[i])
            return i;
    return std::nullopt;
}

// A key ID as akko-protocol.md writes it ("2a"), with or without 0x; only the board's 83 keys.
std::optional<std::uint16_t> parseKeyId(const std::string &text)
{
    std::string h = lower(text);
    if (h.rfind("0x", 0) == 0)
        h = h.substr(2);
    if (h.empty() || h.size() > 4 || !std::all_of(h.begin(), h.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }))
        return std::nullopt;
    const auto id = static_cast<std::uint16_t>(std::stoul(h, nullptr, 16));
    for (const auto &k : layout())
        if (k.id == id)
            return id;
    return std::nullopt;
}

// "00 00 00 29", "00000029" or "unbound", normalised to actionHex() form.
std::optional<Action> parseActionText(const std::string &text)
{
    const std::string t = lower(text);
    if (t == "unbound")
        return kUnbound;
    std::string digits;
    for (char c : t)
        if (c != ' ')
            digits += c;
    if (digits.size() != 8 || !std::all_of(digits.begin(), digits.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; }))
        return std::nullopt;
    // Spaces, if any, must split the bytes as actionHex() does.
    if (t.find(' ') != std::string::npos && t.size() != 11)
        return std::nullopt;
    Action a;
    for (int i = 0; i < 4; ++i)
        a[i] = static_cast<std::uint8_t>(std::stoul(digits.substr(2 * i, 2), nullptr, 16));
    return a;
}

// Only the action forms seen written by the vendor app (akko-protocol.md, Units): system (07),
// profile (09) and select-effect (08 09) keys are GUESS and are refused, except as a key's
// factory action ('factory'), which puts back what the board shipped with.
bool capturedAction(const Action &a)
{
    switch (actionKind(a)) {
    case ActionKind::Key: {
        const auto c = keyCombo(a);
        try {
            return comboAction(c->modifiers, c->keys) == a;
        } catch (const Error &) {
            return false;
        }
    }
    case ActionKind::Mouse: return a[1] >= 1 && a[1] <= 5 && a[2] == 0x01 && a[3] == 0x00;
    case ActionKind::Consumer: return a[1] == 0x00 && (a[2] || a[3]);
    case ActionKind::Macro:
        return (a[1] == 0x01 && a[2] >= 1) || ((a[1] == 0x02 || a[1] == 0x04) && a[2] == 0x00);
    case ActionKind::Lighting: return a[1] <= 4 && a[2] <= 2 && a[3] == 0x00;
    case ActionKind::Layer: return (a[1] == 0x00 || a[1] == 0x01) && a[2] == 0x00 && a[3] == 0x00;
    case ActionKind::Special: return true;  // only 11 00 00 00 maps to Special
    case ActionKind::Unbound: return true;
    default: return false;
    }
}

Parsed parseKeymap(Command cmd, const std::vector<std::string> &args)
{
    const bool key = cmd.kind == Command::Key;
    if (args.size() < (key ? 3u : 2u))
        return usageError(std::string("the following arguments are required: ") + (key ? "layer, key" : "layer"));
    const auto layer = parseLayer(args[1]);
    if (!layer)
        return usageError("argument layer: invalid choice: '" + args[1] + "' (choose from default, fn, fn1)");
    cmd.layer = *layer;

    if (!key) {
        if (args.size() > 3)
            return usageError("unrecognized arguments: " + args[3]);
        if (args.size() == 3) {
            if (lower(args[2]) != "reset")
                return usageError("argument action: invalid choice: '" + args[2] + "' (choose from reset)");
            // 11 02 (Fn1) was never captured.
            if (cmd.layer == 2)
                return usageError("only the default and fn layers can be reset");
            cmd.value = "reset";
        }
        return {cmd, {}, 0};
    }

    const auto id = parseKeyId(args[2]);
    if (!id)
        return usageError("argument key: no such key ID: '" + args[2] + "' (hex, e.g. 2a; see akko-protocol.md)");
    cmd.key = *id;
    if (args.size() == 3)
        return {cmd, {}, 0};

    // The action may come as one argument or as four bytes.
    std::string text;
    for (std::size_t i = 3; i < args.size(); ++i)
        text += (text.empty() ? "" : " ") + args[i];
    if (lower(text) == "factory") {
        cmd.value = "factory";
        return {cmd, {}, 0};
    }
    const auto action = parseActionText(text);
    if (!action)
        return usageError("argument action: invalid action: '" + text +
                          "' (4 hex bytes like '00 00 00 29', 'factory' or 'unbound')");
    if (!capturedAction(*action))
        return usageError("argument action: " + actionHex(*action) +
                          " is not a form captured from the vendor app, so it is not sent");
    cmd.value = actionHex(*action);
    return {cmd, {}, 0};
}

std::string keyId(std::uint16_t key)
{
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02x", key);
    return buf;
}

std::string keyJson(std::uint16_t key, const Action &action, const Action &factory)
{
    return "\"key\": \"" + keyId(key) + "\", \"action\": \"" + actionHex(action) + "\", \"factory\": \"" +
           actionHex(factory) + "\"";
}

std::string runKey(Keyboard &kb, const Command &cmd)
{
    const int profile = kb.profile() + 1;
    const Action factory = factoryAction(cmd.layer, cmd.key);
    // Read first: the answer shows what was there, and an unchanged key is not written.
    Action now = kb.readKey(cmd.layer, cmd.key);
    if (!cmd.value.empty()) {
        const Action want = cmd.value == "factory" ? factory : *parseAction(cmd.value);
        if (actionKind(want) == ActionKind::Macro) {
            const std::size_t count = kb.readMacros().size();
            if (want[3] >= count)
                throw Error("no macro " + std::to_string(want[3] + 1) + " on this profile (it has " +
                            std::to_string(count) + ")");
        }
        if (want != now)
            now = kb.setKey(cmd.layer, cmd.key, want);
    }
    return "{\"connected\": true, \"profile\": " + std::to_string(profile) + ", \"layer\": \"" +
           kLayerNames[cmd.layer] + "\", " + keyJson(cmd.key, now, factory) + "}\n";
}

std::string runKeymap(Keyboard &kb, const Command &cmd)
{
    const int profile = kb.profile() + 1;
    const auto read = cmd.value == "reset" ? kb.resetLayer(cmd.layer) : kb.readLayer(cmd.layer);
    // Printed in layout order (rows top to bottom), not the read order.
    std::string keys;
    for (const auto &k : layout()) {
        const auto it = std::find_if(read.begin(), read.end(), [&](const auto &r) { return r.first == k.id; });
        if (it == read.end())
            throw Error("incomplete keymap read on layer " + std::to_string(cmd.layer));
        keys += (keys.empty() ? "{" : ", {") + keyJson(k.id, it->second, factoryAction(cmd.layer, k.id)) + "}";
    }
    return "{\"connected\": true, \"profile\": " + std::to_string(profile) + ", \"layer\": \"" +
           kLayerNames[cmd.layer] + "\", \"keys\": [" + keys + "]}\n";
}

}  // namespace

Parsed parseArgs(const std::vector<std::string> &args)
{
    if (args.empty())
        return {Command{}, {}, 0};
    for (const auto &a : args)
        if (a == "-h" || a == "--help")
            return {std::nullopt, kHelp, 0};

    static const std::vector<std::pair<std::string, Command::Kind>> kinds = {
        {"status", Command::Status}, {"profile", Command::Profile}, {"brightness", Command::Brightness},
        {"color", Command::Color},   {"on", Command::On},           {"off", Command::Off},
        {"toggle", Command::Toggle}, {"keymap", Command::Keymap}, {"key", Command::Key},
    };
    const auto it = std::find_if(kinds.begin(), kinds.end(), [&](auto &k) { return k.first == args[0]; });
    if (it == kinds.end())
        return usageError("argument cmd: invalid choice: '" + args[0] + "' (choose from " + kChoices + ")");

    Command cmd;
    cmd.kind = it->second;
    if (cmd.kind == Command::Keymap || cmd.kind == Command::Key)
        return parseKeymap(cmd, args);
    const bool takesValue =
        cmd.kind == Command::Profile || cmd.kind == Command::Brightness || cmd.kind == Command::Color;
    const std::size_t want = takesValue ? 2 : 1;
    if (args.size() < want)
        return usageError(std::string("the following arguments are required: ") +
                          (cmd.kind == Command::Color ? "value" : "n"));
    if (args.size() > want) {
        std::string extra;
        for (std::size_t i = want; i < args.size(); ++i)
            extra += (extra.empty() ? "" : " ") + args[i];
        return usageError("unrecognized arguments: " + extra);
    }
    if (cmd.kind == Command::Color) {
        cmd.value = args[1];
    } else if (takesValue) {
        const auto n = parseInt(args[1]);
        if (!n)
            return usageError("argument n: invalid int value: '" + args[1] + "'");
        cmd.number = *n;
    }
    return {cmd, {}, 0};
}

std::string defaultStateFile()
{
    return akko::defaultStateFile();
}

std::string logFileFor(const std::string &stateFile)
{
    return (fs::path(stateFile).parent_path() / "akkoctl.log").string();
}

std::string execute(Keyboard &kb, const Command &cmd, const std::string &stateFile)
{
    switch (cmd.kind) {
    case Command::Status:
        break;
    case Command::Profile: {
        const int count = kb.profileCount();
        if (cmd.number < 1 || cmd.number > count)
            throw Error("profile must be 1.." + std::to_string(count));
        kb.setProfile(cmd.number - 1);
        break;
    }
    case Command::Brightness:
        setBrightness(kb, cmd.number, stateFile);
        break;
    case Command::Color: {
        // Read first, as the Python tool did: a bad colour fails after the read, not before.
        Lighting l = kb.lighting();
        if (lower(cmd.value) == "rainbow") {
            l[Lighting::ColorMode] = 7;
        } else {
            const auto rgb = parseColor(cmd.value);
            l[Lighting::ColorMode] = 0;
            l[Lighting::Red] = rgb[0];
            l[Lighting::Green] = rgb[1];
            l[Lighting::Blue] = rgb[2];
        }
        kb.setLighting(l);
        break;
    }
    case Command::On:
    case Command::Off:
    case Command::Toggle:
        switchLights(kb, cmd.kind == Command::On    ? LightsAction::On
                         : cmd.kind == Command::Off ? LightsAction::Off
                                                    : LightsAction::Toggle,
                     stateFile);
        break;
    case Command::Keymap:
        return runKeymap(kb, cmd);
    case Command::Key:
        return runKey(kb, cmd);
    }
    return statusJson(kb, stateFile);
}

std::string jsonString(const std::string &utf8)
{
    std::string out = "\"";
    char buf[8];
    const auto escape = [&](unsigned cp) {
        std::snprintf(buf, sizeof buf, "\\u%04x", cp);
        out += buf;
    };
    for (std::size_t i = 0; i < utf8.size();) {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c < 0x80) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20 || c == 0x7f)
                    escape(c);
                else
                    out += static_cast<char>(c);
            }
            ++i;
            continue;
        }
        // Decode one UTF-8 sequence; anything malformed becomes U+FFFD.
        const int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 0;
        unsigned cp = len == 4 ? c & 0x07 : len == 3 ? c & 0x0F : c & 0x1F;
        bool ok = len > 0 && i + len <= utf8.size();
        for (int k = 1; ok && k < len; ++k) {
            const unsigned char cc = static_cast<unsigned char>(utf8[i + k]);
            ok = (cc & 0xC0) == 0x80;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok || cp > 0x10FFFF) {
            escape(0xFFFD);
            ++i;
            continue;
        }
        if (cp >= 0x10000) {  // surrogate pair, as Python writes it
            cp -= 0x10000;
            escape(0xD800 + (cp >> 10));
            escape(0xDC00 + (cp & 0x3FF));
        } else {
            escape(cp);
        }
        i += len;
    }
    return out + "\"";
}

std::string errorJson(const std::string &message)
{
    return "{\"connected\": false, \"error\": " + jsonString(message) + "}\n";
}

void logCall(const std::string &logFile, const std::vector<std::string> &args, const std::string &result,
             double elapsedMs)
{
    try {
        const fs::path path(logFile);
        fs::create_directories(path.parent_path());
        std::error_code ec;
        if (fs::exists(path, ec) && fs::file_size(path, ec) > 256 * 1024)
            fs::rename(path, path.string() + ".1", ec);

        const auto now = std::chrono::system_clock::now();
        const std::time_t secs = std::chrono::system_clock::to_time_t(now);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        std::tm tm{};
        localtime_r(&secs, &tm);
        char stamp[16];
        std::strftime(stamp, sizeof stamp, "%H:%M:%S", &tm);

        std::string argv;
        for (const auto &a : args.empty() ? std::vector<std::string>{"status"} : args)
            argv += (argv.empty() ? "" : " ") + a;

        char line[64];
        std::snprintf(line, sizeof line, "%s.%03lld pid=%d ppid=%d ", stamp, static_cast<long long>(ms),
                      static_cast<int>(getpid()), static_cast<int>(getppid()));
        char tail[32];
        std::snprintf(tail, sizeof tail, " (%.0f ms)\n", elapsedMs);

        std::ofstream out(path, std::ios::app);
        out << line << argv << " -> " << result << tail;
    } catch (...) {
        // Logging is best effort, as in the Python tool.
    }
}

}  // namespace akko::cli
