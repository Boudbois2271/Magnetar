// SPDX-License-Identifier: GPL-3.0-or-later
// Runs keyboard work off the UI thread. Each batch of queued actions opens one session (the
// shared lock plus the device), runs the actions in order, reads the keyboard's state back and
// closes the session again. So the lock is only held for a moment, and the bar widget and
// akkoctl keep working while the app is open.
// With a Mural source set, the worker streams it (08 04) whenever nothing is queued, in slices of
// about a second, letting go of the lock between slices and as soon as an action is queued.
#pragma once

#include "keyboard.h"
#include "transport.h"

#include <chrono>
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
// travel (32 frames), the advanced keys (1 + one per key), the per-key colours (8 frames) and
// the macros are only read again when the profile changed, after an error, or when asked;
// writes update them from their read-back.
struct Snapshot {
    int profile = 0;  // 0-based
    int profileCount = 0;
    akko::Lighting lighting;
    akko::BaseFunctions base;  // Base Functions settings, read with every batch
    std::uint16_t firmware = 0;
    // Set by an action after which the keyboard restarts (a polling-rate change): the rest of
    // the batch is dropped, nothing more is read and everything is read again next batch.
    bool restarting = false;
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

    int colorsProfile = -1;              // same, for the per-key colours below
    std::vector<akko::KeyColor> colors;  // Custom effect colours, all 83 keys in keyReadOrder()

    int macrosProfile = -1;           // same, for the macros below (they are per profile)
    std::vector<akko::Macro> macros;  // in blob order
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

    // Fills `colors` with the colours to show now and returns true when they changed since the
    // last call. Called on the worker thread.
    using MuralSource = std::function<bool(std::vector<akko::KeyColor> &colors)>;
    // Streams the source while it is set; an empty one stops. Slices that fail (no keyboard,
    // lock busy) are dropped quietly and tried again a second later: the board just shows its
    // own effect meanwhile, and queued actions report errors as usual.
    void setMural(MuralSource source);
    static constexpr std::chrono::milliseconds kMuralSlice{1000};  // session length
    static constexpr std::chrono::milliseconds kMuralGap{60};      // > akkoctl's 20 ms lock poll
    static constexpr std::chrono::milliseconds kMuralRetry{1000};  // after a failed slice

private:
    struct Item {
        std::string key;
        Action action;
    };
    void loop();
    // One Mural session; false if it failed.
    bool streamSlice(const MuralSource &source, unsigned generation);

    SessionFactory sessions_;
    std::function<void(const Snapshot &)> onState_;
    std::function<void(const std::string &)> onError_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Item> queue_;
    bool stopping_ = false;
    Snapshot state_;  // worker thread only, apart from keymapStale_
    bool keymapStale_ = true;
    MuralSource mural_;
    unsigned muralGeneration_ = 0;  // bumped by every setMural, so a slice sees the change
    std::thread thread_;  // last, so it starts after everything it uses exists
};
