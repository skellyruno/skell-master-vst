#pragma once
#include <algorithm>
#include <cmath>

namespace skelly {
// Slow, stereo-linked partial recovery of compressor attenuation. The ceiling
// is shared across both compressors, and the stage precedes the final limiter.
class CompressionMakeup {
public:
    static constexpr double maxGain=1.4125375446227544; // +3 dB total
    void prepare(double rate) noexcept {
        rate=std::max(8000.0,rate);
        rise=1.-std::exp(-1./(.180*rate));
        fall=1.-std::exp(-1./(.080*rate));
        reset();
    }
    void reset() noexcept { extra=0.; }
    double next(double glueGain,double dynamicsGain,double glue,double dynamics) noexcept {
        const auto loss=[](double gain,double amount) noexcept {
            const double w=std::clamp((amount-.30)/.30,0.,1.);
            if(w<=0.) return 0.;
            const double safe=std::clamp(gain,.25,1.);
            return w*(1./safe-1.);
        };
        const double g=loss(glueGain,glue),d=loss(dynamicsGain,dynamics);
        const double target=std::min(maxGain-1.,.65*(g+d+g*d));
        extra+=(target-extra)*(target>extra ? rise : fall);
        if(extra<1.e-12) extra=0.;
        return 1.+extra;
    }
private:
    double extra=0.,rise=0.,fall=0.;
};
}
