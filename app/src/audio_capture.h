// SPDX-License-Identifier: GPL-3.0-or-later
// What the desktop is playing, for the Mural visualiser: the default output's monitor
// (@DEFAULT_MONITOR@ through libpulse-simple, served by pipewire-pulse), mono float samples at
// Visualiser::kRate, read on a thread of its own. Plain C++, no Qt.
#pragma once

#include <functional>
#include <memory>
#include <span>

class AudioCapture {
public:
    // Called on the capture thread with each block of samples.
    using Sink = std::function<void(std::span<const float>)>;
    // Called on the capture thread when the audio opens (true) or is lost (false).
    using Status = std::function<void(bool hearing)>;
    virtual ~AudioCapture() = default;  // stops the thread
};

using AudioCaptureFactory = std::function<std::unique_ptr<AudioCapture>(AudioCapture::Sink, AudioCapture::Status)>;

// The real thing. When the sound server can't be reached it says so and tries again every
// couple of seconds.
AudioCaptureFactory pulseCapture();
