// SPDX-License-Identifier: GPL-3.0-or-later
#include "lights.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>

#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace akko {

namespace {

constexpr int kMaxSpeed = 4;

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
        out << "{\"lastBrightness\": " << value << "}";
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
