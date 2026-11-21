// SPDX-License-Identifier: GPL-3.0-or-later
// Per-key travel: actuation point, Rapid Trigger and dead zones (akko-protocol.md, Per-key travel).
// Distances are in board units of 0.005 mm.
#pragma once

#include <cstdint>
#include <string>

namespace akko {

constexpr double kMmPerUnit = 0.005;

// What Magnetar lets you set. Actuation is the vendor app's own range (0.1 to 3.5 mm). The Rapid
// Trigger and dead zone caps are ours: the vendor's maxima were never captured, so we stay well
// inside the values seen (limitations.md).
constexpr std::uint16_t kMinActuation = 20, kMaxActuation = 700;
constexpr std::uint16_t kMinRapidTrigger = 1, kMaxRapidTrigger = 200;
constexpr std::uint16_t kMaxDeadZone = 100;

struct KeyTravel {
    std::uint16_t key = 0;

    std::uint16_t actuation = 0;    // 13 00 / 93 00
    std::uint8_t actuationTail = 0;  // last byte of the entry, meaning unknown: kept as read

    bool rapidTrigger = false;       // 19 00 / 99 00
    std::uint16_t press = 0, release = 0;  // kept while RT is off; 0 out of the box
    std::uint8_t rapidTail = 0;

    bool deadZones = false;          // 16 00 / 96 00
    std::uint16_t deadTop = 0, deadBottom = 0;  // kept while off

    bool operator==(const KeyTravel &) const = default;
};

enum class TravelPart { Actuation, RapidTrigger, DeadZones };

// Empty when the part's values are inside the limits above; otherwise why they are not. A key whose
// Rapid Trigger is off may keep 0 / 0 (the factory values).
std::string travelProblem(const KeyTravel &t, TravelPart part);

// True when the two agree on the fields that part writes.
bool samePart(const KeyTravel &a, const KeyTravel &b, TravelPart part);

}  // namespace akko
