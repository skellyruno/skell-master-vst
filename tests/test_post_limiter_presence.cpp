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
    // This filter runs after downsampling at the host rate, not at 4x.
    for(double rate : {44100.0,48000.0,96000.0}) {
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
    // The measured Deuce contour correction is a fixed +0.65dB bell at
    // 2.5kHz, Q=1.0. It is opt-in with Deuce Match and has no host parameter.
    using skelly::experimental::PostLimiterMidPresencePrototype;
    for(double rate : {44100.0,48000.0,96000.0}) {
        const auto gainAt=[&](double freq) {
            PostLimiterMidPresencePrototype p; p.prepare(rate);
            double dryEnergy=0.0, wetEnergy=0.0;
            for(int i=0;i<static_cast<int>(rate*.5);++i) {
                const double original=.075*std::sin(2*pi*freq*i/rate);
                double l=original,r=-original;
                p.process(l,r,1.0);
                require(std::isfinite(l)&&std::isfinite(r),
                        "Deuce mid-presence filter became nonfinite");
                require(std::abs(l+r)<1.e-12,
                        "Deuce mid-presence changed antiphase stereo symmetry");
                if(i>static_cast<int>(rate*.2)){
                    dryEnergy+=original*original;
                    wetEnergy+=l*l;
                }
            }
            return 10.0*std::log10(wetEnergy/dryEnergy);
        };
        const double low=gainAt(100.0);
        const double mid=gainAt(2500.0);
        const double high=gainAt(18000.0);
        require(std::abs(low)<0.04,
                "2.5k contour correction changed low-frequency master");
        require(std::abs(mid-0.65)<0.04,
                "Deuce mid-presence must deliver +0.65dB at 2.5kHz");
        require(std::abs(high)<0.08,
                "2.5k contour correction changed the top octave");
        std::cout<<"PASS: Deuce 2.5kHz +0.65dB at host rate "<<rate
                 <<"; bass "<<low<<"dB, air "<<high<<"dB\n";
    }
    constexpr double midRate=96000.0;
    PostLimiterMidPresencePrototype midClean;
    midClean.prepare(midRate);
    for(int i=0;i<25000;++i){
        double l=.12*std::sin(2*pi*8400.0*i/midRate),r=-l*.72;
        const double dryL=l,dryR=r;
        midClean.process(l,r,0.0);
        require(l==dryL && r==dryR,
                "Deuce mid-presence 0% mix must be bit-exact neutral");
    }
    double midFirstDifference=0.0,midLateDifference=0.0;
    for(int i=0;i<static_cast<int>(midRate*.25);++i){
        const double sample=.27*std::sin(2*pi*2500.0*i/midRate);
        double l=sample,r=sample;
        midClean.process(l,r,1.0);
        if(i==0) midFirstDifference=std::abs(l-sample);
        if(i>static_cast<int>(midRate*.22))
            midLateDifference=std::max(midLateDifference,std::abs(l-sample));
    }
    require(midFirstDifference<0.001,
            "Deuce mid-presence automation produced a sudden sample discontinuity");
    require(midLateDifference>0.003,
            "Deuce mid-presence filter never meaningfully engaged");
    for(int i=0;i<static_cast<int>(midRate*.5);++i){
        const double sample=.27*std::sin(2*pi*2500.0*i/midRate);
        double l=sample,r=sample;
        midClean.process(l,r,0.0);
        if(i==static_cast<int>(midRate*.5)-1)
            require(l==sample&&r==sample,
                    "Deuce mid-presence must recover exact dry output");
    }
    midClean.reset();
    double midBadL=std::numeric_limits<double>::infinity();
    double midBadR=std::numeric_limits<double>::quiet_NaN();
    midClean.process(midBadL,midBadR,std::numeric_limits<double>::quiet_NaN());
    require(midBadL==0.0 && midBadR==0.0,
            "Deuce mid-presence invalid audio/automation must recover silence");
    volatile double midSink=0.0;
    const auto midStart=std::chrono::steady_clock::now();
    for(int i=0;i<midRate;++i){
        const double sample=.3*std::sin(2*pi*1021.0*i/midRate);
        double l=sample,r=-sample*.48;
        midClean.process(l,r,1.0);
        midSink=midSink+l*1.e-9;
    }
    const double midElapsed=std::chrono::duration<double>(
        std::chrono::steady_clock::now()-midStart).count();
    require(midElapsed<6.0,"Deuce mid-presence CPU pathological slowdown");
    std::cout<<"PASS: Deuce mid-presence neutral bypass, smoothed automation, finite recovery; CPU "
             <<midElapsed*1000<<"ms / 1000ms at 96kHz, sink="<<midSink<<"\n";

}
