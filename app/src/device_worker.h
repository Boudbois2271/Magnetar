// SPDX-License-Identifier: GPL-3.0-or-later
// Runs keyboard work off the UI thread. Each batch of queued actions opens one session (the
// shared lock plus the device), runs the actions in order, reads the keyboard's state back and
// closes the session again. So the lock is only held for a moment, and the bar widget and
// akkoctl keep working while the app is open.
#pragma once

#include "keyboard.h"
#include "transport.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// An open connection to the keyboard. The real one is akko::Connection; tests pass a fake.
class Session {
public:
    virtual ~Session() = default;
    virtual akko::Transport &transport() = 0;
    virtual std::string path() const = 0;
};

using SessionFactory = std::function<std::unique_ptr<Session>()>;

// The real thing: takes the lock and opens the hidraw node, or throws akko::Error.
SessionFactory hidrawSessions();

// What the app shows, read from the keyboard after every batch.
struct Snapshot {
    int profile = 0;  // 0-based
    int profileCount = 0;
    akko::Lighting lighting;
    std::uint16_t firmware = 0;
    std::string path;
};

class DeviceWorker {
public:
    using Action = std::function<void(akko::Keyboard &)>;

    // Both callbacks run on the worker thread.
    DeviceWorker(SessionFactory sessions, std::function<void(const Snapshot &)> onState,
                 std::function<void(const std::string &)> onError);
    ~DeviceWorker();
    DeviceWorker(const DeviceWorker &) = delete;
    DeviceWorker &operator=(const DeviceWorker &) = delete;

    // Queue an action. An action still waiting with the same key is replaced instead, so
    // dragging a slider sends only the latest value. An empty key never replaces anything.
    void post(const std::string &key, Action action);

private:
    struct Item {
        std::string key;
        Action action;
    };
    void loop();

    SessionFactory sessions_;
    std::function<void(const Snapshot &)> onState_;
    std::function<void(const std::string &)> onError_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Item> queue_;
    bool stopping_ = false;
    std::thread thread_;  // last, so it starts after everything it uses exists
};
