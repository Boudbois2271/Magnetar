// SPDX-License-Identifier: GPL-3.0-or-later
#include "travel.h"

namespace akko {

std::string travelProblem(const KeyTravel &t, TravelPart part)
{
    const std::string key = "key " + std::to_string(t.key) + ": ";
    switch (part) {
    case TravelPart::Actuation:
        if (t.actuation < kMinActuation || t.actuation > kMaxActuation)
            return key + "actuation out of range";
        break;
    case TravelPart::RapidTrigger: {
        const std::uint16_t lo = t.rapidTrigger ? kMinRapidTrigger : 0;
        if (t.press < lo || t.press > kMaxRapidTrigger || t.release < lo || t.release > kMaxRapidTrigger)
            return key + "Rapid Trigger out of range";
        break;
    }
    case TravelPart::DeadZones:
        if (t.deadTop > kMaxDeadZone || t.deadBottom > kMaxDeadZone)
            return key + "dead zone out of range";
        break;
    }
    return {};
}

bool samePart(const KeyTravel &a, const KeyTravel &b, TravelPart part)
{
    switch (part) {
    case TravelPart::Actuation:
        return a.actuation == b.actuation && a.actuationTail == b.actuationTail;
    case TravelPart::RapidTrigger:
        return a.rapidTrigger == b.rapidTrigger && a.press == b.press && a.release == b.release &&
               a.rapidTail == b.rapidTail;
    case TravelPart::DeadZones:
        return a.deadZones == b.deadZones && a.deadTop == b.deadTop && a.deadBottom == b.deadBottom;
    }
    return false;
}

}  // namespace akko
