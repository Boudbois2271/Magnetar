// SPDX-License-Identifier: GPL-3.0-or-later
#include "lights.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace akko {

namespace {

constexpr int kMaxSpeed = 4;

// A missing or unreadable file, or no "lastBrightness" in it, means no value.
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

// A small JSON reader for state.json: just enough to split the top-level object into its members,
// each kept as written (key and value text), so saving keeps keys we don't use. Every function
// returns the position after what it read, or npos when the text isn't valid JSON.
constexpr std::size_t kBad = std::string::npos;

std::size_t skipSpace(const std::string &t, std::size_t i)
{
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r'))
        ++i;
    return i;
}

std::size_t readString(const std::string &t, std::size_t i)
{
    if (i >= t.size() || t[i] != '"')
        return kBad;
    for (++i; i < t.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(t[i]);
        if (c == '"')
            return i + 1;
        if (c < 0x20)
            return kBad;
        if (c == '\\') {
            if (++i >= t.size())
                return kBad;
            if (t[i] == 'u') {
                for (int k = 0; k < 4; ++k)
                    if (++i >= t.size() || !std::isxdigit(static_cast<unsigned char>(t[i])))
                        return kBad;
            } else if (std::string_view("\"\\/bfnrt").find(t[i]) == std::string_view::npos) {
                return kBad;
            }
        }
    }
    return kBad;
}

std::size_t readNumber(const std::string &t, std::size_t i)
{
    auto digits = [&](std::size_t j) {
        const std::size_t from = j;
        while (j < t.size() && std::isdigit(static_cast<unsigned char>(t[j])))
            ++j;
        return j == from ? kBad : j;
    };
    if (i < t.size() && t[i] == '-')
        ++i;
    if (i < t.size() && t[i] == '0')
        ++i;
    else if ((i = digits(i)) == kBad)
        return kBad;
    if (i < t.size() && t[i] == '.' && (i = digits(i + 1)) == kBad)
        return kBad;
    if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
        ++i;
        if (i < t.size() && (t[i] == '+' || t[i] == '-'))
            ++i;
        if ((i = digits(i)) == kBad)
            return kBad;
    }
    return i;
}

std::size_t readValue(const std::string &t, std::size_t i, int depth = 0);

// Reads "[...]" or "{...}"; members, when given, collects an object's key and value texts.
std::size_t readContainer(const std::string &t, std::size_t i, int depth,
                          std::vector<std::pair<std::string, std::string>> *members = nullptr)
{
    const bool object = t[i] == '{';
    const char close = object ? '}' : ']';
    i = skipSpace(t, i + 1);
    if (i < t.size() && t[i] == close)
        return i + 1;
    for (;;) {
        std::size_t keyStart = i, keyEnd = i;
        if (object) {
            if ((keyEnd = readString(t, i)) == kBad)
                return kBad;
            i = skipSpace(t, keyEnd);
            if (i >= t.size() || t[i] != ':')
                return kBad;
            i = skipSpace(t, i + 1);
        }
        const std::size_t valueStart = i;
        if ((i = readValue(t, i, depth + 1)) == kBad)
            return kBad;
        if (members)
            members->emplace_back(t.substr(keyStart, keyEnd - keyStart), t.substr(valueStart, i - valueStart));
        i = skipSpace(t, i);
        if (i < t.size() && t[i] == close)
            return i + 1;
        if (i >= t.size() || t[i] != ',')
            return kBad;
        i = skipSpace(t, i + 1);
    }
}

std::size_t readValue(const std::string &t, std::size_t i, int depth)
{
    if (i >= t.size() || depth > 64)
        return kBad;
    if (t[i] == '"')
        return readString(t, i);
    if (t[i] == '{' || t[i] == '[')
        return readContainer(t, i, depth);
    for (const char *word : {"true", "false", "null"})
        if (t.compare(i, std::strlen(word), word) == 0)
            return i + std::strlen(word);
    return readNumber(t, i);
}

// The members of the file's top-level object, in order; empty when the file is missing, isn't
// valid JSON or isn't an object (the Python tool started over from {} then too).
std::vector<std::pair<std::string, std::string>> loadMembers(const fs::path &path)
{
    std::ifstream in(path);
    if (!in)
        return {};
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    std::vector<std::pair<std::string, std::string>> members;
    std::size_t i = skipSpace(text, 0);
    if (i >= text.size() || text[i] != '{')
        return {};
    i = readContainer(text, i, 0, &members);
    if (i == kBad || skipSpace(text, i) != text.size())
        return {};
    return members;
}

}  // namespace

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
        // Other keys stay, as written and in order (the Python tool's json.dump kept them too); a
        // repeated "lastBrightness" collapses into its first place, as Python's dict did.
        auto members = loadMembers(path);
        const std::string key = "\"lastBrightness\"";
        bool placed = false;
        std::string text = "{";
        for (const auto &[k, v] : members) {
            if (k == key && placed)
                continue;
            text += text.size() > 1 ? ", " : "";
            text += k + ": " + (k == key ? std::to_string(value) : v);
            placed = placed || k == key;
        }
        if (!placed)
            text += (text.size() > 1 ? ", " : "") + key + ": " + std::to_string(value);
        out << text << "}";
        if (!out.flush())
            throw Error("cannot write " + tmp.string());
    }
    fs::rename(tmp, path);
}

std::array<std::uint8_t, 3> parseColor(const std::string &text)
{
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
        throw Error("bad colour '" + text + "', expected #rrggbb");
    std::array<std::uint8_t, 3> rgb{};
    for (std::size_t i = 0; i < 3; ++i)
        rgb[i] = static_cast<std::uint8_t>(std::stoul(h.substr(i * 2, 2), nullptr, 16));
    return rgb;
}

void setBrightness(Keyboard &kb, int value, const std::string &stateFile)
{
    Lighting l = kb.lighting();
    l[Lighting::Brightness] = static_cast<std::uint8_t>(std::clamp(value, 0, kMaxBrightness));
    kb.setLighting(l);
    rememberBrightness(stateFile, l[Lighting::Brightness]);
}

void setColor(Keyboard &kb, const std::array<std::uint8_t, 3> &rgb)
{
    Lighting l = kb.lighting();
    l[Lighting::ColorMode] = 0;
    l[Lighting::Red] = rgb[0];
    l[Lighting::Green] = rgb[1];
    l[Lighting::Blue] = rgb[2];
    kb.setLighting(l);
}

void setRainbow(Keyboard &kb, bool on)
{
    Lighting l = kb.lighting();
    l[Lighting::ColorMode] = on ? 7 : 0;
    kb.setLighting(l);
}

void setSpeed(Keyboard &kb, int speed)
{
    Lighting l = kb.lighting();
    l[Lighting::Speed] = static_cast<std::uint8_t>(std::clamp(speed, 0, kMaxSpeed));
    kb.setLighting(l);
}

void switchLights(Keyboard &kb, LightsAction action, const std::string &stateFile)
{
    Lighting l = kb.lighting();
    const bool isOn = l[Lighting::Brightness] > 0;
    const bool turnOn = action == LightsAction::On || (action == LightsAction::Toggle && !isOn);
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
}

}  // namespace akko
