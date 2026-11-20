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

void DeviceWorker::loop()
{
    for (;;) {
        std::deque<Item> batch;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (stopping_)
                return;
            batch.swap(queue_);
        }
        try {
            const auto session = sessions_();
            akko::Keyboard kb(session->transport());
            for (auto &item : batch)
                item.action(kb);
            Snapshot s;
            s.lighting = kb.lighting();
            s.profile = kb.profile();
            s.profileCount = kb.profileCount();
            s.firmware = kb.firmwareVersion();
            s.path = session->path();
            onState_(s);
        } catch (const std::exception &e) {
            // A failed batch is dropped: the user sees the error and the real state on retry.
            onError_(e.what());
        }
    }
}
