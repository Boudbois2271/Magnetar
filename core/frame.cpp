// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame.h"

#include <cstdio>

namespace akko {

std::uint8_t checksum(std::span<const std::uint8_t> frame)
{
    unsigned sum = kReportId;
    for (std::size_t i = 0; i < kFrameLen - 1 && i < frame.size(); ++i)
        sum += frame[i];
    return static_cast<std::uint8_t>((0xFF - sum) & 0xFF);
}

Frame buildFrame(std::uint8_t cmd, std::uint8_t sub, std::span<const std::uint8_t> payload,
                 std::uint8_t packetCount, std::uint8_t packetIndex, std::uint8_t arg)
{
    if (payload.size() > kMaxPayload)
        throw Error("payload too long for one frame (" + std::to_string(payload.size()) + " bytes)");
    Frame f{};
    f[0] = cmd;
    f[1] = sub;
    f[2] = arg;
    f[3] = packetCount;
    f[4] = packetIndex;
    f[5] = static_cast<std::uint8_t>(payload.size());
    for (std::size_t i = 0; i < payload.size(); ++i)
        f[kPayloadOffset + i] = payload[i];
    f[kFrameLen - 1] = checksum(f);
    return f;
}

bool isReplyTo(std::span<const std::uint8_t> report, std::uint8_t cmd, std::uint8_t sub,
               std::uint8_t packetIndex)
{
    return report.size() == kReportLen && report[0] == kReportId && report[1] == cmd && report[2] == sub &&
           report[5] == packetIndex;
}

std::string commandName(std::uint8_t cmd, std::uint8_t sub)
{
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02x %02x", cmd, sub);
    return buf;
}

}  // namespace akko
