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
        constexpr int halfTaps = 32;
        constexpr int phases = 16;
        constexpr int taps = 2 * halfTaps + 1;
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

static double besselI0(double x) {
    double sum=1.0, term=1.0;
    const double y=x*x*0.25;
    for(int k=1;k<=80;++k) {
        term*=y/(static_cast<double>(k)*k);
        sum+=term;
        if(term<sum*1.0e-16) break;
    }
    return sum;
}

// Independent 4x Kaiser reference with a longer FIR than the production
// 48-tap BS.1770 detector. This is a test oracle, not realtime code.
static double independentPolyphasePeak4x(const std::vector<skelly::StereoFrame>& samples) {
    constexpr int up=4, taps=257, center=128, phases=4;
    static const auto coefficients=[] {
        constexpr int up=4, taps=257, center=128, phases=4;
        std::array<double,taps> h {};
        double sum=0.0;
        constexpr double beta=12.0;
        const double i0beta=besselI0(beta);
        for(int n=0;n<taps;++n) {
            const double x=static_cast<double>(n-center);
            const double normalized=x/center;
            const double sinc=std::abs(x)<1.0e-12 ? 0.25
                : 0.25*std::sin(independentPi*0.25*x)/(independentPi*0.25*x);
            const double window=besselI0(beta*std::sqrt(
                std::max(0.0,1.0-normalized*normalized)))/i0beta;
            h[static_cast<std::size_t>(n)]=sinc*window;
            sum+=h[static_cast<std::size_t>(n)];
        }
        for(auto& value:h) value/=sum;
        return h;
    }();

    double peak=0.0;
    for(int frame=32;frame<static_cast<int>(samples.size())-32;++frame) {
        const auto& current=samples[static_cast<std::size_t>(frame)];
        peak=std::max(peak,std::max(std::abs(static_cast<double>(current.left)),
                                    std::abs(static_cast<double>(current.right))));
        for(int phase=0;phase<phases;++phase) {
            double left=0.0,right=0.0;
            for(int q=0;up*q+phase<taps;++q) {
                const auto& sample=samples[static_cast<std::size_t>(frame+32-q)];
                const double weight=up*coefficients[static_cast<std::size_t>(up*q+phase)];
                left+=weight*sample.left;
                right+=weight*sample.right;
            }
            peak=std::max(peak,std::max(std::abs(left),std::abs(right)));
        }
    }
    return peak;
}

