// SPDX-License-Identifier: GPL-3.0-or-later
// The byte pipe to the keyboard. The real one is a hidraw node (device.h); tests use a fake.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace akko {

class Transport {
public:
    virtual ~Transport() = default;

    // Throw away input reports that arrived before our request (stale answers, key events).
    virtual void drain() = 0;

    // Send one output report: report ID followed by the 63-byte frame.
    virtual void write(std::span<const std::uint8_t> report) = 0;

    // Wait up to timeoutMs for one input report. nullopt means nothing arrived in time.
    virtual std::optional<std::vector<std::uint8_t>> read(int timeoutMs) = 0;
};

}  // namespace akko
