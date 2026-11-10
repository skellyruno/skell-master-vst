#include "../Source/DSP/MasteringEngine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main() {
    using namespace skelly;
    constexpr double rate = 192000;
    const int lookahead = static_cast<int>(rate * 0.0015);
    // Drive normalization must track the original transfer within audible
    // precision without repeating tanh(drive) per oversampled stereo frame.
    detail::DriveShaper shaper;
    shaper.prepare();
    for (double amount : {0.0, 0.1, 0.37, 0.69, 1.0}) {
        for (double sample : {-0.7, -0.2, 0.0, 0.27, 0.75}) {
            const double a = 1.0 + 4.0 * amount;
            const double reference = sample + amount *
                         (std::tanh(sample * a) / std::tanh(a) - sample);
            require(std::abs(shaper.process(sample, amount) - reference) < 1.e-6,
                    "drive lookup retains the reference saturation shape");
        }
    }

    // Invalid input must be isolated at the boundary; no persistent poisoning.
    const auto NaN = std::numeric_limits<float>::quiet_NaN();
    const auto Inf = std::numeric_limits<float>::infinity();
    StereoPeakGuard invalidGuard;
    invalidGuard.prepare(48000.0);
    const auto invalidGuardOut = invalidGuard.process({NaN, Inf}, detail::dbToLinear(-1));
    require(std::isfinite(invalidGuardOut.left) && std::isfinite(invalidGuardOut.right),
            "final safety guard rejects non-finite input");

    MasteringEngine invalidEngine;
    invalidEngine.prepare(rate, 2, lookahead);
    MasterParameters nonneutral;
    nonneutral.drive = .7; nonneutral.glue = .4; nonneutral.dynamics = .4;
    nonneutral.lowsDb = 3.0; nonneutral.highsDb = 2.0;
    nonneutral.sauce = .6; nonneutral.transients = .6;
    invalidEngine.setParameters(nonneutral);
    for (int i=0; i<8000; ++i) {
        const auto in = i == 300 ? NaN : i == 301 ? Inf :
                        i == 302 ? -Inf : 0.05f;
        const auto out = invalidEngine.processFrame(in, -in);
        require(std::isfinite(out.left) && std::isfinite(out.right),
                "non-finite samples never poison the mastering engine");
    }
    detail::OnePole tail;
    tail.prepare(rate, 2400);
    tail.low(1.0);
    for (int i=0;i<150000;++i) tail.low(0.0);
    require(std::fpclassify(tail.low(0.0)) == FP_ZERO,
            "lowpass state decays to exact zero, without denormals");
    std::cout << "PASS: NaN/Inf recovery and denormal-safe filter tails\n";

    // Host-rate final sample-peak guard regression:
    // both channels must share one gain to preserve image and phase.
    StereoPeakGuard guard;
    guard.prepare(48000.0);
    const double guardCeiling = detail::dbToLinear(-1.0);
    const auto linked = guard.process({2.0f, -1.0f}, guardCeiling);
    require(std::abs(linked.left) <= guardCeiling + 1.e-6,
            "linked safety limiter holds left sample ceiling");
    require(std::abs(linked.right) <= guardCeiling + 1.e-6,
            "linked safety limiter holds right sample ceiling");
    require(std::abs(linked.left + 2.0f * linked.right) < 1.e-6f,
            "linked safety limiter retains stereo ratio and phase");
    const auto releaseFirst = guard.process({0.25f, -0.125f}, guardCeiling);
    require(releaseFirst.left < 0.24f, "safety recovery is smooth after over");
    for (int i = 0; i < 48000; ++i) guard.process({0.0f, 0.0f}, guardCeiling);
    require(guard.currentGain() > 0.99, "safety limiter eventually recovers unity");
    guard.reset();
    const auto neutralGuard = guard.process({0.1f, -0.1f}, guardCeiling);
    require(std::abs(neutralGuard.left - 0.1f) < 1.e-6f,
            "safety limiter transparent under ceiling");
    std::cout << "PASS: linked final safety, gain release, neutral guard\n";

    // Test high-band only stereo imaging, bass stability and anti-phase guard.
    const auto sideGain = [rate](double sideHz, bool invertPhase) {
        StereoImager imager;
        imager.prepare(rate);
        double initialSide = 0.0, imagedSide = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 0.6); ++i) {
            const double t = static_cast<double>(i) / rate;
            const double mid = invertPhase ? 0.0 : 0.20 * std::sin(2.0 * detail::pi * 1000.0 * t);
            const double side = 0.06 * std::sin(2.0 * detail::pi * sideHz * t);
            double l = mid + side, r = mid - side;
            imager.process(l, r, 1.0);
            if (t > 0.3) {
                initialSide += side * side;
                const double imaged = 0.5 * (l-r);
                imagedSide += imaged * imaged;
            }
        }
        return std::sqrt(imagedSide / initialSide);
    };
    const double bassWidth = sideGain(60.0, false);
    const double highWidth = sideGain(3500.0, false);
    const double antiPhaseWidth = sideGain(3500.0, true);
    require(bassWidth < 1.07, "sauce protects low frequency stereo width");
    require(highWidth > 1.10 && highWidth < 1.21,
            "sauce gently widens high frequency stereo detail");
    require(std::abs(antiPhaseWidth - 1.0) < 0.005,
            "sauce avoids widening highly anti-phase stereo");
    StereoImager neutralImager;
    neutralImager.prepare(rate);
    double neutralL = 0.1, neutralR = -0.05;
    neutralImager.process(neutralL, neutralR, 0.0);
    require(std::abs(neutralL - 0.1) < 1.e-12 && std::abs(neutralR + 0.05) < 1.e-12,
            "zero-percent sauce is exactly transparent");
    std::cout << "PASS: bass-safe mid-side imaging, anti-phase protection, neutral signal\n";

    MasteringEngine dsp;
    dsp.prepare(rate, 2, lookahead);
    MasterParameters p;
    dsp.setParameters(p);
    dsp.beginBlock();

    // Neutral controls must be transparent, apart from the precisely specified delay.
    std::vector<float> impulse(lookahead + 512, 0.0f), output(impulse.size());
    impulse[0] = 0.2f;
    for (std::size_t i=0;i<impulse.size();++i) {
        const auto s = dsp.processFrame(impulse[i], impulse[i]);
        output[i] = s.left;
        require(std::isfinite(s.left) && std::isfinite(s.right), "finite impulse output");
    }
    for (int i=0;i<lookahead;i++) require(std::abs(output[static_cast<size_t>(i)]) < 1.e-6f, "lookahead delay initial silence");
    require(std::abs(output[static_cast<size_t>(lookahead)] - .2f) < 1.e-4f, "neutral impulse preserved at reported delay");
    require(dsp.getLookaheadSamples() == lookahead, "reported latency equals configured lookahead");

    // A peak above the ceiling must be contained even in a zero-attack transient.
    dsp.reset();
    p.ceilingDb = -1.0;
    dsp.setParameters(p);
    double maxObserved = 0;
    for (int i=0;i<220000;i++) {
        const float x = i%2048 < 9 ? 2.4f : 0.0f;
        const auto v = dsp.processFrame(x,-x);
        maxObserved = std::max(maxObserved,static_cast<double>(std::max(std::abs(v.left),std::abs(v.right))));
        require(std::isfinite(v.left) && std::isfinite(v.right), "finite hard transient output");
    }
    require(maxObserved <= std::pow(10.0,-1.0/20.0)+1.e-6, "oversampled processing ceiling is respected");
    require(maxObserved > .6, "limiter does not mute the transient");
    std::cout << "Limiting max: " << maxObserved << " at -1 dBFS ceiling\n";

    // Feed an isolated transient into a settled neutral limiter. With 1.5ms
    // lookahead, release must not begin before its delayed output arrives.
    dsp.prepare(rate, 2, lookahead);
    p = {};
    dsp.setParameters(p);
    for (int i=0;i<2000;++i) dsp.processFrame(0,0);
    dsp.processFrame(2.0f, -2.0f);
    StereoFrame impulseAfterDelay;
    for (int i=0;i<lookahead;++i)
        impulseAfterDelay=dsp.processFrame(0,0);
    const double headroom = detail::dbToLinear(-1.0) * 0.977237220956;
    require(std::abs(impulseAfterDelay.left) <= headroom + 2.e-4,
            "limiter holds attenuation through delayed peak");
    require(std::abs(impulseAfterDelay.left + impulseAfterDelay.right) < 1.e-5,
            "held limiter stereo phase remains linked");

    // Two musical 2nd-order shelves: measure *steady-state* response after smoothing.
    const auto shelfGain = [rate, lookahead](double hz, double lo, double hi) {
        MasteringEngine filter;
        filter.prepare(rate, 2, lookahead);
        MasterParameters eq;
        eq.lowsDb = lo; eq.highsDb = hi;
        filter.setParameters(eq);
        double inPower = 0.0, outPower = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 0.65); ++i) {
            const float x = static_cast<float>(0.04 * std::sin(i * 2.0 * detail::pi * hz / rate));
            const auto v = filter.processFrame(x, x);
            if (i >= static_cast<int>(rate * 0.40)) {
                inPower += x*x;
                outPower += v.left*v.left;
            }
        }
        return 10.0 * std::log10(outPower / inPower);
    };
    require(shelfGain(35.0, 6, 0) > 5.5, "low shelf adds six dB in bass");
    require(std::abs(shelfGain(1000.0, 6, 0)) < 0.25, "low shelf preserves mids");
    require(shelfGain(18000.0, 0, 6) > 5.0, "high shelf adds six dB in upper treble");
    require(std::abs(shelfGain(1000.0, 0, 6)) < 0.25, "high shelf preserves mids");

    dsp.prepare(rate, 2, lookahead);
    p = {};
    p.glue = 1.0;
    dsp.setParameters(p);
    dsp.beginBlock();
    for (int i=0;i<static_cast<int>(rate*0.5);++i) {
        const float x = static_cast<float>(0.7 * std::sin(2*detail::pi*120*i/rate));
        dsp.processFrame(x, x);
    }
    require(dsp.getBlockGainReductionDb() > 2, "stereo-linked glue engages when requested");

    // Bypass retains exactly the same lookahead delay without attenuating peaks.
    dsp.prepare(rate, 2, lookahead);
    p = {}; p.bypass = true;
    dsp.setParameters(p);
    for (int i=0;i<static_cast<int>(rate*0.3);++i) dsp.processFrame(0,0);
    dsp.processFrame(1.3f, -1.3f);
    StereoFrame recovered;
    for (int i=0;i<lookahead;++i) recovered = dsp.processFrame(0,0);
    require(std::abs(recovered.left-1.3f) < 1.e-5f, "bypass is dry and latency aligned");
    require(std::abs(recovered.right+1.3f) < 1.e-5f, "bypass preserves stereo phase");

    // Long-running stereo stress: automated controls, mono compatibility, and NaN guard.
    dsp.prepare(rate, 2, lookahead);
    for (int i=0;i<250000;i++) {
        if (i % 4096 == 0) {
            const auto k = static_cast<double>(i / 4096 % 101)/100.0;
            p.drive=k; p.glue=k; p.dynamics=k; p.sauce=k; p.transients=k;
            p.lowsDb=-6+12*k; p.highsDb=6-12*k;
            p.inputDb=8-12*k; p.outputDb=4-8*k;
            p.ceilingDb=-3+2.9*k; p.bypass=(i/4096)%7==0;
            dsp.setParameters(p);
        }
        const float a = static_cast<float>(1.4*std::sin(i*2*3.14159265*240.0/rate));
        const auto v=dsp.processFrame(a,static_cast<float>(a*0.16));
        require(std::isfinite(v.left) && std::isfinite(v.right), "no nonfinite output during automation stress");
        require(std::abs(v.left) < 10.0f && std::abs(v.right) < 10.0f, "numerically stable state");
    }
    dsp.prepare(rate,1,lookahead);
    p={}; dsp.setParameters(p);
    for(int i=0;i<10000;i++) {
        const auto v=dsp.processFrame(i==0?0.2f:0,0);
        require(std::isfinite(v.left), "mono finite");
    }
    std::cout << "PASS: impulse, limiter, automation, stereo, mono tests\n";

    // Offline CPU sanity probe (time is recorded, not treated as a universal
    // benchmark across shared CI runners). Real-time ratio uses 4x oversampling:
    // rate is the actual processing rate of processFrame(), not the host rate.
    MasteringEngine bench;
    bench.prepare(rate, 2, lookahead);
    MasterParameters busy;
    busy.drive=.8; busy.glue=.65; busy.dynamics=.55;
    busy.lowsDb=3; busy.highsDb=3; busy.sauce=.8; busy.transients=.55;
    bench.setParameters(busy);
    constexpr int frames = 192000;
    const auto start = std::chrono::steady_clock::now();
    volatile float sink = 0.0f;
    for (int i=0;i<frames;++i) {
        const float a = static_cast<float>(0.3*std::sin(i*2.0*detail::pi*213.0/rate));
        const auto v=bench.processFrame(a,-a*.72f);
        require(std::isfinite(v.left) && std::isfinite(v.right), "busy DSP stays finite");
        sink = sink + v.left * 1.e-9f;
    }
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    const double audioSeconds = static_cast<double>(frames)/rate;
    std::cout << "DSP CPU probe: " << elapsed*1000.0 << " ms / "
              << audioSeconds*1000.0 << " ms audio ("
              << elapsed/audioSeconds*100.0 << "% one core realtime), sink="
              << sink << "\n";
    require(elapsed < audioSeconds*8.0, "DSP performance pathological slowdown");
}
