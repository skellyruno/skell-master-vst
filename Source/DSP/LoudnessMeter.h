#pragma once

// SKELL MASTER: passive K-weighted output loudness monitor.
// ITU-R BS.1770-5 prefilter, EBU R128 momentary (400 ms) and short term (3 s).
// This is NOT the separately double-gated integrated loudness measurement.
// The audio thread performs no allocation or coefficient design.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace skelly {

class StereoLoudnessMonitor {
public:
    void prepare(double sampleRate, int channels) {
        rate = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 384000.0) : 48000.0;
        isStereo = channels > 1;
        momentaryLength = static_cast<std::size_t>(std::max(1.0, std::round(0.4 * rate)));
        shortLength = static_cast<std::size_t>(std::max(1.0, std::round(3.0 * rate)));
        energyHistory.assign(shortLength, 0.0);
        configureFilters();
        reset();
    }

    void reset() noexcept {
        for (auto& chain : filters)
            for (auto& stage : chain) stage.reset();
        std::fill(energyHistory.begin(), energyHistory.end(), 0.0);
        writePos = 0;
        momentarySum = shortSum = 0.0;
    }

    void process(float left, float right) noexcept {
        if (energyHistory.empty()) return;
        const double l = weighted(0, left);
        const double r = isStereo ? weighted(1, right) : 0.0;
        const double energy = l * l + r * r;

        // Sliding rectangular windows. The history is preallocated in
        // prepare(), and all indexing is constant work per output frame.
        // Subtract before overwriting the oldest short-term sample.
        std::size_t oldMomentary = writePos + shortLength - momentaryLength;
        if (oldMomentary >= shortLength) oldMomentary -= shortLength;
        momentarySum += energy - energyHistory[oldMomentary];
        shortSum += energy - energyHistory[writePos];
        energyHistory[writePos] = energy;
        if (++writePos == shortLength) writePos = 0;
        if (momentarySum < 0.0) momentarySum = 0.0;
        if (shortSum < 0.0) shortSum = 0.0;
    }

    // Silence floor is -100 LUFS, so UI atoms remain finite.
    double momentaryLufs() const noexcept {
        return toLufs(momentarySum / static_cast<double>(momentaryLength));
    }
    double shortTermLufs() const noexcept {
        return toLufs(shortSum / static_cast<double>(shortLength));
    }

private:
    struct Biquad {
        double b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0;
        void set(double nb0, double nb1, double nb2, double na1, double na2) noexcept {
            b0=nb0; b1=nb1; b2=nb2; a1=na1; a2=na2;
        }
        void reset() noexcept { z1=z2=0.0; }
        double process(double sample) noexcept {
            const double output=b0*sample+z1;
            z1=b1*sample-a1*output+z2;
            z2=b2*sample-a2*output;
            if(std::abs(z1)<1.e-25) z1=0.0;
            if(std::abs(z2)<1.e-25) z2=0.0;
            return output;
        }
    };

    void configureFilters() noexcept {
        Biquad shelf, highpass;
        if (std::abs(rate - 48000.0) < 0.001) {
            // Exact published coefficients (Annex 1, Tables 1 and 2).
            shelf.set(1.53512485958697, -2.69169618940638, 1.19839281085285,
                      -1.69065929318241, 0.73248077421585);
            highpass.set(1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621);
        } else {
            // Redesign analog prototypes at the current host rate.
            // The prewarped bilinear forms reproduce the 48 kHz tables.
            constexpr double pi=3.1415926535897932384626433832795;
            constexpr double shelfHz=1681.974450955533;
            constexpr double shelfQ=0.7071752369554196;
            constexpr double rlbHz=38.13547087602444;
            constexpr double rlbQ=0.5003270373238773;
            const double high=std::pow(10.0,3.999843853973347/20.0);
            const double band=std::pow(high,0.499666774155);
            const double k=std::tan(pi*shelfHz/rate);
            const double denominator=1.0+k/shelfQ+k*k;
            shelf.set((high+band*k/shelfQ+k*k)/denominator,
                      2.0*(k*k-high)/denominator,
                      (high-band*k/shelfQ+k*k)/denominator,
                      2.0*(k*k-1.0)/denominator,
                      (1.0-k/shelfQ+k*k)/denominator);
            const double h=std::tan(pi*rlbHz/rate);
            const double hDen=1.0+h/rlbQ+h*h;
            highpass.set(1.0,-2.0,1.0,
                         2.0*(h*h-1.0)/hDen,
                         (1.0-h/rlbQ+h*h)/hDen);
        }
        for(auto& channel : filters) {
            channel[0]=shelf;
            channel[1]=highpass;
        }
    }

    double weighted(int channel,float sample) noexcept {
        double x=std::isfinite(sample) ? std::clamp(static_cast<double>(sample), -256.0, 256.0) : 0.0;
        for(auto& stage : filters[static_cast<std::size_t>(channel)]) x=stage.process(x);
        return std::isfinite(x) ? x : 0.0;
    }

    static double toLufs(double meanSquare) noexcept {
        if(!std::isfinite(meanSquare) || meanSquare <= 1.e-11) return -100.0;
        return std::max(-100.0, -0.691 + 10.0*std::log10(meanSquare));
    }

    double rate=48000.0;
    bool isStereo=true;
    std::size_t momentaryLength=1, shortLength=1, writePos=0;
    std::vector<double> energyHistory;
    double momentarySum=0.0, shortSum=0.0;
    std::array<std::array<Biquad,2>,2> filters {};
};

} // namespace skelly
