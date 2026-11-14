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
        // A 12 ms parameter ramp suppresses zipper-noise from block-rate
        // automation without introducing signal-path delay or allocations.
        controlStep = 1.0 - std::exp(-1.0/(rate * 0.012));
        reset();
    }

    void reset() noexcept {
        for (auto& channel : lowState)
            for (auto& band : channel)
                band.fill(0.0);
        smoothWet = 0.0;
        smoothIntensity.fill(0.0);
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

        const double targetWet = std::isfinite(settings.wet)
            ? std::clamp(settings.wet, 0.0, 1.0) : 0.0;
        smoothWet += (targetWet - smoothWet)*controlStep;
        if (targetWet == 0.0 && smoothWet < 1.0e-12)
            smoothWet = 0.0;
        for (std::size_t b = 0; b < 5; ++b) {
            const double target = std::isfinite(settings.intensity[b])
                ? std::clamp(settings.intensity[b], 0.0, 1.0) : 0.0;
            smoothIntensity[b] += (target - smoothIntensity[b]) * controlStep;
            if (target == 0.0 && smoothIntensity[b] < 1.0e-12)
                smoothIntensity[b] = 0.0;
        }
        // Neutral control positions return input *exactly* while crossover
        // history tracks in the background. Returning to zero during
        // coloration fades out smoothly rather than abruptly bypassing.
        if (smoothWet == 0.0) {left=dryL; right=dryR; return;}

        double outL = 0.0, outR = 0.0;
        bool anyColor = false;
        for (std::size_t b = 0; b < 5; ++b) {
            const double amount = smoothIntensity[b];
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
            const double gain = 1.0 + 0.35 * smoothWet * (shapedGain - 1.0);
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
            // Two cascaded one-poles give better isolation than the
            // original shallow 6 dB/oct crossover. All bands still sum
            // algebraically to the untouched dry input (no hidden EQ).
            auto& state = lowState[c][k];
            const double a = pole[k], b = 1.0 - a;
            state[0] = a*state[0] + b*sample;
            state[1] = a*state[1] + b*state[0];
            if (std::abs(state[0]) < 1.0e-28) state[0] = 0.0;
            if (std::abs(state[1]) < 1.0e-28) state[1] = 0.0;
            band[k] = state[1] - lower;
            lower = state[1];
        }
        band[4] = sample - lower;
    }
    std::array<double,4> pole{};
    std::array<std::array<std::array<double,2>,4>,2> lowState{};
    std::array<double,5> smoothIntensity{};
    double smoothWet = 0.0, controlStep = 1.0;
};

} // namespace skelly::experimental
