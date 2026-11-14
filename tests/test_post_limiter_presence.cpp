#include "../Source/DSP/PostLimiterPresencePrototype.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
static void require(bool ok,const char* message) {
    if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(EXIT_FAILURE);}
}
int main() {
    using skelly::experimental::PostLimiterPresencePrototype;
    constexpr double pi=3.1415926535897932384626433832795;
    for(double rate : {176400.0,192000.0,384000.0}) {
        const auto gainAt=[&](double freq) {
            PostLimiterPresencePrototype p; p.prepare(rate);
            double dryEnergy=0.0, wetEnergy=0.0;
            for(int i=0;i<static_cast<int>(rate*.5);++i) {
                const double original=.075*std::sin(2*pi*freq*i/rate);
                double l=original,r=-original;
                p.process(l,r,1.0);
                require(std::isfinite(l)&&std::isfinite(r),
                        "post-limiter presence filter became nonfinite");
                require(std::abs(l+r)<1.e-12,
                        "post-limiter presence changed antiphase stereo symmetry");
                if(i>static_cast<int>(rate*.2)){
                    dryEnergy+=original*original;
                    wetEnergy+=l*l;
                }
            }
            return 10.0*std::log10(wetEnergy/dryEnergy);
        };
        const double low=gainAt(100.0);
        const double presence=gainAt(5000.0);
        const double high=gainAt(18000.0);
        require(std::abs(low)<0.04,
                "5k presence bell changed the low-frequency master");
        require(std::abs(presence-2.0)<0.04,
                "post-limit presence filter must deliver +2dB at 5kHz");
        require(std::abs(high)<0.5,
                "post-limit presence bell brightened top octave excessively");
        std::cout<<"PASS: post-limit 5kHz +2dB at rate "<<rate
                 <<"; bass "<<low<<"dB, air "<<high<<"dB\n";
    }
    constexpr double rate=192000.0;
    PostLimiterPresencePrototype clean;
    clean.prepare(rate);
    for(int i=0;i<25000;++i){
        double l=0.12*std::sin(2*pi*8400.0*i/rate),r=-l*.72;
        const double dryL=l,dryR=r;
        clean.process(l,r,0.0);
        require(l==dryL && r==dryR,
                "post-limiter 0% mix must be bit-exact neutral");
    }
    // Abrupt DAW automation from 0 to 100% is smoothed; the filter
    // is already primed while bypassed (no stale-state clicks).
    double firstDifference=0.0,lateDifference=0.0;
    for(int i=0;i<static_cast<int>(rate*.25);++i){
        const double source=.27*std::sin(2*pi*5000.0*i/rate);
        double l=source,r=source;
        clean.process(l,r,1.0);
        if(i==0) firstDifference=std::abs(l-source);
        if(i>static_cast<int>(rate*.22))
            lateDifference=std::max(lateDifference,std::abs(l-source));
    }
    require(firstDifference<0.001,
            "presence control produced a sudden sample discontinuity");
    require(lateDifference>0.01,
            "presence filter never meaningfully engaged");
    for(int i=0;i<static_cast<int>(rate*.5);++i){
        const double source=.27*std::sin(2*pi*5000.0*i/rate);
        double l=source,r=source;
        clean.process(l,r,0.0);
        if(i==static_cast<int>(rate*.5)-1)
            require(l==source&&r==source,
                    "post-limit presence must recover exact dry output");
    }
    clean.reset();
    double badL=std::numeric_limits<double>::infinity();
    double badR=std::numeric_limits<double>::quiet_NaN();
    clean.process(badL,badR,std::numeric_limits<double>::quiet_NaN());
    require(badL==0.0 && badR==0.0,
            "invalid host automation and audio must recover neutral silence");

    volatile double sink=0.0;
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<192000;++i){
        const double sample=.3*std::sin(2*pi*1021.0*i/rate);
        double l=sample,r=-sample*.48;
        clean.process(l,r,1.0);
        sink=sink+l*1.e-9;
    }
    const double elapsed=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    require(elapsed<6.0,"post-limit EQ CPU pathological slowdown");
    std::cout<<"PASS: zero mix, click-safe automation, finite protection, mono/stereo; CPU "
             <<elapsed*1000<<"ms / 1000ms at 192kHz, sink="<<sink<<"\n";
}
