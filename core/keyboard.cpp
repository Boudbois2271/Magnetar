// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard.h"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace akko {

Frame Keyboard::transact(std::uint8_t cmd, std::uint8_t sub, std::span<const std::uint8_t> payload,
                         int timeoutMs, int retries)
{
    return transactPacket(cmd, sub, 1, 0, payload, timeoutMs, retries);
}

Frame Keyboard::transactPacket(std::uint8_t cmd, std::uint8_t sub, std::uint8_t packetCount,
                               std::uint8_t packetIndex, std::span<const std::uint8_t> payload, int timeoutMs,
                               int retries)
{
    using clock = std::chrono::steady_clock;
    const Frame frame = buildFrame(cmd, sub, payload, packetCount, packetIndex);
    std::array<std::uint8_t, kReportLen> report{};
    report[0] = kReportId;
    std::copy(frame.begin(), frame.end(), report.begin() + 1);

    for (int attempt = 0; attempt <= retries; ++attempt) {
        t_.drain();
        t_.write(report);
        const auto deadline = clock::now() + std::chrono::milliseconds(timeoutMs);
        for (;;) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - clock::now());
            if (left.count() <= 0)
                break;
            const auto data = t_.read(static_cast<int>(left.count()));
            if (!data)
                break;  // timed out: send again
            if (isReplyTo(*data, cmd, sub, packetIndex)) {
                Frame answer{};
                std::copy(data->begin() + 1, data->end(), answer.begin());
                return answer;
            }
            // Anything else (key events, another command's answer) is skipped.
        }
    }
    throw Error("no response to command " + commandName(cmd, sub));
}

int Keyboard::profile()
{
    return transact(0x90, 0x00)[kPayloadOffset];
}

int Keyboard::profileCount()
{
    int n = 0;
    try {
        n = transact(0x82, 0x0F)[kPayloadOffset + 3];
    } catch (const Error &) {
        n = 0;
    }
    return (n >= 1 && n <= 8) ? n : 3;
}

void Keyboard::setProfile(int index)
{
    const std::uint8_t p[] = {static_cast<std::uint8_t>(index)};
    transact(0x10, 0x00, p);
}

Lighting Keyboard::lighting()
{
    const Frame r = transact(0x84, 0x01);
    Lighting l;
    std::copy_n(r.begin() + kPayloadOffset, l.bytes.size(), l.bytes.begin());
    return l;
}

void Keyboard::setLighting(const Lighting &l)
{
    transact(0x04, 0x01, l.bytes);
}

void Keyboard::setEffect(int effect)
{
    const std::uint8_t p[] = {static_cast<std::uint8_t>(effect)};
    transact(0x04, 0x02, p);
}

std::uint16_t Keyboard::firmwareVersion()
{
    // The two bytes read 08 01 for v1.08: low byte first, unlike the rest of the protocol.
    const Frame r = transact(0x82, 0x02);
    return static_cast<std::uint16_t>(r[kPayloadOffset] | (r[kPayloadOffset + 1] << 8));
}

namespace {

void checkLayer(int layer)
{
    if (layer < 0 || layer >= kLayerCount)
        throw Error("no such layer: " + std::to_string(layer));
}

std::uint16_t be16(const Frame &f, std::size_t at)
{
    return static_cast<std::uint16_t>(f[at] << 8 | f[at + 1]);
}

}  // namespace

std::vector<std::pair<std::uint16_t, Action>> Keyboard::readLayer(int layer)
{
    checkLayer(layer);
    constexpr std::size_t kPerPacket = 9, kEntry = 6;  // key u16 + action
    const auto order = keyReadOrder();
    const auto packets = static_cast<std::uint8_t>((order.size() + kPerPacket - 1) / kPerPacket);
    std::vector<std::pair<std::uint16_t, Action>> keys;
    for (std::uint8_t p = 0; p < packets; ++p) {
        const auto chunk = order.subspan(p * kPerPacket, std::min(kPerPacket, order.size() - p * kPerPacket));
        std::vector<std::uint8_t> request;
        for (auto id : chunk) {
            request.push_back(static_cast<std::uint8_t>(id >> 8));
            request.push_back(static_cast<std::uint8_t>(id & 0xFF));
        }
        const Frame r = transactPacket(0x83, static_cast<std::uint8_t>(layer), packets, p, request);
        const std::size_t len = std::min<std::size_t>(r[5], kMaxPayload);
        for (std::size_t at = kPayloadOffset; at + kEntry <= kPayloadOffset + len; at += kEntry) {
            const std::uint16_t id = be16(r, at);
            if (std::find(chunk.begin(), chunk.end(), id) == chunk.end())
                continue;  // not a key we asked for in this packet
            Action a;
            std::copy_n(r.begin() + at + 2, 4, a.begin());
            keys.emplace_back(id, a);
        }
    }
    if (keys.size() != order.size())
        throw Error("incomplete keymap read on layer " + std::to_string(layer));
    return keys;
}

Action Keyboard::readKey(int layer, std::uint16_t key)
{
    checkLayer(layer);
    const std::uint8_t p[] = {static_cast<std::uint8_t>(key >> 8), static_cast<std::uint8_t>(key & 0xFF)};
    const Frame r = transact(0x83, static_cast<std::uint8_t>(layer), p);
    if (be16(r, kPayloadOffset) != key)
        throw Error("keymap read answered for another key");
    Action a;
    std::copy_n(r.begin() + kPayloadOffset + 2, 4, a.begin());
    return a;
}

Action Keyboard::setKey(int layer, std::uint16_t key, const Action &action)
{
    checkLayer(layer);
    const std::uint8_t p[] = {static_cast<std::uint8_t>(key >> 8), static_cast<std::uint8_t>(key & 0xFF),
                              action[0], action[1], action[2], action[3]};
    transact(0x03, static_cast<std::uint8_t>(layer), p);
    const Action back = readKey(layer, key);
    if (back != action)
        throw Error("the keyboard kept " + actionHex(back) + " instead of " + actionHex(action));
    return back;
}

std::string formatFirmware(std::uint16_t version)
{
    char buf[16];
    std::snprintf(buf, sizeof buf, "v%x.%02x", version >> 8, version & 0xFF);
    return buf;
}

}  // namespace akko
