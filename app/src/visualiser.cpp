// SPDX-License-Identifier: GPL-3.0-or-later
#include "visualiser.h"

#include "keymap.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>

namespace {

constexpr double kSecondsPerSpectrum = double(Visualiser::kHop) / Visualiser::kRate;
constexpr double kRangeDb = 40;      // a bar's height covers this much below the peak
constexpr double kPeakMinDb = -30;   // the automatic gain never boosts more than this
constexpr double kPeakFallDb = 6;    // per second: a loud moment is forgotten this fast
constexpr double kSilenceDb = -70;   // quieter than this is nothing (dBFS)
constexpr double kTiltDb = 1.5;      // per octave above kLowHz: music is quieter up high
constexpr double kRise = 0.7;        // a bar goes this part of the way up per spectrum
constexpr double kFall = 1.6;        // a bar drops at most this much (of its height) per second
// Key centres in akko::layout() (quarter key widths) of the first and last band.
constexpr double kFirstCentre = 2, kLastCentre = 64;

// In-place radix-2 FFT; a.size() is a power of two.
void fft(std::vector<std::complex<double>> &a)
{
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double angle = -2 * std::numbers::pi / double(len);
        const std::complex<double> step(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= step;
            }
        }
    }
}

// Row 0 (the F-keys) to 5 (the space bar) from a key's y in akko::layout().
int rowOf(int y)
{
    constexpr int kRowTop[] = {0, 5, 9, 13, 17, 21};
    int row = 0;
    for (int r = 0; r < Visualiser::kRows; ++r)
        if (y >= kRowTop[r])
            row = r;
    return row;
}

std::array<double, 3> hue(double h)  // h in 0…1 → full RGB, 0…1
{
    const double x = h * 6;
    const auto ch = [&](double offset) {
        const double d = std::fmod(x + offset, 6.0);
        return std::clamp(std::abs(d - 3) - 1, 0.0, 1.0);
    };
    return {ch(0), ch(4), ch(2)};
}

}  // namespace

Visualiser::Visualiser() : window_(kWindow), samples_(kWindow, 0.0f), peakDb_(kPeakMinDb)
{
    for (int n = 0; n < kWindow; ++n)
        window_[n] = float(0.5 - 0.5 * std::cos(2 * std::numbers::pi * n / (kWindow - 1)));
}

bool Visualiser::feed(std::span<const float> samples)
{
    bool fresh = false;
    for (float s : samples) {
        samples_[(pending_ + kWindow - kHop) % kWindow] = std::isfinite(s) ? std::clamp(s, -1.0f, 1.0f) : 0.0f;
        // samples_ is kept in order: shift by a hop at a time, not by a sample.
        if (++pending_ == kHop) {
            analyse();
            std::rotate(samples_.begin(), samples_.begin() + kHop, samples_.end());
            pending_ = 0;
            fresh = true;
        }
    }
    return fresh;
}

void Visualiser::reset()
{
    std::fill(samples_.begin(), samples_.end(), 0.0f);
    pending_ = 0;
    levels_.fill(0);
    peakDb_ = kPeakMinDb;
}

void Visualiser::analyse()
{
    // samples_ holds the window with the newest hop at the end, rotated just after.
    std::vector<std::complex<double>> x(kWindow);
    double windowPower = 0;
    for (int n = 0; n < kWindow; ++n) {
        x[n] = double(samples_[n]) * window_[n];
        windowPower += double(window_[n]) * window_[n];
    }
    fft(x);

    // A full-scale sine puts ~0 dB into its band (Parseval, positive half).
    const double norm = kWindow * windowPower / 4;
    const double binHz = double(kRate) / kWindow;
    std::array<double, kBands> db{};
    double loudest = -1e9;
    // Band edges in bins, log-spaced, each band at least one bin wide (the low ones are).
    std::array<int, kBands + 1> edge{};
    for (int b = 0; b <= kBands; ++b) {
        const double hz = kLowHz * std::pow(kHighHz / kLowHz, double(b) / kBands);
        edge[b] = std::max(b ? edge[b - 1] + 1 : 1, int(std::lround(hz / binHz)));
    }
    for (int b = 0; b < kBands; ++b) {
        const int k0 = edge[b], k1 = edge[b + 1] - 1;
        const double centreHz = (k0 + k1) / 2.0 * binHz;
        double energy = 0;
        for (int k = k0; k <= k1 && k < kWindow / 2; ++k)
            energy += std::norm(x[k]);
        const double raw = 10 * std::log10(energy / norm + 1e-12);
        db[b] = raw < kSilenceDb ? -1e9 : raw + kTiltDb * std::log2(std::max(centreHz, kLowHz) / kLowHz);
        loudest = std::max(loudest, db[b]);
    }

    peakDb_ = std::max({loudest, peakDb_ - kPeakFallDb * kSecondsPerSpectrum, kPeakMinDb});
    const double gainDb = 20 * std::log10(kSensitivity);
    for (int b = 0; b < kBands; ++b) {
        const double target = std::clamp((db[b] + gainDb - (peakDb_ - kRangeDb)) / kRangeDb, 0.0, 1.0);
        double &level = levels_[b];
        level = target > level ? level + (target - level) * kRise : std::max(target, level - kFall * kSecondsPerSpectrum);
    }
}

std::vector<akko::KeyColor> Visualiser::colors() const
{
    std::vector<akko::KeyColor> out;
    for (const auto &k : akko::layout()) {
        const double f = std::clamp((k.x + k.w / 2.0 - kFirstCentre) / (kLastCentre - kFirstCentre), 0.0, 1.0);
        const double p = f * (kBands - 1);
        const int b = std::min(int(p), kBands - 2);
        const double level = levels_[b] + (levels_[b + 1] - levels_[b]) * (p - b);
        const int fromBottom = kRows - 1 - rowOf(k.y);
        const double lit = std::clamp(level * kRows - fromBottom, 0.0, 1.0) * std::clamp(brightness_, 0.0, 1.0);
        const std::array<double, 3> base = settings_.rainbow
            ? hue(f * 0.8)  // red on the left to violet on the right
            : std::array<double, 3>{settings_.color[0] / 255.0, settings_.color[1] / 255.0, settings_.color[2] / 255.0};
        akko::KeyColor c{k.id, {}};
        for (int i = 0; i < 3; ++i)
            c.rgb[i] = static_cast<std::uint8_t>(std::lround(base[i] * lit * 255));
        out.push_back(c);
    }
    return out;
}
