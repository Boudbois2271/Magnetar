// SPDX-License-Identifier: GPL-3.0-or-later
// Lighting actions shared by the CLI and the app, including the remembered brightness that
// lets "lights on" come back to where "lights off" left it. The value lives in
// ~/.local/state/akkoctl/state.json, so both tools (and the bar widget) agree on it.
#pragma once

#include "keyboard.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace akko {

// ~/.local/state/akkoctl/state.json, or under $XDG_STATE_HOME.
std::string defaultStateFile();

// The remembered brightness, or fallback when there is none.
int lastBrightnessOr(const std::string &stateFile, int fallback);

// Remember a brightness (only values above 0: "off" is never remembered).
void rememberBrightness(const std::string &stateFile, int value);

// "#rgb", "rrggbb", "##rrggbb", surrounding spaces... as the CLI has always accepted.
// Throws Error("bad colour '…', expected #rrggbb").
std::array<std::uint8_t, 3> parseColor(const std::string &text);

// An unsolicited input report the board sends after a change made on the board itself (Fn
// shortcut): `fe 0b kk …` for lighting, `fe 09 pp` for the profile. See akko-protocol.md.
struct BoardReport {
    enum class Kind { Effect, Brightness, Speed, Color, Profile };
    Kind kind = Kind::Brightness;
    std::uint8_t value = 0;             // effect, brightness, speed, colour mode or 0-based profile
    std::array<std::uint8_t, 3> rgb{};  // Color only

    bool operator==(const BoardReport &) const = default;
};

// The change in a whole 64-byte input report (report ID included), checksum checked. nullopt for
// any other report, or a value out of range.
std::optional<BoardReport> boardReport(std::span<const std::uint8_t> report);

// The brightness in an unsolicited `fe 0b 02 bb` input report (64 bytes, report ID included),
// sent after a brightness change made on the board (Fn shortcut). nullopt for anything else.
std::optional<int> brightnessReport(std::span<const std::uint8_t> report);

enum class LightsAction { On, Off, Toggle };

// Read-modify-write of 04 01, keeping every other lighting byte as it is.
void setBrightness(Keyboard &kb, int value, const std::string &stateFile);  // clamped to 0..20
void setColor(Keyboard &kb, const std::array<std::uint8_t, 3> &rgb);         // also leaves rainbow
void setRainbow(Keyboard &kb, bool on);
void setSpeed(Keyboard &kb, int speed);                                      // clamped to 0..4
void switchLights(Keyboard &kb, LightsAction action, const std::string &stateFile);

}  // namespace akko
