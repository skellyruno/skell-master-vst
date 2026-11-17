#include "../Source/DSP/MasteringEngine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

static void require(bool ok, const char* message) {
    if(!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main() {
    using namespace skelly::experimental;
    constexpr double rate = 192000.0;
    constexpr double pi = 3.1415926535897932384626433832795;
    FiveBandColorPrototype color;
    color.prepare(rate);

    // All factory presets remain unchanged: wet=0 or zero band intensity
    // must reproduce the input exactly while the filter history advances.
    FiveBandColorSettings zero;
    FiveBandColorSettings noColor;
    noColor.wet = 1.0;
    for(int i=0;i<40000;++i) {
        double l=0.4*std::sin(2.0*pi*121.0*i/rate);
        double r=-0.22*std::sin(2.0*pi*2501.0*i/rate);
        const double originalL=l, originalR=r;
        color.process(l,r,i%2==0 ? zero : noColor);
        require(l==originalL && r==originalR,
                "experimental five-band zero color must be bit-exact dry");
    }
    std::cout << "PASS: bit-exact neutral path with continuously updated filters\n";

    // This is a stereo-linked tonal stage: mono and opposite polarity must
    // remain aligned even during sustained high-band harmonic coloration.
    FiveBandColorSettings full;
    full.wet=0.75;
    full.intensity={0.38,0.55,0.64,0.72,0.84};
    color.reset();
    double worstMono=0.0;
    for(int i=0;i<90000;++i) {
        const double x=0.27*std::sin(2.0*pi*85.0*i/rate)
                      +0.08*std::sin(2.0*pi*13290.0*i/rate);
        double l=x,r=x;
        color.process(l,r,full);
        worstMono=std::max(worstMono,std::abs(l-r));
        require(std::isfinite(l) && std::isfinite(r),
                "five-band mono pass must stay finite");
    }
    require(worstMono<1.0e-11,"five-band color changed mono symmetry");
    color.reset();
    double worstAnti=0.0;
    for(int i=0;i<90000;++i) {
        const double x=0.31*std::sin(2.0*pi*6700.0*i/rate)
                      +0.17*std::sin(2.0*pi*113.0*i/rate);
        double l=x,r=-x;
        color.process(l,r,full);
        worstAnti=std::max(worstAnti,std::abs(l+r));
        require(std::isfinite(l)&&std::isfinite(r),"five-band anti-phase not finite");
    }
    require(worstAnti<1.0e-11,"five-band color distorted anti-phase symmetry");
    std::cout << "PASS: mono and anti-phase imaging at high coloration\n";

    // Activate ONLY the highest band. 60 Hz bass should remain close to
    // neutral; 7 kHz should measurably receive extra harmonic density.
    const auto gainAt=[rate,pi](double hz) {
        FiveBandColorPrototype proc;
        proc.prepare(rate);
        FiveBandColorSettings highOnly;
        highOnly.wet=1.0;
        highOnly.intensity[4]=1.0;
        double dryPower=0.0, wetPower=0.0;
        for (int i=0;i<static_cast<int>(rate*0.45);++i) {
            const double x=0.15*std::sin(2.0*pi*hz*i/rate);
            double l=x,r=x;
            proc.process(l,r,highOnly);
            if(i>static_cast<int>(rate*0.20)) {
                dryPower+=x*x;
                wetPower+=l*l;
            }
        }
        return std::sqrt(wetPower/dryPower);
    };
    const double bassGain=gainAt(60.0);
    const double upperGain=gainAt(7000.0);
    require(std::abs(bassGain-1.0)<0.025,
            "high-band coloration altered deep bass excessively");
    require(upperGain>1.01 && upperGain<1.45,
            "high-band coloration must be audible but bounded");
    std::cout << "PASS: isolated band response bass " << bassGain
              << " 7kHz " << upperGain << "\n";

    // A preset/load or DAW automation step must not create a full-scale
    // instantaneous discontinuity on an otherwise constant input.
    // This exercises both a switch-on AND a switch-off with running states.
    FiveBandColorPrototype smoothed;
    smoothed.prepare(rate);
    FiveBandColorSettings off, on=full;
    double previous=0.27;
    for(int i=0;i<6000;++i) {
        double l=0.27,r=0.27;
        smoothed.process(l,r,off);
        require(l==0.27 && r==0.27,
                "zero-mix filter tracking must be exactly dry");
    }
    double largestStep=0.0, steadyColor=0.0;
    for(int i=0;i<static_cast<int>(rate * 0.16);++i) {
        double l=0.27,r=0.27;
        smoothed.process(l,r,on);
        largestStep=std::max(largestStep,std::abs(l-previous));
        previous=l;
        steadyColor=std::abs(l-0.27);
    }
    require(steadyColor>0.005,
            "long-enough ramp must audibly engage the coloration");
    for(int i=0;i<static_cast<int>(rate * 0.4);++i) {
        double l=0.27,r=0.27;
        smoothed.process(l,r,off);
        largestStep=std::max(largestStep,std::abs(l-previous));
        previous=l;
    }
    require(largestStep<0.002,
            "five-band parameter automation caused a discontinuity");
    require(std::abs(previous-0.27)<1.e-8,
            "smoothing release must recover bit-exact dry input");
    std::cout << "PASS: smooth color engagement/release without DC clicks; largest step "
              << largestStep << "\\n";

    // Hostile automation, finite extreme input and long silent tails must
    // not poison any of the eight internal crossover states.
    color.reset();
    FiveBandColorSettings changing=full;
    double maxSample=0.0;
    for(int i=0;i<250000;++i) {
        if(i%997==0) {
            changing.wet=((i/997)%2)==0 ? 0.0 : 1.0;
            for(std::size_t k=0;k<5;++k)
                changing.intensity[k]=((i/997+static_cast<int>(k))%4)*0.32;
        }
        const double x=(i%907==0 ? 2.3 : 0.3*std::sin(2*pi*97*i/rate));
        double l=x, r=-0.37*x;
        color.process(l,r,changing);
        require(std::isfinite(l)&&std::isfinite(r),
                "full five-band automation produced invalid samples");
        maxSample=std::max(maxSample,std::max(std::abs(l),std::abs(r)));
    }
    require(maxSample<4.0,"unbounded experimental five-band coloration");
    // Invalid values are neutral *targets*, not abrupt hard bypass
    // switches: after active coloration they must smoothly fade to dry.
    changing.wet=std::numeric_limits<double>::quiet_NaN();
    changing.intensity[0]=std::numeric_limits<double>::infinity();
    double badL=0.2,badR=-0.1;
    color.process(badL,badR,changing);
    require(std::isfinite(badL) && std::isfinite(badR),
            "nonfinite automation must not destabilize the fade");
    for(int i=0;i<static_cast<int>(rate * 0.5);++i) {
        badL=0.2; badR=-0.1;
        color.process(badL,badR,changing);
    }
    require(std::abs(badL-0.2)<1.e-8 && std::abs(badR+0.1)<1.e-8,
            "nonfinite automation must recover dry after smoothing");
    color.reset();
    badL=0.2;badR=-0.1;
    color.process(badL,badR,changing);
    require(badL==0.2 && badR==-0.1,
            "nonfinite automation must be exactly neutral from reset");
    for(int i=0;i<250000;++i) {
        double l=0.0,r=0.0;
        color.process(l,r,full);
        require(std::isfinite(l)&&std::isfinite(r),
                "five-band silence recovery became non-finite");
    }
    std::cout << "PASS: full 5-band automation, malformed settings and silent tails\n";

    // Production topology regression: the Deuce five-band stage must be
    // inside MasteringEngine BEFORE its internal lookahead limiter. A mirror
    // of the old post-limiter order deliberately exceeds the same ceiling.
    {
        constexpr double engineRate = 192000.0;
        constexpr int lookahead = 288; // 1.5 ms at the 4x processing rate
        const double ceiling = skelly::detail::dbToLinear(-1.0);

        skelly::MasteringEngine preLimiterEngine;
        skelly::MasteringEngine oldOrderEngine;
        preLimiterEngine.prepare(engineRate, 2, lookahead);
        oldOrderEngine.prepare(engineRate, 2, lookahead);

        skelly::MasterParameters deuceParams;
        deuceParams.deuceMatch = true;
        deuceParams.ceilingDb = -1.0;
        preLimiterEngine.setParameters(deuceParams);

        skelly::MasterParameters neutralParams;
        neutralParams.ceilingDb = -1.0;
        oldOrderEngine.setParameters(neutralParams);

        FiveBandColorPrototype postLimiterMirror;
        postLimiterMirror.prepare(engineRate);
        FiveBandColorSettings referenceColor;
        referenceColor.wet = 0.75;
        referenceColor.intensity = {0.38, 0.55, 0.64, 0.72, 0.84};

        double preLimiterPeak = 0.0;
        double oldOrderPeak = 0.0;
        double preLimiterEnergy = 0.0;
        constexpr int frames = static_cast<int>(engineRate * 0.25);
        for (int i = 0; i < frames; ++i) {
            const float input = static_cast<float>(
                1.2 * std::sin(2.0 * pi * 1000.0 * i / engineRate));
            const auto pre = preLimiterEngine.processFrame(input, input);
            const auto neutral = oldOrderEngine.processFrame(input, input);

            double postL = neutral.left;
            double postR = neutral.right;
            postLimiterMirror.process(postL, postR, referenceColor);

            require(std::isfinite(pre.left) && std::isfinite(pre.right)
                    && std::isfinite(postL) && std::isfinite(postR),
                    "pre/post-limiter five-band routing must remain finite");
            if (i > static_cast<int>(engineRate * 0.15)) {
                preLimiterPeak = std::max(preLimiterPeak,
                    std::max(std::abs(static_cast<double>(pre.left)),
                             std::abs(static_cast<double>(pre.right))));
                oldOrderPeak = std::max(oldOrderPeak,
                    std::max(std::abs(postL), std::abs(postR)));
                preLimiterEnergy += static_cast<double>(pre.left) * pre.left;
            }
        }
        require(preLimiterPeak <= ceiling + 1.0e-5,
                "pre-limiter Deuce coloration escaped the internal sample ceiling");
        require(oldOrderPeak > ceiling + 1.0e-4,
                "post-limiter mirror did not expose the old color-stage ordering");
        require(preLimiterEnergy > 1.0,
                "pre-limiter Deuce mode unexpectedly muted the programme");
        std::cout << "PASS: Deuce five-band coloration precedes internal limiter; "
                  << "pre-limiter peak " << preLimiterPeak
                  << ", old-order post-limiter peak " << oldOrderPeak << "\\n";
    }

    // Bypass must disable the optional nonlinear color stage even if the
    // factory selection/automation still requests Deuce Match. Exercise real
    // audio through bypass and reactivation: the crossover filters must keep
    // tracking while wet=0, and the settled output must rejoin an always-active
    // reference engine without a stale-state transient or gain discontinuity.
    {
        constexpr double engineRate = 192000.0;
        constexpr int lookahead = 288;
        constexpr int warmFrames = static_cast<int>(engineRate * 0.25);
        constexpr int transitionFrames = static_cast<int>(engineRate * 0.25);
        constexpr int settleStart = static_cast<int>(engineRate * 0.15);

        skelly::MasteringEngine bypassEngine;
        skelly::MasteringEngine alwaysOnEngine;
        bypassEngine.prepare(engineRate, 2, lookahead);
        alwaysOnEngine.prepare(engineRate, 2, lookahead);

        skelly::MasterParameters p;
        p.deuceMatch = true;
        p.bypass = true;
        bypassEngine.setParameters(p);
        require(!bypassEngine.isPreLimiterColorEnabled(),
                "latency-aligned bypass must disable pre-limiter Deuce coloration");

        auto active = p;
        active.bypass = false;
        alwaysOnEngine.setParameters(active);
        require(alwaysOnEngine.isPreLimiterColorEnabled(),
                "always-active reference must enable Deuce coloration");

        // A quiet stereo multitone keeps every downstream limiter/clipper
        // below threshold, isolating the color-stage state transition.
        for (int i = 0; i < warmFrames; ++i) {
            const double t = static_cast<double>(i) / engineRate;
            const float left = static_cast<float>(
                0.32 * std::sin(2.0 * pi * 997.0 * t)
                + 0.07 * std::sin(2.0 * pi * 83.0 * t));
            const float right = static_cast<float>(
                0.27 * std::sin(2.0 * pi * 997.0 * t + 0.23)
                - 0.05 * std::sin(2.0 * pi * 127.0 * t));
            const auto bypassOut = bypassEngine.processFrame(left, right);
            const auto activeOut = alwaysOnEngine.processFrame(left, right);
            require(std::isfinite(bypassOut.left) && std::isfinite(bypassOut.right)
                    && std::isfinite(activeOut.left) && std::isfinite(activeOut.right),
                    "bypassed/active Deuce engines must remain finite during warm-up");
        }

        p.bypass = false;
        bypassEngine.setParameters(p);
        require(bypassEngine.isPreLimiterColorEnabled(),
                "leaving bypass must restore the selected Deuce coloration");

        double maximumStep = 0.0;
        double maximumSample = 0.0;
        double settledDifference = 0.0;
        float previousLeft = 0.0f, previousRight = 0.0f;
        bool havePrevious = false;
        for (int i = 0; i < transitionFrames; ++i) {
            const int n = warmFrames + i;
            const double t = static_cast<double>(n) / engineRate;
            const float left = static_cast<float>(
                0.32 * std::sin(2.0 * pi * 997.0 * t)
                + 0.07 * std::sin(2.0 * pi * 83.0 * t));
            const float right = static_cast<float>(
                0.27 * std::sin(2.0 * pi * 997.0 * t + 0.23)
                - 0.05 * std::sin(2.0 * pi * 127.0 * t));
            const auto reenabled = bypassEngine.processFrame(left, right);
            const auto reference = alwaysOnEngine.processFrame(left, right);
            require(std::isfinite(reenabled.left) && std::isfinite(reenabled.right)
                    && std::isfinite(reference.left) && std::isfinite(reference.right),
                    "Deuce reactivation produced a non-finite sample");

            maximumSample = std::max(maximumSample,
                std::max(std::abs(static_cast<double>(reenabled.left)),
                         std::abs(static_cast<double>(reenabled.right))));
            if (havePrevious) {
                maximumStep = std::max(maximumStep,
                    std::max(std::abs(static_cast<double>(reenabled.left - previousLeft)),
                             std::abs(static_cast<double>(reenabled.right - previousRight))));
            }
            previousLeft = reenabled.left;
            previousRight = reenabled.right;
            havePrevious = true;

            if (i >= settleStart) {
                settledDifference = std::max(settledDifference,
                    std::max(std::abs(static_cast<double>(reenabled.left - reference.left)),
                             std::abs(static_cast<double>(reenabled.right - reference.right))));
            }
        }
        require(maximumSample < 0.8,
                "Deuce reactivation created an unexpected gain spike");
        require(maximumStep < 0.05,
                "Deuce reactivation created an abrupt sample discontinuity");
        require(settledDifference < 2.0e-5,
                "warm bypass crossover history did not converge to the always-active output");

        p.deuceMatch = false;
        bypassEngine.setParameters(p);
        require(!bypassEngine.isPreLimiterColorEnabled(),
                "turning off Deuce Match must disable pre-limiter coloration");
    }
    std::cout << "PASS: Deuce bypass warm-history, smooth reactivation and active-output convergence\\n";

    // Meaningful measured CPU data (machine-specific, not a universal spec).
    color.reset();
    volatile double sink=0.0;
    const auto t0=std::chrono::steady_clock::now();
    for(int i=0;i<192000;++i) {
        double l=0.24*std::sin(2*pi*219*i/rate);
        double r=-0.18*std::sin(2*pi*1118*i/rate);
        color.process(l,r,full);
        sink=sink+l*1.0e-10;
    }
    const double seconds=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-t0).count();
    std::cout << "Five-band prototype benchmark: " << seconds*1000
              << " ms processing / 1000 ms program at 4x, ratio "
              << seconds*100.0 << "% one core realtime, sink=" << sink << "\n";
    require(seconds<6.0,"experimental five-band CPU pathological slowdown");
}
