// SPDX-License-Identifier: GPL-3.0-or-later
// Advanced keys (12 00 / 92 00, the frame's arg byte is the type): Toggle, Mod-Tap, Dynamic
// Keystroke, SOCD, Multi-Point and Release. Encodings are in akko-protocol.md, Advanced keys.
// Depths are in board units of 0.005 mm, times in ms.
#pragma once

#include "keymap.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace akko {

enum class AdvancedType : std::uint8_t {
    None = 0x00,  // also the clear command
    Toggle = 0x01,
    ModTap = 0x02,
    Dks = 0x03,
    Socd = 0x04,
    MultiPoint = 0x05,
    Release = 0x06,
};

// SOCD: what wins when both keys are down. The vendor app offers only these four.
enum class SocdMode : std::uint8_t { Neutral = 0x00, LastInput = 0x01, Key1 = 0x03, Key2 = 0x04 };

// Our limits. Depths: the actuation range (0.1 to 3.5 mm). Hold time: the vendor app's slider
// range was never captured (seen: 200 default, 322, 965), so we stay inside 100 to 1000 ms.
constexpr std::uint16_t kMinHoldMs = 100, kMaxHoldMs = 1000;
constexpr std::size_t kMultiPointSlots = 3;  // the vendor app always sends three
constexpr std::size_t kDksRows = 4, kDksStages = 4;
constexpr Action kNoAction{0, 0, 0, 0};  // an empty Multi-Point or DKS slot

// DKS stage bits, one byte per stage per row (press start, bottom, release from bottom,
// full release).
constexpr std::uint8_t kStageDown = 0x02, kStageHold = 0x04, kStageUp = 0x08;

struct DksRow {
    Action action = kNoAction;
    std::array<std::uint8_t, kDksStages> stages{};
    bool operator==(const DksRow &) const = default;
};

// What a DKS row does in the terms the vendor UI offers: nothing, fire once at one stage
// (from == to), or hold from one stage until another (from < to).
struct DksStroke {
    int from = -1, to = -1;  // stage indexes 0..3; from < 0 = not used
    bool operator==(const DksStroke &) const = default;
};
std::array<std::uint8_t, kDksStages> dksStages(DksStroke s);
std::optional<DksStroke> dksStroke(const std::array<std::uint8_t, kDksStages> &stages);  // nullopt: another pattern

struct PointSlot {
    Action action = kNoAction;
    std::uint16_t depth = 0;
    bool operator==(const PointSlot &) const = default;
};

struct AdvancedKey {
    AdvancedType type = AdvancedType::None;
    std::uint16_t key = 0;   // SOCD: the first key
    Action action{};         // Toggle, Release; Mod-Tap: the hold action
    Action tap{};            // Mod-Tap
    std::uint16_t holdMs = 0;  // Toggle, Mod-Tap

    std::array<std::uint16_t, 4> depths{};  // DKS: press start, bottom, release from bottom, full release
    std::array<DksRow, kDksRows> rows{};

    std::uint16_t key2 = 0;  // SOCD
    SocdMode mode = SocdMode::Neutral;
    std::uint8_t socdLead = 0x02;  // first payload byte of SOCD, always 02 so far: kept as read

    std::vector<PointSlot> points;  // Multi-Point

    bool operator==(const AdvancedKey &) const = default;
};

// A record of that type on that key, everything else empty.
AdvancedKey makeAdvanced(AdvancedType type, std::uint16_t key);

// The keys this advanced key occupies (two for SOCD).
std::vector<std::uint16_t> advancedKeyIds(const AdvancedKey &a);

// The payload of a 12 00 write, and the record a 92 00 read answers. decode returns nullopt
// when the payload is too short for its type or the type is unknown.
std::vector<std::uint8_t> encodeAdvanced(const AdvancedKey &a);
std::optional<AdvancedKey> decodeAdvanced(std::uint8_t type, std::span<const std::uint8_t> payload);

// Empty when the record can be sent; otherwise why not.
std::string advancedProblem(const AdvancedKey &a);

}  // namespace akko
