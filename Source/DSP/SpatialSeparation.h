#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace skelly {
// Original mastering spatial stage. Parallel two-pole high-pass residuals,
// local M/S power protection, transient-dependent side detail. No decorrelation
// delay, no generated side signal, no vendor coefficients. Mid is unchanged.
class SpatialSeparation {
public:
    void prepare(double rate) noexcept {
        rate=std::max(8000.,rate);
        constexpr double pi=3.14159265358979323846;
        constexpr double hz[3]={260.,1450.,5200.};
        for(int i=0;i<3;++i) a[i]=1.-std::exp(-2.*pi*hz[i]/rate);
        envelope=1.-std::exp(-1./(.045*rate));
        fast=1.-std::exp(-1./(.008*rate));
        slow=1.-std::exp(-1./(.095*rate));
        reset();
    }
    void reset() noexcept {
        bands={}; midPower=sidePower=fastMid=slowMid=0.;
    }
    void process(double& l,double& r,double amount) noexcept {
        const double m=.5*(l+r), s=.5*(l-r);
        midPower+=envelope*(m*m-midPower);
        sidePower+=envelope*(s*s-sidePower);
        fastMid+=fast*(m*m-fastMid);
        slowMid+=slow*(m*m-slowMid);
        suppressTiny(midPower); suppressTiny(sidePower);
        suppressTiny(fastMid); suppressTiny(slowMid);
        const double global=protection(midPower,sidePower);
        const double onset=std::clamp((fastMid-slowMid)/(slowMid+1.e-8),0.,2.);
        const double attack=1.-.18*onset;
        double detail=0.;
        constexpr double weight[3]={.36,.54,.72};
        for(int i=0;i<3;++i) {
            auto& b=bands[i];
            const double mh=high(m,a[i],b.ml,b.mt);
            const double sh=high(s,a[i],b.sl,b.st);
            b.mp+=envelope*(mh*mh-b.mp);
            b.sp+=envelope*(sh*sh-b.sp);
            suppressTiny(b.mp); suppressTiny(b.sp);
            detail+=weight[i]*sh*protection(b.mp,b.sp);
        }
        if(amount<=1.e-8) return;
        const double x=std::clamp(amount,0.,1.);
        const double spatial=s+x*(.65+.35*x)*global*attack*detail;
        l=m+spatial; r=m-spatial;
    }
private:
    struct Band { double ml=0.,mt=0.,sl=0.,st=0.,mp=0.,sp=0.; };
    static void suppressTiny(double& x) noexcept { if(std::abs(x)<1.e-28) x=0.; }
    static double high(double x,double a,double& low,double& tail) noexcept {
        low+=a*(x-low); const double h=x-low;
        tail+=a*(h-tail); const double y=h-tail;
        suppressTiny(low); suppressTiny(tail); return y;
    }
    static double protection(double m,double s) noexcept {
        // Reduce expansion as existing side power approaches center power.
        const double c=(m-s)/(m+s+1.e-16);
        return std::clamp((c+.08)/.68,0.,1.);
    }
    std::array<Band,3> bands{};
    std::array<double,3> a{};
    double envelope=0.,fast=0.,slow=0.,midPower=0.,sidePower=0.,fastMid=0.,slowMid=0.;
};
}
