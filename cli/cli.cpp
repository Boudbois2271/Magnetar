// SPDX-License-Identifier: GPL-3.0-or-later
#include "cli.h"

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

#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace akko::cli {

namespace {

const char *const kUsage =
    "usage: akkoctl [-h] {status,profile,brightness,color,on,off,toggle} ...\n";

const char *const kHelp =
    "usage: akkoctl [-h] {status,profile,brightness,color,on,off,toggle} ...\n"
    "\n"
    "Akko TAC75 HE profile/lighting control\n"
    "\n"
    "commands:\n"
    "  status        print state as JSON (default)\n"
    "  profile N     switch onboard profile (1-based)\n"
    "  brightness N  set brightness 0-20\n"
    "  color VALUE   set colour #rrggbb, or 'rainbow'\n"
    "  on            lighting on\n"
    "  off           lighting off\n"
    "  toggle        lighting toggle\n"
    "\n"
    "Every command prints the keyboard state as one line of JSON.\n";

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

// Only "lastBrightness" is ever stored. A missing or unreadable file means no value.
std::optional<long long> loadLastBrightness(const std::string &stateFile)
{
    std::ifstream in(stateFile);
    if (!in)
        return std::nullopt;
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    const std::string key = "\"lastBrightness\"";
    auto pos = text.find(key);
    if (pos == std::string::npos)
        return std::nullopt;
    pos += key.size();
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
        ++pos;
    if (pos >= text.size() || text[pos] != ':')
        return std::nullopt;
    ++pos;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
        ++pos;
    std::size_t end = pos;
    if (end < text.size() && text[end] == '-')
        ++end;
    const std::size_t digits = end;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
        ++end;
    if (end == digits)
        return std::nullopt;
    try {
        return std::stoll(text.substr(pos, end - pos));
    } catch (...) {
        return std::nullopt;
    }
}

int lastBrightnessOr(const std::string &stateFile, int fallback)
{
    const auto v = loadLastBrightness(stateFile);
    return v ? static_cast<int>(std::clamp<long long>(*v, INT_MIN, INT_MAX)) : fallback;
}

void rememberBrightness(const std::string &stateFile, int value)
{
    if (value <= 0)
        return;
    const fs::path path(stateFile);
    fs::create_directories(path.parent_path());
    // Written beside the target and renamed, so a crash never leaves a half-written file.
    const fs::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        out << "{\"lastBrightness\": " << value << "}";
        if (!out.flush())
            throw Error("cannot write " + tmp.string());
    }
    fs::rename(tmp, path);
}

std::array<std::uint8_t, 3> parseColor(const std::string &text)
{
    const auto bad = [&] { return Error("bad colour '" + text + "', expected #rrggbb"); };
    std::size_t b = 0, e = text.size();
    while (b < e && std::isspace(static_cast<unsigned char>(text[b])))
        ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(text[e - 1])))
        --e;
    while (b < e && text[b] == '#')
        ++b;
    std::string h = text.substr(b, e - b);
    if (h.size() == 3)
        h = {h[0], h[0], h[1], h[1], h[2], h[2]};
    if (h.size() != 6 || !std::all_of(h.begin(), h.end(), [](unsigned char c) { return std::isxdigit(c); }))
        throw bad();
    std::array<std::uint8_t, 3> rgb{};
    for (std::size_t i = 0; i < 3; ++i)
        rgb[i] = static_cast<std::uint8_t>(std::stoul(h.substr(i * 2, 2), nullptr, 16));
    return rgb;
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
        {"toggle", Command::Toggle},
    };
    const auto it = std::find_if(kinds.begin(), kinds.end(), [&](auto &k) { return k.first == args[0]; });
    if (it == kinds.end())
        return usageError("argument cmd: invalid choice: '" + args[0] +
                          "' (choose from status, profile, brightness, color, on, off, toggle)");

    Command cmd;
    cmd.kind = it->second;
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
    const char *xdg = std::getenv("XDG_STATE_HOME");
    std::string base;
    if (xdg && *xdg) {
        base = xdg;
    } else {
        const char *home = std::getenv("HOME");
        if (!home || !*home) {
            const passwd *pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        base = std::string(home) + "/.local/state";
    }
    return base + "/akkoctl/state.json";
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
    case Command::Brightness: {
        Lighting l = kb.lighting();
        l[Lighting::Brightness] = static_cast<std::uint8_t>(std::clamp(cmd.number, 0, kMaxBrightness));
        kb.setLighting(l);
        rememberBrightness(stateFile, l[Lighting::Brightness]);
        break;
    }
    case Command::Color: {
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
    case Command::Toggle: {
        Lighting l = kb.lighting();
        const bool isOn = l[Lighting::Brightness] > 0;
        const bool turnOn = cmd.kind == Command::On || (cmd.kind == Command::Toggle && !isOn);
        if (turnOn) {
            if (!isOn) {
                const int last = lastBrightnessOr(stateFile, kMaxBrightness);
                l[Lighting::Brightness] = static_cast<std::uint8_t>(std::max(1, std::min(kMaxBrightness, last)));
                kb.setLighting(l);
            }
        } else if (isOn) {
            rememberBrightness(stateFile, l[Lighting::Brightness]);
            l[Lighting::Brightness] = 0;
            kb.setLighting(l);
        }
        break;
    }
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
