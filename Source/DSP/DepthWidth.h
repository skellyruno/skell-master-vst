#pragma once
#include <algorithm>
#include <cmath>
#include <array>

namespace skelly {
// Original parallel high-pass side detail, not a vendor coefficient model.
// Mid is untouched. No artificial delay, phase inversion or synthetic stereo.
class DepthWidth {
public:
    void prepare(double rate) noexcept {
        rate=std::max(8000.0,rate);
        constexpr double pi=3.14159265358979323846;
        const double frequencies[3]={250.,1200.,5000.};
        for(int i=0;i<3;++i) coefficients[i]=1.-std::exp(-2.*pi*frequencies[i]/rate);
        fast=1.-std::exp(-1./(.008*rate));
        slow=1.-std::exp(-1./(.090*rate));
        band=1.-std::exp(-1./(.040*rate));
        reset();
    }
    void reset() noexcept {
        low.fill(0.); tail.fill(0.); midLow=midTail=0.;
        fastPower=slowPower=midBand=sideBand=0.;
    }
    // Width offset 0..0.6; globalProtection 0..1 from the existing linked detector.
    // Update history even at zero; return the original side bit-exactly.
    double process(double mid,double side,double offset,double globalProtection) noexcept {
        std::array<double,3> high{};
        for(int i=0;i<3;++i) {
            low[i]+=coefficients[i]*(side-low[i]);
            const double first=side-low[i];
            tail[i]+=coefficients[i]*(first-tail[i]);
            high[i]=first-tail[i];
            if(std::abs(low[i])<1.e-28) low[i]=0.;
            if(std::abs(tail[i])<1.e-28) tail[i]=0.;
        }
        midLow+=coefficients[1]*(mid-midLow);
        const double first=mid-midLow;
        midTail+=coefficients[1]*(first-midTail);
        const double mh=first-midTail;
        const double mp=mid*mid;
        fastPower+=fast*(mp-fastPower);
        slowPower+=slow*(mp-slowPower);
        midBand+=band*(mh*mh-midBand);
        sideBand+=band*(high[1]*high[1]-sideBand);
        if(std::abs(midLow)<1.e-28) midLow=0.;
        if(std::abs(midTail)<1.e-28) midTail=0.;
        if(fastPower<1.e-30) fastPower=0.;
        if(slowPower<1.e-30) slowPower=0.;
        if(midBand<1.e-30) midBand=0.;
        if(sideBand<1.e-30) sideBand=0.;
        if(offset<=1.e-6) return side;
        const double correlation=(midBand-sideBand)/(midBand+sideBand+1.e-12);
        const double bandProtection=std::clamp((correlation+.20)/.65,0.,1.);
        // Keep center attacks intact: reduce only the additional side gain.
        const double onset=std::clamp((fastPower-slowPower)/(slowPower+1.e-8),0.,2.);
        const double attackProtection=1.-.15*onset;
        const double detail=.60*high[0]+bandProtection*(.80*high[1]+1.05*high[2]);
        return side+std::clamp(offset,0.,.6)*std::clamp(globalProtection,0.,1.)*attackProtection*detail;
    }
private:
    std::array<double,3> coefficients{},low{},tail{};
    double midLow=0.,midTail=0.,fast=0.,slow=0.,band=0.;
    double fastPower=0.,slowPower=0.,midBand=0.,sideBand=0.;
};
}
