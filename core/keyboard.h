// SPDX-License-Identifier: GPL-3.0-or-later
// The keyboard's commands, on top of any Transport. Only KNOWN writes live here
// (see the status labels in akko-protocol.md).
#pragma once

#include "frame.h"
#include "transport.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace akko {

constexpr int kMaxBrightness = 20;

// The 7-byte lighting block of 04 01 / 84 01.
struct Lighting {
    enum : std::size_t { Effect, ColorMode, Red, Green, Blue, Brightness, Speed };
    std::array<std::uint8_t, 7> bytes{};

    std::uint8_t &operator[](std::size_t i) { return bytes[i]; }
    std::uint8_t operator[](std::size_t i) const { return bytes[i]; }
    bool rainbow() const { return bytes[ColorMode] >= 7; }
};

// "v1.08" for 0x0108, as the vendor app's Device Info page writes it.
std::string formatFirmware(std::uint16_t version);

class Keyboard {
public:
    explicit Keyboard(Transport &transport) : t_(transport) {}

    // Send one frame and wait for the matching answer. Returns the answer's 63 bytes
    // (report ID stripped), so payload[n] is at index 6 + n. Writes are echoed back.
    Frame transact(std::uint8_t cmd, std::uint8_t sub, std::span<const std::uint8_t> payload = {},
                   int timeoutMs = 1000, int retries = 2);

    int profile();       // 0-based active profile (90 00)
    int profileCount();  // from 82 0f; 3 when the answer is missing or implausible
    void setProfile(int index);

    Lighting lighting();  // 84 01
    void setLighting(const Lighting &l);
    // 04 02: switch effect. The board loads that effect's own stored colour and speed,
    // so read lighting() again afterwards.
    void setEffect(int effect);

    // 82 02: firmware version, e.g. 0x0108 for the v1.08 the vendor app shows.
    std::uint16_t firmwareVersion();

private:
    Transport &t_;
};

}  // namespace akko
