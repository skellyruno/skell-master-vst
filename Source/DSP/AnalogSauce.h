#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace skelly {
// Original bounded parallel analog color: even/odd rational residuals,
// pre-emphasis band, DC rejection, and linked level-dependent headroom.
// Designed for the existing 4x engine; no delay, allocation or vendor tables.
class AnalogSauce {
public:
    void prepare(double rate) noexcept {
        rate=std::max(8000.,rate);
        const auto pole=[rate](double hz) {return 1.-std::exp(-6.2831853071795864769*hz/rate);};
        bass=pole(170.); air=pole(5800.); dc=pole(45.); tone=pole(8500.);
        envA=1.-std::exp(-1./(.045*rate)); reset();
    }
    void reset() noexcept { states={}; power=0.; }
    void process(double& l,double& r,double amount) noexcept {
        const double x=std::clamp(amount,0.,1.);
        const double peak=std::max(std::abs(l),std::abs(r));
        power+=envA*(peak*peak-power); tiny(power);
        const double drive=2.6/(1.+1.2*power);
        const double a=shape(l,states[0],drive), b=shape(r,states[1],drive);
        if(x<=1.e-8) return;
        l+=x*a; r+=x*b;
    }
private:
    struct State { double lo=0., hi=0., bias=0., smooth=0.; };
    static void tiny(double& x) noexcept {if(std::abs(x)<1.e-28)x=0.;}
    static double low(double x,double a,double& z) noexcept {z+=a*(x-z);tiny(z);return z;}
    double shape(double x,State& s,double drive) const noexcept {
        const double band=low(x-low(x,bass,s.lo),air,s.hi);
        const double u=band*drive;
        const double q=u*u;
        // Positive even harmonic and gentle odd-order soft compression.
        // Both vanish faster than linearly at low input levels.
        const double residual=(.17*q-.12*u*q)/(1.+q)/drive;
        const double ac=residual-low(residual,dc,s.bias);
        return low(ac,tone,s.smooth);
    }
    std::array<State,2> states{};
    double bass=0.,air=0.,dc=0.,tone=0.,envA=0.,power=0.;
};
}
