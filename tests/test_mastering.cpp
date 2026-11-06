#include "../Source/DSP/MasteringEngine.h"
#include <algorithm>
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

    // Two musical 2nd-order shelves: measure *steady-state* response after smoothing.
    const auto shelfGain = [](double hz, double lo, double hi) {
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
}
