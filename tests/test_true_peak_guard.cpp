#include "../Source/DSP/MasteringEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(EXIT_FAILURE); }
}

static constexpr double independentPi = 3.1415926535897932384626433832795;

// Test-only 16x windowed-sinc reconstruction. This deliberately does not use
// StereoTruePeakMonitor, so the limiter is not checked by its own detector.
static double independentSincPeak(const std::vector<skelly::StereoFrame>& samples) {
    constexpr int halfTaps = 32;
    constexpr int phases = 16;
    constexpr int taps = 2 * halfTaps + 1;
    static const auto weights = [] {
        std::array<std::array<double, taps>, phases> result {};
        for (int phase = 0; phase < phases; ++phase) {
            const double fraction = static_cast<double>(phase) / phases;
            double sum = 0.0;
            for (int tap = -halfTaps; tap <= halfTaps; ++tap) {
                const double distance = fraction - static_cast<double>(tap);
                const double sinc = std::abs(distance) < 1.0e-12
                    ? 1.0 : std::sin(independentPi * distance) / (independentPi * distance);
                const double normalized = distance / halfTaps;
                const double window = std::abs(distance) <= halfTaps
                    ? 0.42 + 0.5 * std::cos(independentPi * normalized)
                          + 0.08 * std::cos(2.0 * independentPi * normalized)
                    : 0.0;
                const double value = sinc * window;
                result[static_cast<std::size_t>(phase)]
                      [static_cast<std::size_t>(tap + halfTaps)] = value;
                sum += value;
            }
            for (auto& value : result[static_cast<std::size_t>(phase)])
                value /= sum;
        }
        return result;
    }();

    double peak = 0.0;
    for (int frame = halfTaps;
         frame < static_cast<int>(samples.size()) - halfTaps - 1; ++frame) {
        const auto& current = samples[static_cast<std::size_t>(frame)];
        peak = std::max(peak, std::max(std::abs(static_cast<double>(current.left)),
                                      std::abs(static_cast<double>(current.right))));
        for (int phase = 1; phase < phases; ++phase) {
            double left = 0.0, right = 0.0;
            for (int tap = -halfTaps; tap <= halfTaps; ++tap) {
                const auto& sample = samples[static_cast<std::size_t>(frame + tap)];
                const double weight = weights[static_cast<std::size_t>(phase)]
                                            [static_cast<std::size_t>(tap + halfTaps)];
                left += weight * sample.left;
                right += weight * sample.right;
            }
            peak = std::max(peak, std::max(std::abs(left), std::abs(right)));
        }
    }
    return peak;
}

