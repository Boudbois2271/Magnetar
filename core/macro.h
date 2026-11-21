// SPDX-License-Identifier: GPL-3.0-or-later
// Macros: the blob that holds them all (05 00 / 85 00) and the keymap action that runs one.
// Layout in the Macros section of akko-protocol.md.
#pragma once

#include "keymap.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace akko {

constexpr std::size_t kMacroStorage = 4096;    // "Macro Storage: N B / 4096B"
constexpr std::size_t kMacroPacket = 56;       // bytes of blob per 05 00 / 85 00 packet
constexpr std::uint16_t kStandardDelayMs = 50;  // "Use Standard Delay (50ms)"
constexpr std::size_t kMinMacroName = 2;        // the vendor app refuses shorter names
constexpr std::size_t kMaxMacroName = 255;      // name_len is one byte

// One step: a key goes down or up, then the macro waits delayMs. usage = HID keyboard usage.
struct MacroStep {
    bool press = true;
    std::uint16_t delayMs = kStandardDelayMs;
    std::uint8_t usage = 0;

    bool operator==(const MacroStep &) const = default;
};

struct Macro {
    std::string name;
    std::vector<MacroStep> steps;

    bool operator==(const Macro &) const = default;
};

// Bytes the macro takes in the blob, its directory entry not included.
std::size_t macroSize(const Macro &m);
// Bytes the whole blob takes (directory included); 0 for no macros.
std::size_t macroStorageUsed(std::span<const Macro> macros);

// Why the macro can't be stored, or "" if it can: name of 2–255 printable ASCII characters,
// at least one step, no step with usage 0.
std::string macroProblem(const Macro &m);
// The same for a whole list, and that it fits in kMacroStorage.
std::string macrosProblem(std::span<const Macro> macros);

// The vendor app gives the last step a delay of 1 ms; macros are written that way.
Macro normalizedMacro(Macro m);

// Blob ⇄ macros. parseMacroBlob throws Error if the directory doesn't fit the bytes;
// macroBlob throws Error if macrosProblem() objects. No macros = an empty blob.
std::vector<Macro> parseMacroBlob(std::span<const std::uint8_t> blob);
std::vector<std::uint8_t> macroBlob(std::span<const Macro> macros);

// How a bound key runs its macro: the tt byte of 03 tt nn ii.
enum class MacroLoop : std::uint8_t {
    Times = 0x01,            // "Specified Times", count nn
    UntilKeyPressed = 0x02,  // "Repeat until any key is pressed", count 00
    WhileHeld = 0x04,        // "Repeat While Held, Stop on Release", count 00
};
constexpr int kMaxMacroRepeat = 255;

struct MacroBinding {
    int index = 0;  // 0-based, in blob order
    MacroLoop loop = MacroLoop::Times;
    int count = 1;  // Times only

    bool operator==(const MacroBinding &) const = default;
};

// 03 tt nn ii. Throws Error for an index outside 0–255 or a Times count outside 1–255.
Action macroAction(const MacroBinding &b);
// nullopt unless the action is a macro with a known loop type.
std::optional<MacroBinding> macroBinding(const Action &a);

}  // namespace akko
