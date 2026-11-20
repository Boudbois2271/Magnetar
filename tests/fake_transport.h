// SPDX-License-Identifier: GPL-3.0-or-later
// A pretend keyboard for tests: records every report written and answers from a script.
#pragma once

#include "frame.h"
#include "transport.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <vector>

namespace akko::test {

using Bytes = std::vector<std::uint8_t>;

// A 64-byte input report answering cmd/sub with the given payload bytes.
inline Bytes reply(std::uint8_t cmd, std::uint8_t sub, std::initializer_list<std::uint8_t> payload = {})
{
    Bytes r(kReportLen, 0);
    r[0] = kReportId;
    r[1] = cmd;
    r[2] = sub;
    r[4] = 0x01;
    r[6] = static_cast<std::uint8_t>(payload.size());
    std::size_t i = 7;
    for (auto b : payload)
        r[i++] = b;
    return r;
}

class FakeTransport : public Transport {
public:
    // Called on every write with the report sent; returns what the keyboard "sends back".
    std::function<std::vector<Bytes>(const Bytes &)> respond;

    std::vector<Bytes> written;  // every report sent, report ID included
    std::deque<Bytes> pending;   // input not read yet
    int drains = 0;

    void drain() override
    {
        ++drains;
        pending.clear();
    }

    void write(std::span<const std::uint8_t> report) override
    {
        written.emplace_back(report.begin(), report.end());
        if (respond)
            for (auto &r : respond(written.back()))
                pending.push_back(std::move(r));
    }

    std::optional<Bytes> read(int) override
    {
        if (pending.empty())
            return std::nullopt;  // a timeout, without the wait
        Bytes r = std::move(pending.front());
        pending.pop_front();
        return r;
    }
};

// A keyboard that echoes writes and answers reads from a small state, like the real one.
struct FakeKeyboard {
    std::uint8_t profile = 0;
    std::uint8_t profileCount = 3;
    std::array<std::uint8_t, 7> lighting{0x02, 0x00, 0x00, 0xff, 0x00, 20, 0x04};

    std::vector<Bytes> operator()(const Bytes &w)
    {
        const std::uint8_t cmd = w[1], sub = w[2];
        if (cmd == 0x90 && sub == 0x00)
            return {reply(cmd, sub, {profile})};
        if (cmd == 0x82 && sub == 0x0f)
            return {reply(cmd, sub, {0x00, 0x00, 0x00, profileCount})};
        if (cmd == 0x84 && sub == 0x01) {
            const auto &l = lighting;
            return {reply(cmd, sub, {l[0], l[1], l[2], l[3], l[4], l[5], l[6]})};
        }
        if (cmd == 0x10 && sub == 0x00)
            profile = w[7];
        else if (cmd == 0x04 && sub == 0x01)
            std::copy_n(w.begin() + 7, 7, lighting.begin());
        Bytes echo(w);  // writes are echoed back unchanged
        return {echo};
    }
};

}  // namespace akko::test
