// SPDX-License-Identifier: GPL-3.0-or-later
#include "device_worker.h"

#include "device.h"

#include <algorithm>
#include <exception>

namespace {

class HidrawSession : public Session {
public:
    akko::Transport &transport() override { return connection_.transport(); }
    std::string path() const override { return connection_.path(); }

private:
    akko::Connection connection_;
};

}  // namespace

SessionFactory hidrawSessions()
{
    return [] { return std::make_unique<HidrawSession>(); };
}

DeviceWorker::DeviceWorker(SessionFactory sessions, std::function<void(const Snapshot &)> onState,
                           std::function<void(const std::string &)> onError)
    : sessions_(std::move(sessions)), onState_(std::move(onState)), onError_(std::move(onError)),
      thread_([this] { loop(); })
{
}

DeviceWorker::~DeviceWorker()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
        queue_.clear();
    }
    wake_.notify_all();
    thread_.join();
}

void DeviceWorker::post(const std::string &key, Action action)
{
    {
        std::lock_guard lock(mutex_);
        auto waiting = key.empty() ? queue_.end()
                                   : std::find_if(queue_.begin(), queue_.end(),
                                                  [&](const Item &i) { return i.key == key; });
        if (waiting != queue_.end())
            waiting->action = std::move(action);
        else
            queue_.push_back({key, std::move(action)});
    }
    wake_.notify_one();
}

void DeviceWorker::invalidateKeymap()
{
    std::lock_guard lock(mutex_);
    keymapStale_ = true;
}

void DeviceWorker::setMural(MuralSource source)
{
    {
        std::lock_guard lock(mutex_);
        mural_ = std::move(source);
        ++muralGeneration_;
    }
    wake_.notify_one();
}

bool DeviceWorker::streamSlice(const MuralSource &source, unsigned generation)
{
    try {
        const auto session = sessions_();
        akko::Keyboard kb(session->transport());
        const auto end = std::chrono::steady_clock::now() + kMuralSlice;
        bool first = true;  // a new session starts from the current colours, not from dark
        kb.streamMural(
            [&] {
                std::lock_guard lock(mutex_);
                return !stopping_ && queue_.empty() && muralGeneration_ == generation
                    && std::chrono::steady_clock::now() < end;
            },
            [&](std::vector<akko::KeyColor> &colors) { return source(colors) || std::exchange(first, false); });
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

void DeviceWorker::loop()
{
    for (;;) {
        std::deque<Item> batch;
        bool keymapStale = false;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !queue_.empty() || mural_; });
            if (stopping_)
                return;
            if (queue_.empty()) {
                // Nothing to do but the Mural: one slice, then a gap long enough for anyone
                // waiting on the lock to get it. Actions and changes cut both short.
                const auto source = mural_;
                const unsigned generation = muralGeneration_;
                lock.unlock();
                const bool ok = streamSlice(source, generation);
                lock.lock();
                wake_.wait_for(lock, ok ? kMuralGap : kMuralRetry, [&] {
                    return stopping_ || !queue_.empty() || muralGeneration_ != generation;
                });
                continue;
            }
            batch.swap(queue_);
            keymapStale = std::exchange(keymapStale_, false);
        }
        Snapshot &s = state_;
        try {
            const auto session = sessions_();
            akko::Keyboard kb(session->transport());
            if (keymapStale)
                s.keymapProfile = s.travelProfile = s.advancedProfile = s.colorsProfile = s.macrosProfile = -1;
            s.restarting = false;
            for (auto &item : batch) {
                item.action(kb, s);
                if (s.restarting)
                    break;
            }
            if (s.restarting) {
                // The board is about to drop off USB: don't touch it again in this session.
                s.keymapProfile = s.travelProfile = s.advancedProfile = s.colorsProfile = s.macrosProfile = -1;
                onState_(s);
                continue;
            }
            s.lighting = kb.lighting();
            s.base = kb.baseFunctions();
            s.profile = kb.profile();
            s.profileCount = kb.profileCount();
            s.firmware = kb.firmwareVersion();
            s.path = session->path();
            if (s.keymapProfile != s.profile) {
                for (int layer = 0; layer < akko::kLayerCount; ++layer)
                    s.keymap[layer] = kb.readLayer(layer);
                s.keymapProfile = s.profile;
            }
            if (s.travelProfile != s.profile) {
                s.travel = kb.readTravel();
                s.travelProfile = s.profile;
            }
            if (s.advancedProfile != s.profile) {
                s.advanced = kb.readAdvancedKeys();
                s.advancedProfile = s.profile;
            }
            if (s.colorsProfile != s.profile) {
                s.colors = kb.readColors();
                s.colorsProfile = s.profile;
            }
            if (s.macrosProfile != s.profile) {
                s.macros = kb.readMacros();
                s.macrosProfile = s.profile;
            }
            if (s.travelUndo && s.travelUndo->profile != s.profile)
                s.travelUndo.reset();
            onState_(s);
        } catch (const std::exception &e) {
            // A failed batch is dropped: the user sees the error and the real state on retry,
            // keymap and travel included.
            s.keymapProfile = s.travelProfile = s.advancedProfile = s.colorsProfile = s.macrosProfile = -1;
            onError_(e.what());
        }
    }
}
