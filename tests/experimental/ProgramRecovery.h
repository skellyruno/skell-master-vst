#pragma once
#include <algorithm>
#include <cmath>
namespace skelly {
// Original experiment: measure how continuously the peak detector requests
// reduction, rather than treating one brief peak like a sustained overload.
// No change to lookahead, attack or final peak guards. Not production-tuned.
class ProgramRecovery {
public:
    void prepare(double rate) noexcept {
        rate=std::max(8000.,rate);
        densityA=1.-std::exp(-1./(.075*rate));
        fast=std::exp(-1./(.028*rate));
        slow=std::exp(-1./(.180*rate));
        reset();
    }
    void reset() noexcept { density=0.; }
    double coefficient(double wanted,double gain) noexcept {
        const double active=std::clamp((1.-wanted)*8.,0.,1.);
        density+=densityA*(active-density);
        if(density<1.e-28) density=0.;
        const double depth=std::clamp((1.-gain)*3.,0.,1.);
        const double weight=std::clamp(.65*depth+.75*density,0.,1.);
        return fast+(slow-fast)*weight;
    }
private:
    double density=0.,densityA=0.,fast=0.,slow=0.;
};
}
