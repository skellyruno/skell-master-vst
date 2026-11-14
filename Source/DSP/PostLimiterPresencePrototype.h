#pragma once
// SKELL MASTER | optional post-limiter 5 kHz presence prototype.
// This is NOT connected to the production plugin. The user's master chain
// applies a +2 dB 5 kHz analog EQ *after* L2, unlike the current Skell Master
// pre-limiter high shelf. No Waves code, coefficients or proprietary presets
// are copied. A true-peak-safe final output stage is a separate release gate.
#include <algorithm>
#include <array>
#include <cmath>

namespace skelly::experimental {

class PostLimiterPresencePrototype {
public:
    void prepare(double sampleRate) noexcept {
        const double rate = std::isfinite(sampleRate)
            ? std::max(8000.0, sampleRate) : 192000.0;
        const double f = std::min(5000.0, rate * 0.40);
        // Ordinary RBJ bell; fixed 2 dB gain, Q=0.9. Coefficients are
        // calculated once in prepare(), never on the audio thread.
        const double a = std::pow(10.0, 2.0/40.0);
        const double omega = 6.28318530717958647692 * f / rate;
        const double alpha = std::sin(omega) / (2.0 * 0.9);
        const double cosw = std::cos(omega);
        const double scale = 1.0 / (1.0 + alpha/a);
        b0 = (1.0 + alpha*a)*scale;
        b1 = (-2.0*cosw)*scale;
        b2 = (1.0 - alpha*a)*scale;
        a1 = (-2.0*cosw)*scale;
        a2 = (1.0 - alpha/a)*scale;
        smoothStep = 1.0 - std::exp(-1.0/(rate*0.012));
        reset();
    }

    void reset() noexcept {
        for(auto& v:state) v.fill(0.0);
        wet = 0.0;
    }

    void process(double& left, double& right, double requestedWet) noexcept {
        const double dryL = sanitize(left), dryR = sanitize(right);
        // Both filter states track continuously, even at 0% mix; enabling
        // after a quiet passage cannot expose stale ringing.
        const double coloredL = filter(0, dryL);
        const double coloredR = filter(1, dryR);
        const double target = std::isfinite(requestedWet)
            ? std::clamp(requestedWet, 0.0, 1.0) : 0.0;
        wet += (target - wet)*smoothStep;
        if(target == 0.0 && wet < 1.0e-12) wet = 0.0;
        // Exact 0% pass-through is essential to protect legacy material.
        if(wet == 0.0) {left = dryL; right = dryR; return;}
        left = dryL + wet*(coloredL - dryL);
        right = dryR + wet*(coloredR - dryR);
    }
private:
    static double sanitize(double x) noexcept {
        return std::isfinite(x) ? std::clamp(x,-256.0,256.0) : 0.0;
    }
    double filter(std::size_t channel,double input) noexcept {
        auto& z = state[channel];
        const double output = b0*input + z[0];
        z[0] = b1*input - a1*output + z[1];
        z[1] = b2*input - a2*output;
        if(std::abs(z[0]) < 1.0e-28) z[0] = 0.0;
        if(std::abs(z[1]) < 1.0e-28) z[1] = 0.0;
        return output;
    }
    std::array<std::array<double,2>,2> state{};
    double b0=1.0,b1=0.0,b2=0.0,a1=0.0,a2=0.0;
    double wet=0.0,smoothStep=1.0;
};

} // namespace skelly::experimental
