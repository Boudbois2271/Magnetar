// SPDX-License-Identifier: GPL-3.0-or-later
#include "mural.h"

#include <algorithm>
#include <array>
#include <initializer_list>

namespace akko {

namespace {

// Key IDs row by row, left to right (akko-protocol.md, Keymap). Row r, position c = LED c × 8 + r.
const std::array<std::vector<std::uint16_t>, 6> kRows = {{
    {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x63, 0x62},
    {0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x64},
    {0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x66},
    {0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x67},
    {0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x4a, 0x65},
    {0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4c, 0x4b, 0x4d},
}};

}  // namespace

std::optional<std::uint8_t> muralLed(std::uint16_t key)
{
    for (std::size_t r = 0; r < kRows.size(); ++r) {
        const auto &row = kRows[r];
        const auto it = std::find(row.begin(), row.end(), key);
        if (it != row.end())
            return static_cast<std::uint8_t>((it - row.begin()) * 8 + r);
    }
    return std::nullopt;
}

std::uint16_t rgb565(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    return static_cast<std::uint16_t>((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

std::vector<Frame> muralFrames(std::span<const KeyColor> colors)
{
    std::map<std::uint8_t, std::uint16_t> led;  // every LED, black unless a key says otherwise
    for (const auto &row : kRows)
        for (auto key : row)
            led[*muralLed(key)] = 0;
    for (const auto &c : colors)
        if (const auto l = muralLed(c.key))
            led[*l] = rgb565(c.rgb[0], c.rgb[1], c.rgb[2]);

    std::vector<std::pair<std::uint16_t, std::vector<std::uint8_t>>> groups;  // first appearance order
    for (const auto &[l, colour] : led) {
        auto it = std::find_if(groups.begin(), groups.end(), [&](const auto &g) { return g.first == colour; });
        if (it == groups.end())
            it = groups.insert(groups.end(), {colour, {}});
        it->second.push_back(l);
    }
    std::vector<std::uint8_t> stream;
    for (const auto &[colour, leds] : groups) {
        stream.push_back(static_cast<std::uint8_t>(colour >> 8));
        stream.push_back(static_cast<std::uint8_t>(colour & 0xFF));
        stream.push_back(static_cast<std::uint8_t>(leds.size()));
        stream.insert(stream.end(), leds.begin(), leds.end());
    }

    const std::size_t count = (stream.size() + kMuralPacketPayload - 1) / kMuralPacketPayload;
    std::vector<Frame> frames;
    for (std::size_t i = 0; i < count; ++i) {
        const auto begin = stream.begin() + static_cast<std::ptrdiff_t>(i * kMuralPacketPayload);
        const auto end = stream.begin() + static_cast<std::ptrdiff_t>(std::min(stream.size(), (i + 1) * kMuralPacketPayload));
        const std::vector<std::uint8_t> part(begin, end);
        frames.push_back(buildFrame(0x08, 0x04, part, static_cast<std::uint8_t>(count), static_cast<std::uint8_t>(i)));
    }
    return frames;
}

std::map<std::uint8_t, std::uint16_t> muralDecode(std::span<const Frame> packets)
{
    std::vector<std::uint8_t> stream;
    for (const auto &f : packets) {
        if (f[0] != 0x08 || f[1] != 0x04 || f[5] > kMuralPacketPayload)
            throw Error("not a Mural packet");
        stream.insert(stream.end(), f.begin() + kPayloadOffset, f.begin() + kPayloadOffset + f[5]);
    }
    std::map<std::uint8_t, std::uint16_t> out;
    std::size_t i = 0;
    while (i < stream.size()) {
        if (i + 3 > stream.size() || i + 3 + stream[i + 2] > stream.size())
            throw Error("Mural frame ends inside a group");
        const auto colour = static_cast<std::uint16_t>(stream[i] << 8 | stream[i + 1]);
        const std::size_t n = stream[i + 2];
        for (std::size_t k = 0; k < n; ++k)
            if (!out.emplace(stream[i + 3 + k], colour).second)
                throw Error("Mural frame lists an LED twice");
        i += 3 + n;
    }
    return out;
}

}  // namespace akko
