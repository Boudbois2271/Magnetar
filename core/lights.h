// SPDX-License-Identifier: GPL-3.0-or-later
// Lighting actions shared by the CLI and the app, including the remembered brightness that
// lets "lights on" come back to where "lights off" left it. The value lives in
// ~/.local/state/akkoctl/state.json, so both tools (and the bar widget) agree on it.
#pragma once

#include "keyboard.h"

#include <array>
#include <cstdint>
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

enum class LightsAction { On, Off, Toggle };

// Read-modify-write of 04 01, keeping every other lighting byte as it is.
void setBrightness(Keyboard &kb, int value, const std::string &stateFile);  // clamped to 0..20
void setColor(Keyboard &kb, const std::array<std::uint8_t, 3> &rgb);         // also leaves rainbow
void setRainbow(Keyboard &kb, bool on);
void setSpeed(Keyboard &kb, int speed);                                      // clamped to 0..4
void switchLights(Keyboard &kb, LightsAction action, const std::string &stateFile);

}  // namespace akko
