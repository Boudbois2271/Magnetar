// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard.h"

#include <algorithm>
#include <chrono>

namespace akko {

Frame Keyboard::transact(std::uint8_t cmd, std::uint8_t sub, std::span<const std::uint8_t> payload,
                         int timeoutMs, int retries)
{
    using clock = std::chrono::steady_clock;
    const Frame frame = buildFrame(cmd, sub, payload);
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
            if (isReplyTo(*data, cmd, sub)) {
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

}  // namespace akko
