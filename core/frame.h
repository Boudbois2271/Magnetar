// SPDX-License-Identifier: GPL-3.0-or-later
// Frames of the keyboard's vendor HID interface (report ID 9, 63 data bytes).
// Layout and checksum are described in akko-protocol.md.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace akko {

// Every failure the CLI or the app reports to the user is an Error; its message is shown as is.
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

constexpr std::uint8_t kReportId = 0x09;
constexpr std::size_t kFrameLen = 63;              // bytes after the report ID
constexpr std::size_t kReportLen = kFrameLen + 1;  // what goes over hidraw, report ID included
constexpr std::size_t kPayloadOffset = 6;
constexpr std::size_t kMaxPayload = kFrameLen - 1 - kPayloadOffset;  // byte 62 is the checksum

using Frame = std::array<std::uint8_t, kFrameLen>;

// A frame: cmd, sub, arg 0, packet count, packet index, len, payload, checksum. Most commands
// fit in one packet (count 1, index 0); multi-packet reads number their requests.
Frame buildFrame(std::uint8_t cmd, std::uint8_t sub, std::span<const std::uint8_t> payload = {},
                 std::uint8_t packetCount = 1, std::uint8_t packetIndex = 0);

// The byte that makes (report ID + sum of frame[0..61] + checksum) & 0xFF equal 0xFF.
std::uint8_t checksum(std::span<const std::uint8_t> frame);

// True when a 64-byte input report is the keyboard's answer to cmd/sub (and, for a
// multi-packet exchange, to that packet index).
bool isReplyTo(std::span<const std::uint8_t> report, std::uint8_t cmd, std::uint8_t sub,
               std::uint8_t packetIndex = 0);

// "90 00" style name of a command, as used in messages and in akko-protocol.md.
std::string commandName(std::uint8_t cmd, std::uint8_t sub);

}  // namespace akko
