// SPDX-License-Identifier: GPL-3.0-or-later
// Key calibration (akko-protocol.md, Key Calibration). Values are raw sensor readings, which
// fall as a key goes down (around 10000-11000 at rest, around 1600-2300 at the bottom).
#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace akko {

// One key's stored calibration (94 05).
struct KeyCalibration {
    std::uint16_t key = 0;
    std::uint16_t rest = 0, bottom = 0;

    bool operator==(const KeyCalibration &) const = default;
};

// One entry of the 94 02 reports the keyboard sends every 50 ms while calibrating, for the keys
// moved most recently. Finish stores `lowest` as the key's new bottom.
struct CalibrationSample {
    std::uint16_t key = 0;
    std::uint16_t current = 0;  // bits 0-14 of the entry
    std::uint16_t lowest = 0;   // lowest value seen this session
    bool flag = false;          // bit 15; meaning unknown (clear for one report after a full press)

    bool operator==(const CalibrationSample &) const = default;
};

// The entries of a 64-byte input report (report ID first); empty unless it is a 94 02 report.
std::vector<CalibrationSample> calibrationSamples(std::span<const std::uint8_t> report);

}  // namespace akko