int main() {
    using namespace skelly;
    constexpr double pi=3.1415926535897932384626433832795;
    const double target=detail::dbToLinear(-1.1); // Acceptance floor: -1.1 dBTP, 0.1 dB below the -1.0 dBTP ceiling.
    const double targetTolerance=target*detail::dbToLinear(0.1); // Independent reconstruction tolerance: 0.1 dB.

    // High-frequency sine waves produce reconstructed inter-sample peaks
    // materially higher than their host-rate sample peaks.
    for (const double rate : {44100.0,48000.0,96000.0}) {
        StereoTruePeakGuard guard;
        StereoTruePeakMonitor outputMeter;
        guard.prepare(rate);
        outputMeter.reset();
        require(guard.getLatencySamples()==32,
                "true-peak guard must report its exact 32-sample lookahead");
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
        require(maxTruePeak<=targetTolerance,
                "high-frequency intersample peak exceeded -1.1 dBTP target (+0.1 dB measurement tolerance)");
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
        const double referencePeak4x = independentPolyphasePeak4x(input);
        require(std::abs(referencePeak4x - 0.95) < 0.01,
                "independent 4x Kaiser reference must recover the 20kHz sine peak");
        require(std::abs(referencePeak - 0.95) < 0.002,
                "independent 16x sinc reference must recover the 20kHz sine peak");
        const double independentPeak = independentSincPeak(rendered);
        std::cout << "INFO: independent 16x sinc 20kHz true peak at " << rate
                  << " Hz = "
                  << 20.0 * std::log10(std::max(1.0e-12, independentPeak))
                  << " dBTP; production target -1.1 dBTP (+0.1 dB measurement tolerance)\\n";
        require(independentPeak <= targetTolerance,
                "independent 16x sinc reconstruction exceeded -1.1 dBTP target (+0.1 dB measurement tolerance) at 20kHz");
        require(independentPeak >= detail::dbToLinear(-1.35),
                "20kHz true-peak guard attenuated more than 0.25 dB beyond its -1.1 dBTP target");
        std::cout << "PASS: independent 16x sinc 20kHz true peak at "
                  << rate << " Hz = "
                  << 20.0 * std::log10(std::max(1.0e-12, independentPeak))
                  << " dBTP\n";
    }

    // Legacy factory path must retain the original sample-only guard while
    // Deuce Match opts into reconstructed true-peak limiting.
    {
        constexpr double rate = 48000.0;
        constexpr double ceiling = 0.8912509381337456; // -1 dBFS
        StereoPeakGuard legacy;
        legacy.prepare(rate);
        StereoTruePeakGuard legacyDelay;
        legacyDelay.prepare(rate);
        StereoTruePeakGuard deuce;
        deuce.prepare(rate);
        std::vector<StereoFrame> legacyRendered, legacyDelayed, deuceRendered;
        legacyRendered.reserve(7200);
        legacyDelayed.reserve(7200);
        deuceRendered.reserve(7200);
        for (int i = 0; i < 7200; ++i) {
            const double phase = 2.0 * 3.14159265358979323846
                * 20000.0 * static_cast<double>(i) / rate + 3.14159265358979323846 / 4.0;
            const float left = static_cast<float>(0.92 * std::sin(phase));
            const float right = left * 0.73f;
            const auto legacyFrame = legacy.process({left, right}, ceiling);
            legacyRendered.push_back(legacyFrame);
            legacyDelayed.push_back(legacyDelay.process(legacyFrame, ceiling, true));
            deuceRendered.push_back(deuce.process({left, right}, ceiling, false));
        }
        double legacySamplePeak = 0.0;
        for (const auto& frame : legacyRendered)
            legacySamplePeak = std::max(legacySamplePeak,
                std::max(std::abs(static_cast<double>(frame.left)),
                         std::abs(static_cast<double>(frame.right))));
        const double legacyTruePeak = independentSincPeak(legacyDelayed);
        const double deuceTruePeak = independentSincPeak(deuceRendered);
        for (std::size_t i = 0; i < legacyDelayed.size(); ++i) {
            const StereoFrame expected = i < 32 ? StereoFrame{}
                : legacyRendered[i - 32];
            require(legacyDelayed[i].left == expected.left
                    && legacyDelayed[i].right == expected.right,
                    "legacy sample guard changed while applying fixed PDC delay");
            require(legacyDelay.currentGain() == 1.0,
                    "legacy PDC delay must not apply true-peak attenuation");
        }
        require(legacySamplePeak > 0.85 && legacySamplePeak < ceiling - 1.0e-4,
                "legacy sample-peak guard must pass through samples below its ceiling");
        require(legacyTruePeak > detail::dbToLinear(-1.0) + 1.0e-3,
                "legacy path unexpectedly enabled the new true-peak limiter");
        require(deuceTruePeak <= targetTolerance,
                "Deuce Match true-peak path failed the same inter-sample stress signal");
        std::cout<<"PASS: legacy sample guard preserved; Deuce Match true-peak peak "
                 <<20.0*std::log10(std::max(1.0e-12,deuceTruePeak))<<" dBTP\\n";
    }

    // Deterministic full-band bursts verify linked attenuation and finite tails
    // at all supported host rates. The 16x oracle is independent of both the
    // production 257-tap detector and the test's 4x Kaiser cross-check.
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        StereoTruePeakGuard guard;
        StereoTruePeakMonitor meter;
        guard.prepare(rate);
        meter.reset();
        std::mt19937 rng(0x51e11u);
        std::uniform_real_distribution<float> noise(-1.35f,1.35f);
        double maxTruePeak=0.0,worstRatio=0.0;
        const int frames=static_cast<int>(rate*0.25);
        std::vector<StereoFrame> rendered;
        rendered.reserve(static_cast<std::size_t>(frames));
        for(int i=0;i<frames;++i) {
            const float left=noise(rng);
            const float right=-0.61f*left;
            const auto out=guard.process({left,right},detail::dbToLinear(-1.0),false);
            rendered.push_back(out);
            maxTruePeak=std::max(maxTruePeak,
                static_cast<double>(meter.process(out.left,out.right)));
            if(std::abs(out.left)>1.0e-5f)
                worstRatio=std::max(worstRatio,
                    std::abs(static_cast<double>(out.right/out.left)+0.61));
        }
        require(maxTruePeak<=targetTolerance,
                "full-band burst exceeded the true-peak target");
        const double independentPeak=independentPolyphasePeak4x(rendered);
        const double independentPeak16x=independentSincPeak(rendered);
        std::cout << "INFO: full-band " << rate << " Hz independent peaks: Kaiser4x "
                  << 20.0 * std::log10(std::max(1.0e-12, independentPeak))
                  << " dBTP, sinc16x "
                  << 20.0 * std::log10(std::max(1.0e-12, independentPeak16x))
                  << " dBTP; acceptance floor -1.0 dBTP\\n";
        require(independentPeak<=targetTolerance,
                "independent 257-tap Kaiser reconstruction exceeded -1.1 dBTP target (+0.1 dB measurement tolerance) on full-band bursts");
        require(independentPeak16x<=targetTolerance,
                "independent 16x sinc reconstruction exceeded -1.1 dBTP target (+0.1 dB measurement tolerance) on full-band bursts");
        std::cout<<"PASS: full-band true peak at "<<rate<<" Hz; Kaiser "
                 <<20.0*std::log10(std::max(1.0e-12,independentPeak))
                 <<" dBTP, independent 16x sinc "
                 <<20.0*std::log10(std::max(1.0e-12,independentPeak16x))<<" dBTP\\n";
        require(worstRatio<2.0e-5,
                "stereo-linked true-peak gain changed channel balance");
        for(int i=0;i<static_cast<int>(rate/10.0);++i) {
            const auto out=guard.process({0.0f,0.0f},detail::dbToLinear(-1.0),false);
            require(std::isfinite(out.left)&&std::isfinite(out.right),
                    "true-peak release tail became nonfinite");
        }
    }

    // Independent left/right broadband noise catches stereo combinations that
    // the earlier linked anti-phase noise fixture does not exercise. Match the
    // hosted VST3 acceptance margin, then cross-check with the independent 16x
    // sinc oracle rather than relying only on the production detector.
    for (const double rate : {44100.0, 48000.0, 96000.0}) {
        StereoTruePeakGuard guard;
        StereoTruePeakMonitor meter;
        guard.prepare(rate);
        meter.reset();
        std::mt19937 rng(0x51e11u);
        std::uniform_real_distribution<float> noise(-0.95f, 0.95f);
        const int frames = static_cast<int>(rate * 0.25);
        std::vector<StereoFrame> rendered;
        rendered.reserve(static_cast<std::size_t>(frames));
        double maxMonitorPeak = 0.0;
        for (int i = 0; i < frames; ++i) {
            const float left = noise(rng);
            const float right = noise(rng);
            const auto out = guard.process({left, right}, detail::dbToLinear(-1.0), false);
            require(std::isfinite(out.left) && std::isfinite(out.right),
                    "independent stereo noise produced nonfinite true-peak output");
            rendered.push_back(out);
            maxMonitorPeak = std::max(maxMonitorPeak,
                static_cast<double>(meter.process(out.left, out.right)));
        }
        const double independentPeak16x = independentSincPeak(rendered);
        const double hostedMarginTarget = detail::dbToLinear(-1.0);
        std::cout << "INFO: independent stereo noise " << rate
                  << " Hz: 48-tap monitor "
                  << 20.0 * std::log10(std::max(1.0e-12, maxMonitorPeak))
                  << " dBTP, independent 16x sinc "
                  << 20.0 * std::log10(std::max(1.0e-12, independentPeak16x))
                  << " dBTP; hosted acceptance -1.0 dBTP\\n";
        require(maxMonitorPeak <= hostedMarginTarget + 3.0e-4,
                "independent stereo noise exceeded the hosted -1.0dBTP ceiling");
        require(independentPeak16x <= hostedMarginTarget + 3.0e-4,
                "independent stereo noise exceeded the independent 16x -1.0dBTP ceiling");
    }

    // Bypass remains bit-exact except for the fixed, reported delay.
    {
        StereoTruePeakGuard guard;
        guard.prepare(48000.0);
        for(int i=0;i<40;++i) {
            const float x=i==0 ? 0.25f : 0.0f;
            const auto out=guard.process({x,-0.5f*x},
                                         detail::dbToLinear(-1.0),true);
            if(i<32) require(out.left==0.0f&&out.right==0.0f,
                             "bypass delay emitted before its 32-sample latency");
            if(i==32) require(out.left==0.25f&&out.right==-0.125f,
                             "bypass did not preserve the exact delayed impulse");
            require(guard.currentGain()==1.0,
                    "bypass must never apply true-peak attenuation");
        }
    }

    // A bypassed guard must keep its FIR history warm while skipping the
    // expensive reconstruction. Enabling it later must match an always-tracking
    // guard from the very first delayed output frame.
    {
        StereoTruePeakGuard bypassWarmed;
        StereoTruePeakGuard alwaysTracking;
        bypassWarmed.prepare(48000.0);
        alwaysTracking.prepare(48000.0);
        const double ceiling = detail::dbToLinear(-1.0);
        std::vector<StereoFrame> resumedOutput;
        resumedOutput.reserve(4096);
        for (int i = 0; i < 256; ++i) {
            const float x = static_cast<float>(0.05 * std::sin(
                2.0 * detail::pi * 997.0 * static_cast<double>(i) / 48000.0));
            const StereoFrame input { x, -0.61f * x };
            const auto bypassOut = bypassWarmed.process(input, ceiling, true);
            const auto trackedOut = alwaysTracking.process(input, ceiling, false);
            require(bypassOut.left == trackedOut.left && bypassOut.right == trackedOut.right,
                    "bypassed warm-up changed the fixed-latency dry signal");
            resumedOutput.push_back(bypassOut);
        }
        double maxSamplePeak = 0.0;
        for (int i = 256; i < 4096; ++i) {
            const double phase = 2.0 * detail::pi * 20000.0
                * static_cast<double>(i) / 48000.0 + detail::pi / 4.0;
            const float left = static_cast<float>(0.92 * std::sin(phase));
            const StereoFrame input { left, 0.73f * left };
            const auto bypassOut = bypassWarmed.process(input, ceiling, false);
            const auto trackedOut = alwaysTracking.process(input, ceiling, false);
            require(std::abs(bypassOut.left - trackedOut.left) < 1.0e-7f
                    && std::abs(bypassOut.right - trackedOut.right) < 1.0e-7f,
                    "bypassed true-peak detector lost history before re-enable");
            resumedOutput.push_back(bypassOut);
            maxSamplePeak = std::max(maxSamplePeak,
                std::max(std::abs(static_cast<double>(bypassOut.left)),
                         std::abs(static_cast<double>(bypassOut.right))));
        }
        require(maxSamplePeak > 0.1 && maxSamplePeak < 0.90,
                "re-enabled Deuce guard did not resume finite peak control");
        const double resumedTruePeak = independentSincPeak(resumedOutput);
        require(resumedTruePeak <= targetTolerance,
                "reactivated Deuce guard exceeded the -1.1 dBTP target plus 0.1 dB measurement tolerance");
        std::cout << "PASS: bypass skips FIR measurement, retains history, and re-enables cleanly; "
                  << "reactivated true peak "
                  << 20.0 * std::log10(std::max(1.0e-12, resumedTruePeak)) << " dBTP\n";
    }

    // Direct detector regression: history-only updates must produce the same
    // first subsequent reconstruction as full measurement throughout warm-up.
    {
        StereoTruePeakDetector4x historyOnly;
        StereoTruePeakDetector4x continuouslyMeasured;
        historyOnly.prepare();
        continuouslyMeasured.prepare();
        for (int i = 0; i < 384; ++i) {
            const double phase = 2.0 * detail::pi * (300.0 + (i % 7) * 130.0)
                * static_cast<double>(i) / 48000.0;
            const float left = static_cast<float>(0.3 * std::sin(phase));
            const float right = -0.57f * left;
            require(historyOnly.process(left, right, false) == 0.0f,
                    "history-only detector unexpectedly performed peak measurement");
            (void) continuouslyMeasured.process(left, right, true);
        }
        for (int i = 384; i < 768; ++i) {
            const double phase = 2.0 * detail::pi * 20000.0
                * static_cast<double>(i) / 48000.0;
            const float left = static_cast<float>(0.9 * std::sin(phase));
            const float right = -0.63f * left;
            const float resumed = historyOnly.process(left, right, true);
            const float reference = continuouslyMeasured.process(left, right, true);
            require(std::abs(resumed - reference) < 1.0e-7f,
                    "history-only detector diverged after measurement resumed");
        }
        std::cout << "PASS: history-only detector resumes exact rolling FIR measurement\n";
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
