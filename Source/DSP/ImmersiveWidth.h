#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace skelly {
// Parallel residuals from existing stereo content; no synthetic stereo,
// no decorrelation delays. Mid remains unchanged. Local and global energy
// protection keep bass and already diffuse/anti-phase material controlled.
class ImmersiveWidth {
public:
    void prepare(double rate) noexcept {
        rate=std::max(8000.,rate);
        constexpr double hz[4]={230.,720.,2100.,6200.};
        for(int i=0;i<4;++i) a[i]=1.-std::exp(-6.2831853071795864769*hz[i]/rate);
        env=1.-std::exp(-1./(.045*rate));
        fast=1.-std::exp(-1./(.008*rate));
        slow=1.-std::exp(-1./(.100*rate));
        reset();
    }
    void reset() noexcept { bands={}; mp=sp=fm=sm=0.; }
    void process(double& l,double& r,double amount) noexcept {
        const double m=.5*(l+r),s=.5*(l-r);
        track(mp,m*m,env); track(sp,s*s,env);
        track(fm,m*m,fast); track(sm,m*m,slow);
        const double global=protect(mp,sp);
        const double transient=1.-.16*std::clamp((fm-sm)/(sm+1.e-8),0.,2.);
        constexpr double weights[4]={.64,.68,.57,.44};
        double detail=0.;
        for(int i=0;i<4;++i) {
            auto& b=bands[i];
            const double mh=high(m,a[i],b.ml,b.mt),sh=high(s,a[i],b.sl,b.st);
            track(b.mp,mh*mh,env); track(b.sp,sh*sh,env);
            detail+=weights[i]*sh*protect(b.mp,b.sp);
        }
        if(amount<=1.e-8) return;
        const double x=std::clamp(amount,0.,1.);
        const double spatial=s+x*(.60+.40*x)*global*transient*detail;
        l=m+spatial; r=m-spatial;
    }
private:
    struct Band { double ml=0.,mt=0.,sl=0.,st=0.,mp=0.,sp=0.; };
    static void track(double& v,double x,double a) noexcept { v+=a*(x-v); if(std::abs(v)<1.e-28)v=0.; }
    static double high(double x,double a,double& lo,double& tail) noexcept {
        track(lo,x,a);const double h=x-lo;track(tail,h,a);return h-tail;
    }
    static double protect(double m,double s) noexcept {
        const double c=(m-s)/(m+s+1.e-16);
        return std::clamp((c+.04)/.72,0.,1.);
    }
    std::array<Band,4> bands{};
    std::array<double,4> a{};
    double env=0.,fast=0.,slow=0.,mp=0.,sp=0.,fm=0.,sm=0.;
};
}
