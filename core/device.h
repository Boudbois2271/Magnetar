// SPDX-License-Identifier: GPL-3.0-or-later
// Finding the keyboard, holding the session lock, and talking to its hidraw node.
#pragma once

#include "transport.h"

#include <string>

namespace akko {

constexpr unsigned kVendorId = 0x38EE;
constexpr unsigned kProductId = 0x0016;

// The /dev/hidrawN node of the vendor interface (usage page 0xFF60), found by scanning sysfs.
// sysRoot is only changed by tests.
std::string findHidraw(const std::string &sysRoot = "/sys/class/hidraw");

// $XDG_RUNTIME_DIR/akkoctl.lock, shared with the bar widget and the capture workflow.
std::string defaultLockPath();

// Exclusive flock held for the whole session. Two sessions at once make the firmware stop
// sending keystrokes until the next clean command, so every tool must take this first.
class SessionLock {
public:
    explicit SessionLock(const std::string &path = defaultLockPath(), int timeoutMs = 5000);
    ~SessionLock();
    SessionLock(const SessionLock &) = delete;
    SessionLock &operator=(const SessionLock &) = delete;

private:
    int fd_ = -1;
};

class HidrawTransport : public Transport {
public:
    explicit HidrawTransport(const std::string &path);
    ~HidrawTransport() override;
    HidrawTransport(const HidrawTransport &) = delete;
    HidrawTransport &operator=(const HidrawTransport &) = delete;

    void drain() override;
    void write(std::span<const std::uint8_t> report) override;
    std::optional<std::vector<std::uint8_t>> read(int timeoutMs) override;

private:
    int fd_ = -1;
};

// A ready-to-use connection: lock first, then find and open the keyboard, in that order.
class Connection {
public:
    Connection();
    Transport &transport() { return transport_; }
    const std::string &path() const { return path_; }

private:
    SessionLock lock_;
    std::string path_;
    HidrawTransport transport_;
};

}  // namespace akko
