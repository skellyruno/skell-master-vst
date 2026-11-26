#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace skelly::experimental {
// Original spectral residual control inspired by the independent Spectral4
// and program-leveler research. Split only the added coloration, never the
// dry signal. Bright, high-energy material gets less extra upper-band color.
class SpectralColorGuard {
public:
    void prepare(double rate) noexcept {
        rate=std::max(8000.,rate);
        const auto pole=[rate](double seconds){return 1.-std::exp(-1./(seconds*rate));};
        band=1.-std::exp(-6.2831853071795864769*3600./rate);
        attack=pole(.008); release=pole(.120); smooth=pole(.025);
        reset();
    }
    void reset() noexcept { states={}; total=high=0.; gain=1.; }
    void process(double l,double r,double& dl,double& dr,double amount) noexcept {
        const double hl=l-low(l,states[0].input),hr=r-low(r,states[1].input);
        const double p=.5*(l*l+r*r),h=.5*(hl*hl+hr*hr);
        follow(total,p);follow(high,h);
        const double ratio=high/(total+1.e-12);
        const double bright=std::clamp((ratio-.06)/.28,0.,1.);
        const double active=std::clamp((total-.0025)/.0225,0.,1.);
        const double target=1.-.60*bright*active*std::clamp(amount,0.,1.);
        gain+=smooth*(target-gain);
        const double ll=low(dl,states[0].residual),rr=low(dr,states[1].residual);
        if(amount<=0.) return;
        dl=ll+gain*(dl-ll);dr=rr+gain*(dr-rr);
    }
    double currentGain() const noexcept {return gain;}
private:
    struct State{double input=0.,residual=0.;};
    std::array<State,2> states{};
    double band=0.,attack=0.,release=0.,smooth=0.,total=0.,high=0.,gain=1.;
    static void tiny(double& v) noexcept {if(std::abs(v)<1.e-28)v=0.;}
    double low(double x,double& s) const noexcept {s+=band*(x-s);tiny(s);return s;}
    void follow(double& s,double x) const noexcept {s+=(x-s)*(x>s?attack:release);tiny(s);}
};
}
