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
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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

using LayerMap = std::vector<std::pair<std::uint16_t, akko::Action>>;

// What the app shows, read from the keyboard after every batch. The keymap (30 frames), the
// travel (32 frames) and the advanced keys (1 + one per key) are only read again when the profile changed, after an error, or when
// asked; writes update them from their read-back.
struct Snapshot {
    int profile = 0;  // 0-based
    int profileCount = 0;
    akko::Lighting lighting;
    std::uint16_t firmware = 0;
    std::string path;
    int keymapProfile = -1;  // profile the keymap below was read from, -1 = not read
    std::array<LayerMap, akko::kLayerCount> keymap;
    int travelProfile = -1;               // same, for the travel below
    std::vector<akko::KeyTravel> travel;  // all 83 keys, in keyReadOrder()

    // The values the last travel change replaced, so it can be undone (one level).
    struct TravelUndo {
        int profile;
        akko::TravelPart part;
        std::vector<akko::KeyTravel> before;
    };
    std::optional<TravelUndo> travelUndo;

    int advancedProfile = -1;                // same, for the advanced keys below
    std::vector<akko::AdvancedKey> advanced;  // in the board's list order
};

class DeviceWorker {
public:
    // An action may update the snapshot it gets (a key write stores its read-back there).
    using Action = std::function<void(akko::Keyboard &, Snapshot &)>;

    // Both callbacks run on the worker thread.
    DeviceWorker(SessionFactory sessions, std::function<void(const Snapshot &)> onState,
                 std::function<void(const std::string &)> onError);
    ~DeviceWorker();
    DeviceWorker(const DeviceWorker &) = delete;
    DeviceWorker &operator=(const DeviceWorker &) = delete;

    // Queue an action. An action still waiting with the same key is replaced instead, so
    // dragging a slider sends only the latest value. An empty key never replaces anything.
    void post(const std::string &key, Action action);
    // Read the whole keymap and travel again at the end of the next batch.
    void invalidateKeymap();

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
    Snapshot state_;  // worker thread only, apart from keymapStale_
    bool keymapStale_ = true;
    std::thread thread_;  // last, so it starts after everything it uses exists
};
