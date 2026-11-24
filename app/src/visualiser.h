// SPDX-License-Identifier: GPL-3.0-or-later
// The Mural audio visualiser: mono samples in, one colour per key out. Spectrum bars across the
// board, low notes on the left and high ones on the right, each key placed by where it sits on
// the board (not by its LED column, whose rows are skewed), so the bars stand straight. A bar is
// 0–6 rows tall from the bottom row; the top key of a bar is lit in part. Plain C++, no Qt.
#pragma once

#include "keyboard.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

struct VisualiserSettings {
    double sensitivity = 1.0;  // gain on top of the automatic one: 2 = bars twice as loud
    bool rainbow = true;       // a rainbow across the board, else one colour
    std::array<std::uint8_t, 3> color{255, 255, 255};
};

class Visualiser {
public:
    static constexpr int kRate = 48000;   // samples per second
    static constexpr int kWindow = 2048;  // samples per spectrum
    static constexpr int kHop = 1024;     // a new spectrum every kHop samples (~21 ms)
    static constexpr int kBands = 16;     // log-spaced, kLowHz to kHighHz
    static constexpr double kLowHz = 40, kHighHz = 12000;
    static constexpr int kRows = 6;
    static constexpr double kSensitivityMin = 0.25, kSensitivityMax = 4;

    Visualiser();

    void setSettings(const VisualiserSettings &settings) { settings_ = settings; }
    const VisualiserSettings &settings() const { return settings_; }

    // Adds samples (-1…1). True when at least one new spectrum came out of them.
    bool feed(std::span<const float> samples);
    // Forgets the sound so far: bars drop to nothing, the automatic gain starts over.
    void reset();

    // Bar heights, 0…1 (1 = all six rows), one per band, smoothed.
    std::span<const double> levels() const { return levels_; }
    // One colour for each of the 83 keys, in akko::layout() order; black for a dark key.
    std::vector<akko::KeyColor> colors() const;

private:
    void analyse();

    VisualiserSettings settings_;
    std::vector<float> window_;   // Hann
    std::vector<float> samples_;  // the last kWindow samples
    int pending_ = 0;             // samples since the last spectrum
    std::array<double, kBands> levels_{};
    double peakDb_;  // automatic gain: the loudest band lately, slowly forgotten
};