int main() {
    using namespace skelly;
    constexpr double pi=3.1415926535897932384626433832795;
    const double target=detail::dbToLinear(-1.25);

    // High-frequency sine waves produce reconstructed inter-sample peaks
    // materially higher than their host-rate sample peaks.
    for (const double rate : {44100.0,48000.0,96000.0}) {
        StereoTruePeakGuard guard;
        StereoTruePeakMonitor outputMeter;
        guard.prepare(rate);
        outputMeter.reset();
        require(guard.getLatencySamples()==11,
                "true-peak guard must report its exact 11-sample lookahead");
        double maxTruePeak=0.0,maxSample=0.0;
        const int frames=static_cast<int>(rate*0.10);
        for(int i=0;i<frames;++i) {
            const double phase=2.0*pi*(rate*0.25)*i/rate+pi/4.0;
            const float left=static_cast<float>(0.95*std::sin(phase));
            const float right=left*0.73f;
            const auto out=guard.process({left,right},detail::dbToLinear(-1.0),false);
            require(std::isfinite(out.left)&&std::isfinite(out.right),
                    "true-peak guard emitted nonfinite audio");
            maxSample=std::max(maxSample,static_cast<double>(
                std::max(std::abs(out.left),std::abs(out.right))));
            maxTruePeak=std::max(maxTruePeak,
                static_cast<double>(outputMeter.process(out.left,out.right)));
        }
        require(maxTruePeak<=target+3.0e-4,
                "high-frequency intersample peak exceeded -1.25 dBTP");
        require(maxSample<=detail::dbToLinear(-1.0)+3.0e-4,
                "true-peak guard failed the sample-peak ceiling as well");
        std::cout<<"PASS: "<<rate<<" Hz high-frequency true-peak ceiling "
                 <<20.0*std::log10(std::max(1.0e-12,maxTruePeak))
                 <<" dBTP, sample peak "
                 <<20.0*std::log10(std::max(1.0e-12,maxSample))<<" dBFS\n";
    }

    // Independent true-peak oracle at 20kHz. The previous test compared the
    // guard to the same 4x FIR used internally and could therefore miss filter
    // under-reading near the top of the audio band.
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        StereoTruePeakGuard guard;
        guard.prepare(rate);
        const int frames = static_cast<int>(rate * 0.15);
        std::vector<StereoFrame> input, rendered;
        input.reserve(static_cast<std::size_t>(frames));
        rendered.reserve(static_cast<std::size_t>(frames));
        for (int i = 0; i < frames; ++i) {
            const double phase = 2.0 * pi * 20000.0 * i / rate + pi / 4.0;
            const float left = static_cast<float>(0.95 * std::sin(phase));
            const float right = left * 0.73f;
            input.push_back({left, right});
            const auto out = guard.process({left, right}, detail::dbToLinear(-1.0), false);
            rendered.push_back(out);
        }
        const double referencePeak = independentSincPeak(input);
        require(std::abs(referencePeak - 0.95) < 0.002,
                "independent 16x sinc reference must recover the 20kHz sine peak");
        const double independentPeak = independentSincPeak(rendered);
        require(independentPeak <= target + 3.0e-4,
                "independent 16x sinc reconstruction exceeded -1.25 dBTP at 20kHz");
        std::cout << "PASS: independent 16x sinc 20kHz true peak at "
                  << rate << " Hz = "
                  << 20.0 * std::log10(std::max(1.0e-12, independentPeak))
                  << " dBTP\n";
    }

    // Deterministic full-band bursts verify linked attenuation and finite tails.
    {
        constexpr double rate=48000.0;
        StereoTruePeakGuard guard;
        StereoTruePeakMonitor meter;
        guard.prepare(rate);
        meter.reset();
        std::mt19937 rng(0x51e11u);
        std::uniform_real_distribution<float> noise(-1.35f,1.35f);
        double maxTruePeak=0.0,worstRatio=0.0;
        for(int i=0;i<12000;++i) {
            const float left=noise(rng);
            const float right=-0.61f*left;
            const auto out=guard.process({left,right},detail::dbToLinear(-1.0),false);
            maxTruePeak=std::max(maxTruePeak,
                static_cast<double>(meter.process(out.left,out.right)));
            if(std::abs(out.left)>1.0e-5f)
                worstRatio=std::max(worstRatio,
                    std::abs(static_cast<double>(out.right/out.left)+0.61));
        }
        require(maxTruePeak<=target+3.0e-4,
                "full-band burst exceeded the true-peak target");
        require(worstRatio<2.0e-5,
                "stereo-linked true-peak gain changed channel balance");
        for(int i=0;i<rate/10;++i) {
            const auto out=guard.process({0.0f,0.0f},detail::dbToLinear(-1.0),false);
            require(std::isfinite(out.left)&&std::isfinite(out.right),
                    "true-peak release tail became nonfinite");
        }
    }

    // Bypass remains bit-exact except for the fixed, reported delay.
    {
        StereoTruePeakGuard guard;
        guard.prepare(48000.0);
        for(int i=0;i<40;++i) {
            const float x=i==0 ? 0.25f : 0.0f;
            const auto out=guard.process({x,-0.5f*x},
                                         detail::dbToLinear(-1.0),true);
            if(i<11) require(out.left==0.0f&&out.right==0.0f,
                             "bypass delay emitted before its 11-sample latency");
            if(i==11) require(out.left==0.25f&&out.right==-0.125f,
                             "bypass did not preserve the exact delayed impulse");
            require(guard.currentGain()==1.0,
                    "bypass must never apply true-peak attenuation");
        }
    }

    // Reset, silence, and malformed inputs cannot poison detector or delay state.
    {
        StereoTruePeakGuard guard;
        guard.prepare(44100.0);
        const auto bad=guard.process({
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity()},
            detail::dbToLinear(-1.0),false);
        require(std::isfinite(bad.left)&&std::isfinite(bad.right),
                "NaN/Inf input poisoned the true-peak guard");
        guard.reset();
        for(int i=0;i<32;++i) {
            const auto out=guard.process({0.0f,0.0f},
                                         detail::dbToLinear(-1.0),false);
            require(out.left==0.0f&&out.right==0.0f,
                    "reset did not clear true-peak delay history");
        }
    }

    std::cout<<"PASS: true-peak lookahead, 44.1/48/96k, stereo link, bypass, reset, finite tails\n";
    return EXIT_SUCCESS;
}
