#pragma once

// SKELL MASTER | experimental, portable five-band coloration engine.
// The *production* plugin does NOT activate this prototype. It is an
// independently tested starting point for a future user-approved sound
// engine, never a clone of any proprietary vendor algorithm.
//
// Designed for the existing 4x oversampled DSP domain; no allocations,
// locks, I/O, or coefficient calculations on the audio thread. Zero mix
// and zero-intensity controls return exact original input samples while
// tracking filter state to avoid stale-history clicks when enabled later.
#include <algorithm>
#include <array>
#include <cmath>

namespace skelly::experimental {

struct FiveBandColorSettings {
    double wet = 0.0;  // 0..1, neutral default
    std::array<double,5> intensity{}; // 0..1 for each band
};

class FiveBandColorPrototype {
public:
    void prepare(double sampleRate) noexcept {
        const double rate = std::isfinite(sampleRate)
            ? std::max(8000.0, sampleRate) : 192000.0;
        constexpr double twoPi = 6.283185307179586476925286766559;
        constexpr double crossovers[4] {90.0, 500.0, 2594.0, 9310.0};
        for (int i = 0; i < 4; ++i) {
            const double hz = std::min(crossovers[i], rate * 0.45);
            pole[static_cast<std::size_t>(i)] =
                std::exp(-twoPi * hz / rate);
        }
        reset();
    }

    void reset() noexcept {
        for (auto& channel : lowState)
            channel.fill(0.0);
    }

    // Stateful, full-band stereo coloration: one five-band split per
    // channel, and one linked gain per band, derived from the louder
    // channel. Linked band gains preserve mono, polarity and level ratio
    // for frequency-coherent material while introducing gentle harmonics.
    void process(double& left, double& right,
                 const FiveBandColorSettings& settings) noexcept {
        // 48dBFS guard matches the existing JUCE engine's host boundary.
        const auto safe = [](double x) noexcept {
            return std::isfinite(x) ? std::clamp(x,-256.0,256.0) : 0.0;
        };
        const double dryL = safe(left), dryR = safe(right);
        std::array<double,5> bandsL{}, bandsR{};
        split(0, dryL, bandsL);
        split(1, dryR, bandsR);

        const double wet = std::isfinite(settings.wet)
            ? std::clamp(settings.wet, 0.0, 1.0) : 0.0;
        if (wet <= 1.0e-6) {left=dryL; right=dryR; return;}

        double outL = 0.0, outR = 0.0;
        bool anyColor = false;
        for (std::size_t b = 0; b < 5; ++b) {
            const double amount = std::isfinite(settings.intensity[b])
                ? std::clamp(settings.intensity[b],0.0,1.0) : 0.0;
            if (amount <= 1.0e-6) {
                outL += bandsL[b];
                outR += bandsR[b];
                continue;
            }
            anyColor = true;
            // Smooth, bounded rational soft saturator. Unlike per-side
            // clipping, the instantaneous gain is shared by both channels.
            // A gentle 35% maximum blend avoids exaggerated drive.
            const double x = std::max(std::abs(bandsL[b]),std::abs(bandsR[b]));
            const double drive = 0.8 * amount;
            const double shapedGain = (1.0 + drive) / (1.0 + drive * x);
            const double gain = 1.0 + 0.35 * wet * (shapedGain - 1.0);
            outL += bandsL[b] * gain;
            outR += bandsR[b] * gain;
        }
        left = anyColor ? outL : dryL;
        right = anyColor ? outR : dryR;
    }
private:
    void split(int channel, double sample,
               std::array<double,5>& band) noexcept {
        const auto c = static_cast<std::size_t>(channel);
        double lower = 0.0;
        for (std::size_t k = 0; k < 4; ++k) {
            double& state = lowState[c][k];
            const double a = pole[k];
            state = a * state + (1.0 - a) * sample;
            if (std::abs(state) < 1.0e-28) state = 0.0;
            band[k] = state - lower;
            lower = state;
        }
        band[4] = sample - lower;
    }
    std::array<double,4> pole{};
    std::array<std::array<double,4>,2> lowState{};
};

} // namespace skelly::experimental
