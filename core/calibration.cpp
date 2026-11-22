// SPDX-License-Identifier: GPL-3.0-or-later
#include "calibration.h"

#include "frame.h"

#include <algorithm>

namespace akko {

std::vector<CalibrationSample> calibrationSamples(std::span<const std::uint8_t> report)
{
    std::vector<CalibrationSample> out;
    if (report.size() < kReportLen || report[0] != kReportId || report[1] != 0x94 || report[2] != 0x02)
        return out;
    const auto frame = report.subspan(1);
    const std::size_t len = std::min<std::size_t>(frame[5], kMaxPayload);
    auto u16 = [&](std::size_t at) { return static_cast<std::uint16_t>(frame[at] << 8 | frame[at + 1]); };
    for (std::size_t at = kPayloadOffset; at + 6 <= kPayloadOffset + len; at += 6) {
        const std::uint16_t cur = u16(at + 2);
        out.push_back({u16(at), static_cast<std::uint16_t>(cur & 0x7fff), u16(at + 4), (cur & 0x8000) != 0});
    }
    return out;
}

}  // namespace akko
