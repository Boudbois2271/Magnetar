// SPDX-License-Identifier: GPL-3.0-or-later
// Mural: live per-LED colours (08 04, akko-protocol.md). The board shows a frame while frames
// keep coming and goes back to its stored effect when they stop; nothing is stored.
#pragma once

#include "frame.h"
#include "keyboard.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace akko {

constexpr std::size_t kMuralLeds = 83;
constexpr std::size_t kMuralPacketPayload = 0x38;  // 56 bytes per packet, groups run across packets

// The Mural LED behind a key: column × 8 + row, the column counting keys from the left in
// that row (the LED grid has exactly as many LEDs per row as the board has keys).
std::optional<std::uint8_t> muralLed(std::uint16_t key);

// RGB565, as the frames carry colours.
std::uint16_t rgb565(std::uint8_t r, std::uint8_t g, std::uint8_t b);

// One Mural frame showing these colours: every LED once (keys not listed are black), LEDs in
// ascending order, grouped by colour (colour u16, n, n × LED) and cut into 56-byte packets.
// Unknown keys are ignored.
std::vector<Frame> muralFrames(std::span<const KeyColor> colors);

// LED → RGB565 of a frame's packets; throws Error if the groups don't parse or an LED repeats.
std::map<std::uint8_t, std::uint16_t> muralDecode(std::span<const Frame> packets);

}  // namespace akko
