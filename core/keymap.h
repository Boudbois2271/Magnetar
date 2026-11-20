// SPDX-License-Identifier: GPL-3.0-or-later
// What a key does: the 4-byte action shared by the keymap and the advanced keys, the 83 keys'
// physical layout, and the factory keymap. Everything here comes from captures, see the
// Units and Keymap sections of akko-protocol.md.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace akko {

constexpr int kLayerCount = 3;  // 0 Default, 1 Fn, 2 Fn1
constexpr int kKeyCount = 83;

using Action = std::array<std::uint8_t, 4>;

enum class ActionKind {
    Key,       // 00 mm k2 k1: a key, a modifier, or a shortcut of modifiers + up to two keys
    Mouse,     // 01 bb 01 00
    Consumer,  // 02 00 hi lo: media, volume, apps
    Macro,     // 03 tt nn ii
    System,    // 07 00 00 nn (GUESS)
    Lighting,  // 08 aa bb 00 (only 08 03 00 00 is SEEN)
    Profile,   // 09 00 00 nn (GUESS)
    Layer,     // 0d nn 00 00: Fn (00) or Fn1 (01)
    Unbound,   // ff ff ff ff
    Unknown,
};

ActionKind actionKind(const Action &a);

// The parts of a Key action: modifier bits (01 LCtrl … 80 RGui) and the keys' HID usages, in
// the order they were chosen (0, 1 or 2 of them).
struct KeyCombo {
    std::uint8_t modifiers = 0;
    std::vector<std::uint8_t> keys;
};
std::optional<KeyCombo> keyCombo(const Action &a);  // nullopt unless the kind is Key

// Encoders for every form that has a captured before/after.
Action keyAction(std::uint8_t usage);                                    // 00 00 00 uu
Action comboAction(std::uint8_t modifiers, std::span<const std::uint8_t> keys);  // 00 mm 00 00 / 00 mm uu 00 / 00 mm k2 k1
Action mouseAction(std::uint8_t button);                                  // 01 bb 01 00: 1 left, 2 right, 3 middle
Action consumerAction(std::uint16_t usage);                               // 02 00 hi lo
Action layerAction(int layer);                                            // 0d 00 00 00 Fn, 0d 01 00 00 Fn1
constexpr Action kUnbound{0xff, 0xff, 0xff, 0xff};
constexpr Action kBrightnessCycle{0x08, 0x03, 0x00, 0x00};

// "00 00 00 29" and back. parseAction returns nullopt unless it is exactly 4 hex bytes.
std::string actionHex(const Action &a);
std::optional<Action> parseAction(const std::string &hex);

// One key of the drawing. Positions and sizes are in quarter key widths, from the top-left
// corner; rows are 4 apart, with a quarter gap under the function row and the arrows dropped
// by a quarter.
struct KeyPosition {
    std::uint16_t id;
    std::uint8_t x, y, w;
};
std::span<const KeyPosition> layout();  // all 83 keys, row by row, left to right

// The 83 key IDs in the order the vendor app reads a layer (10 packets of up to 9).
std::span<const std::uint16_t> keyReadOrder();

// The action each key has out of the box (Fn1 starts as a copy of Default).
Action factoryAction(int layer, std::uint16_t key);

}  // namespace akko
