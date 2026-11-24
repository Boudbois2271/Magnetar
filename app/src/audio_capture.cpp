// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio_capture.h"

#include "visualiser.h"

#include <pulse/error.h>
#include <pulse/simple.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace {

class PulseCapture : public AudioCapture {
public:
    PulseCapture(Sink sink, Status status) : sink_(std::move(sink)), status_(std::move(status)), thread_([this] { run(); }) {}

    ~PulseCapture() override
    {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        wake_.notify_all();
        thread_.join();  // a read returns within one block (~21 ms)
    }

private:
    void run()
    {
        const pa_sample_spec spec{PA_SAMPLE_FLOAT32NE, Visualiser::kRate, 1};
        // Small fragments, so the bars keep up with the sound and stopping is quick.
        const std::uint32_t block = Visualiser::kHop * sizeof(float);
        const pa_buffer_attr attr{std::uint32_t(-1), std::uint32_t(-1), std::uint32_t(-1), std::uint32_t(-1), block};
        std::vector<float> buffer(Visualiser::kHop);
        while (!stopping()) {
            int error = 0;
            pa_simple *s = pa_simple_new(nullptr, "Magnetar", PA_STREAM_RECORD, "@DEFAULT_MONITOR@",
                                         "Mural visualiser", &spec, nullptr, &attr, &error);
            if (s) {
                status_(true);
                while (!stopping() && pa_simple_read(s, buffer.data(), block, &error) >= 0)
                    sink_(buffer);
                pa_simple_free(s);
            }
            if (stopping())
                return;
            status_(false);
            std::unique_lock lock(mutex_);
            wake_.wait_for(lock, std::chrono::seconds(2), [this] { return stop_; });
        }
    }

    bool stopping()
    {
        std::lock_guard lock(mutex_);
        return stop_;
    }

    Sink sink_;
    Status status_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stop_ = false;
    std::thread thread_;  // last, so it starts after everything it uses exists
};

}  // namespace

AudioCaptureFactory pulseCapture()
{
    return [](AudioCapture::Sink sink, AudioCapture::Status status) -> std::unique_ptr<AudioCapture> {
        return std::make_unique<PulseCapture>(std::move(sink), std::move(status));
    };
}
