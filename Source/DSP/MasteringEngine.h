#pragma once

#include "FiveBandColorPrototype.h"
#include "DepthWidth.h"
#include "SpatialSeparation.h"
#include "ImmersiveWidth.h"
#include "PeakWindow.h"
#include "AnalogSauce.h"
#include "CompressionMakeup.h"
#include <SkellCore/experimental/FiveBandEnhancer.h>

// SKELLY MASTER | portable DSP core (C++17, no GUI or host dependencies).
// State is allocated during prepare(); processFrame() performs no allocation.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace skelly {
namespace detail {
constexpr double pi = 3.1415926535897932384626433832795;
inline double clamp(double v, double lo, double hi) noexcept { return std::max(lo, std::min(hi, v)); }
inline double dbToLinear(double db) noexcept { return std::pow(10.0, db / 20.0); }
// 2.4 dB Deuce Match makeup, precomputed so the audio callback does no pow().
inline constexpr double deuceMatchMakeupGain = 1.318256738556407;
// The uploaded L2 threshold (-2.8) and output ceiling (-1.1) imply
// +1.7dB input drive before limiting. Only the new v7 chain opts in.
inline constexpr double fivePluginL2InputGain = 1.216186000646368;
inline double linearToDb(double v) noexcept {
    return 20.0 * std::log10(std::isfinite(v) ? std::max(v, 1.0e-12) : 1.0e-12);
}
inline double finiteSample(double v) noexcept { return std::isfinite(v) ? v : 0.0; }

class Smoother {
public:
    void prepare(double rate, double timeMs, double initial) noexcept {
        coefficient = 1.0 - std::exp(-1.0 / (std::max(1.0, rate) * std::max(0.001, timeMs * 0.001)));
        current = target = initial;
    }
    void setTarget(double newTarget) noexcept { target = newTarget; }
    double next() noexcept {
        current += (target - current) * coefficient;
        if (target == 0.0 && std::abs(current) < 1.0e-28) current = 0.0;
        return current;
    }
    double value() const noexcept { return current; }
    void snap(double v) noexcept { current = target = v; }
private:
    double current = 0.0, target = 0.0, coefficient = 1.0;
};

// The saturation normalization only depends on DRIVE, which changes slowly.
// Precompute its reciprocal tanh curve once during prepare() so the 4x audio
// path doesn't evaluate an extra transcendental function for every frame.
class DriveShaper {
public:
    void prepare() noexcept {
        for (std::size_t i = 0; i < table.size(); ++i) {
            const double amount = 1.0 + 4.0 * static_cast<double>(i) / (table.size() - 1);
            table[i] = 1.0 / std::tanh(amount);
        }
    }
    double process(double input, double drive) const noexcept {
        if (drive <= 1.0e-5) return input;
        const double amount = clamp(drive, 0.0, 1.0);
        const double t = amount * static_cast<double>(table.size() - 1);
        const std::size_t index = static_cast<std::size_t>(t);
        const std::size_t next = std::min(index + 1, table.size() - 1);
        const double fraction = t - static_cast<double>(index);
        const double normalization = table[index] + fraction * (table[next] - table[index]);
        const double shaped = std::tanh(input * (1.0 + 4.0 * amount)) * normalization;
        return input + amount * (shaped - input);
    }

    // One shared nonlinear gain keeps asymmetric stereo images stable, rather
    // than reshaping each channel to an unrelated instantaneous gain. Equal
    // magnitude (mono or anti-phase) retains the original DRIVE curve. The
    // dominant channel still sets saturation; nothing is allocated in DSP.
    void processStereo(double& left, double& right, double drive) const noexcept {
        if (drive <= 1.0e-5) return;
        const double magnitude = std::max(std::abs(left), std::abs(right));
        if (magnitude <= 1.0e-24) return;
        const double gain = process(magnitude, drive) / magnitude;
        left *= gain;
        right *= gain;
    }
private:
    std::array<double, 1025> table {};
};

// Transposed direct-form II, suitable for changing shelf coefficients without resetting state.
class Biquad {
public:
    void reset() noexcept { z1 = z2 = 0.0; }
    double process(double input) noexcept {
        const double output = b0 * input + z1;
        z1 = b1 * input - a1 * output + z2;
        z2 = b2 * input - a2 * output;
        // Suppress long-running subnormal tails without a hard audio-range gate.
        if (std::abs(z1) < 1.0e-28) z1 = 0.0;
        if (std::abs(z2) < 1.0e-28) z2 = 0.0;
        return output;
    }
    // RBJ peaking EQ, used only by the opt-in final API-like reference
    // stage. Coefficients are calculated during prepare(), never on audio.
    void setPeak(double sampleRate, double frequency, double gainDb, double q) noexcept {
        const double A=std::pow(10.0,clamp(gainDb,-12.0,12.0)/40.0);
        const double omega=2.0*pi*clamp(frequency,20.0,sampleRate*.45)/sampleRate;
        const double c=std::cos(omega);
        const double alpha=std::sin(omega)/(2.0*clamp(q,0.1,12.0));
        const double a0=1.0+alpha/A;
        const double norm=1.0/a0;
        b0=(1.0+alpha*A)*norm;
        b1=-2.0*c*norm;
        b2=(1.0-alpha*A)*norm;
        a1=-2.0*c*norm;
        a2=(1.0-alpha/A)*norm;
    }
    void setShelf(double sampleRate, double frequency, double gainDb, bool high) noexcept {
        const double A = std::pow(10.0, clamp(gainDb, -12.0, 12.0) / 40.0);
        const double w = 2.0 * pi * clamp(frequency, 20.0, sampleRate * 0.45) / sampleRate;
        const double c = std::cos(w), s = std::sin(w), rootA = std::sqrt(A);
        constexpr double slope = 0.85;
        const double alpha = s * 0.5 * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        double nb0, nb1, nb2, na0, na1, na2;
        if (high) {
            nb0 = A * ((A + 1) + (A - 1) * c + 2 * rootA * alpha);
            nb1 = -2 * A * ((A - 1) + (A + 1) * c);
            nb2 = A * ((A + 1) + (A - 1) * c - 2 * rootA * alpha);
            na0 = (A + 1) - (A - 1) * c + 2 * rootA * alpha;
            na1 = 2 * ((A - 1) - (A + 1) * c);
            na2 = (A + 1) - (A - 1) * c - 2 * rootA * alpha;
        } else {
            nb0 = A * ((A + 1) - (A - 1) * c + 2 * rootA * alpha);
            nb1 = 2 * A * ((A - 1) - (A + 1) * c);
            nb2 = A * ((A + 1) - (A - 1) * c - 2 * rootA * alpha);
            na0 = (A + 1) + (A - 1) * c + 2 * rootA * alpha;
            na1 = -2 * ((A - 1) + (A + 1) * c);
            na2 = (A + 1) + (A - 1) * c - 2 * rootA * alpha;
        }
        const double invA0 = 1.0 / na0;
        b0 = nb0 * invA0; b1 = nb1 * invA0; b2 = nb2 * invA0;
        a1 = na1 * invA0; a2 = na2 * invA0;
    }
    void setAllPassCoefficients(double first, double second) noexcept {
        a1=first; a2=second;
        b0=a2; b1=a1; b2=1.0;
    }
private:
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;
};

// API-550A is AFTER L2 in the owner's real five-plugin chain.
 // An opt-in host-rate mid bell therefore follows the Skell limiter,
 // ahead of the final true-peak guard. No extra PDC, allocations,
 // per-sample transcendental functions or host automation IDs.
class ReferenceFinalEq {
public:
    void prepare(double sampleRate) noexcept {
        const double rate=std::max(8000.0,sampleRate);
        for(auto& filter:mid)
            filter.setPeak(rate,1400.0,0.8,0.78);
        blend.prepare(rate,12.0,0.0);
        reset();
    }
    void reset() noexcept {
        for(auto& filter:mid) filter.reset();
        blend.snap(0.0);
    }
    void setEnabled(bool enabled) noexcept {
        blend.setTarget(enabled?1.0:0.0);
    }
    void process(double& left,double& right) noexcept {
        // Track both filter histories while disabled. When toggled on,
        // the next sample starts with warm, continuous filter state.
        const double filteredL=mid[0].process(left);
        const double filteredR=mid[1].process(right);
        const double wet=blend.next();
        if(wet>1.0e-12) {
            left+=wet*(filteredL-left);
            right+=wet*(filteredR-right);
        }
    }
private:
    std::array<Biquad,2> mid;
    Smoother blend;
};

// Host-rate reference correction ONLY for the primary SKELL MASTER factory.
// Independent private owner-audio comparisons found a -0.36dB deficit in the
// 500-2500Hz region. An initial four-band fit overfitted its source excerpt,
// so the held-out 14-second ending selected a simpler two-filter correction:
// -0.10dB low shelf at 95Hz and +0.70dB broad 1350Hz bell. This improves
// both training and held-out spectral-envelope matching without chasing
// owner reference files' unsafe +dBTP overs. The exact compiled 2-band VST3
// cut five-band full-song error 0.178->0.103dB, but its stereo side/mid
// widened by 0.274dB and crest by 0.139dB. Add a small linked correction
// and restrained makeup after EQ, BEFORE the existing true-peak guard.
// No new automation IDs, allocation or PDC.
class ReferenceToneCorrection {
public:
    void prepare(double sampleRate) noexcept {
        const double rate=std::max(8000.0,sampleRate);
        for(auto& channel : filters) {
            channel[0].setShelf(rate,95.0,-0.10,false);
            channel[1].setPeak(rate,1350.0,0.70,0.78);
        }
        blend.prepare(rate,12.0,0.0);
        sideTrim.prepare(rate,12.0,0.957194071294);
        reset();
    }
    void reset() noexcept {
        for(auto& channel : filters)
            for(auto& stage : channel) stage.reset();
        blend.snap(0.0);
        sideTrim.snap(0.957194071294);
    }
    void setClipAmount(double amount, bool calibrated) noexcept {
        const double mix=clamp(finiteSample(amount)/0.35,0.0,1.0);
        const double gain=0.941889596523
            +(0.954992586021-0.941889596523)*mix;
        sideTrim.setTarget(calibrated ? gain : 0.957194071294);
    }
    void setEnabled(bool enabled) noexcept {
        blend.setTarget(enabled?1.0:0.0);
    }
    void process(double& left,double& right) noexcept {
        // Keep filter histories current in bypass/other factories so enabling
        // a reference factory never produces a stale-state pop. At wet=0 the
        // audio is bit-exact dry. No coefficient work or allocation here.
        double filteredL=left, filteredR=right;
        const double sideGain=sideTrim.next();
        for(auto& stage : filters[0]) filteredL=stage.process(filteredL);
        for(auto& stage : filters[1]) filteredR=stage.process(filteredR);
        const double wet=blend.next();
        if(wet>1.0e-12) {
            // Real 99.65s owner's master A/B: the broad midrange correction
            // reduced spectral MAE, but increased side/mid balance by 0.274dB.
            // Full-song comparisons at CLIP0 and CLIP35 selected -0.38dB
            // side trim to reduce the remaining shared stereo-width error.
            // +0.15dB makeup drives the EXISTING final true-peak limiter
            // rather than raising output above its configured sample ceiling.
            // No clipping is introduced here: the guard remains last.
            constexpr double makeup = 1.017419366181;
            const double mid=0.5*(filteredL+filteredR);
            const double side=0.5*(filteredL-filteredR)*sideGain;
            filteredL=(mid+side)*makeup;
            filteredR=(mid-side)*makeup;
            left+=wet*(filteredL-left);
            right+=wet*(filteredR-right);
        }
    }
private:
    std::array<std::array<Biquad,2>,2> filters {};
    Smoother blend;
    Smoother sideTrim;
};

// New v8-only response curve: direct, bit-identical mapping through the
// complete published 0..30% factory range, followed by a smooth increasing
// compression taper. Unlike a hard clamp it has a continuous first derivative
// at 30% and never reverses direction near 100%.
inline double refinedCompressorAmount(double value,double maxEffective) noexcept {
    const double raw=clamp(finiteSample(value),0.0,1.0);
    if(raw<=0.30) return raw;
    const double maxAmount=clamp(maxEffective,0.31,1.0);
    const double taper=(0.70/(maxAmount-0.30)-1.0)/0.70;
    const double excess=raw-0.30;
    return 0.30+excess/(1.0+taper*excess);
}

class Compressor {
public:
    void prepare(double rate, double attackMs, double releaseMs) noexcept {
        attack = std::exp(-1.0 / (rate * attackMs * 0.001));
        release = std::exp(-1.0 / (rate * releaseMs * 0.001));
        gainAttack = std::exp(-1.0 / (rate * 0.003));
        gainRelease = std::exp(-1.0 / (rate * 0.070));
        // New-session, high-control-only program recovery coefficients.
        // All transcendental functions run during prepare, never audio.
        programFastRelease = std::exp(-1.0 / (rate * 0.034));
        programSlowRelease = std::exp(-1.0 / (rate * 0.142));
        programDensityPole = std::exp(-1.0 / (rate * 0.090));
        reset();
    }
    void reset() noexcept { envelope = 0.0; gain = targetGain = 1.0; tick = 0; programDensity = 0.0; }
    double next(double stereoPeak, double amount, double thresholdAtMax, double ratioAtMax,
                bool adaptive = false) noexcept {
        const double a = stereoPeak > envelope ? attack : release;
        envelope = a * envelope + (1.0 - a) * stereoPeak;
        if (envelope < 1.0e-27) envelope = 0.0;
        if (++tick >= 16) {
            tick = 0;
            if (amount <= 0.00001) targetGain = 1.0;
            else {
                const double thresholdDb = -3.0 + (thresholdAtMax + 3.0) * amount;
                const double ratio = 1.0 + (ratioAtMax - 1.0) * amount;
                const double over = linearToDb(envelope) - thresholdDb;
                // 6dB soft knee, avoiding discontinuities at onset.
                double reductionDb = 0.0;
                if (over > 3.0) reductionDb = (1.0 - 1.0 / ratio) * over;
                else if (over > -3.0) {
                    const double x = over + 3.0;
                    reductionDb = (1.0 - 1.0 / ratio) * x * x / 12.0;
                }
                targetGain = dbToLinear(-reductionDb);
            }
        }
        double effectiveRelease = gainRelease;
        // Only non-reference Glue/Dynamics settings above 30% use the new
        // release; all factory and restored-legacy curves stay bit-exact.
        const double blend = adaptive ? detail::clamp((amount-0.30) / 0.70, 0.0, 1.0) : 0.0;
        if (blend > 1.0e-9) {
            const double reduction = detail::clamp((1.0-targetGain)*6.0,0.0,1.0);
            programDensity = programDensityPole*programDensity
                + (1.0-programDensityPole)*reduction;
            if (programDensity < 1.0e-27) programDensity = 0.0;
            const double severity = detail::clamp((1.0-gain)*3.0,0.0,1.0);
            const double sustained = detail::clamp(0.80*programDensity
                +0.20*severity,0.0,1.0);
            const double dynamicRelease=programFastRelease
                +(programSlowRelease-programFastRelease)*sustained;
            effectiveRelease=gainRelease+blend*(dynamicRelease-gainRelease);
        } else {
            programDensity=0.0;
        }
        const double g = targetGain < gain ? gainAttack : effectiveRelease;
        gain = g * gain + (1.0 - g) * targetGain;
        return gain;
    }
private:
    double envelope = 0.0, gain = 1.0, targetGain = 1.0;
    double attack = 0.0, release = 0.0, gainAttack = 0.0, gainRelease = 0.0;
    double programFastRelease = 0.0, programSlowRelease = 0.0;
    double programDensityPole = 0.0, programDensity = 0.0;
    int tick = 0;
};

class OnePole {
public:
    void prepare(double rate, double hz) noexcept { a = std::exp(-2.0 * pi * hz / rate); z = 0.0; }
    void reset() noexcept { z = 0.0; }
    double low(double x) noexcept {
        z = a * z + (1.0 - a) * x;
        if (std::abs(z) < 1.0e-27) z = 0.0;
        return z;
    }
private:
    double a = 0.0, z = 0.0;
};
// Control zero maps to the safe factory ceiling of -1 dBFS.
inline double ceilingOffsetToDb(double offset) noexcept {
    const double x=std::isfinite(offset) ? std::clamp(offset,-3.0,3.0) : 0.0;
    return x<0.0 ? -1.0+(2.0/3.0)*x : -1.0+0.3*x;
}
inline double ceilingDbToOffset(double db) noexcept {
    const double x=std::isfinite(db) ? std::clamp(db,-3.0,-0.1) : -1.0;
    return x<-1.0 ? (x+1.0)*1.5 : (x+1.0)/0.3;
}
} // namespace detail

struct MasterParameters {
    double inputDb = 0.0;
    double drive = 0.0;     // 0..1
    double glue = 0.0;      // 0..1
    double dynamics = 0.0;  // 0..1
    double lowsDb = 0.0;    // -6..+6
    double highsDb = 0.0;   // -6..+6
    double sauce = 0.0;     // 0..1
    double width = 1.0;     // Legacy V10/V11: 0..1.6, 1 neutral
    double spatialWidth = 0.0; // V12: 0..1 creative stereo separation
    bool immersiveWidth = false; // V17 new-session wider midband
    bool fiveBandEnhancement = false; // opt-in creative mode, never implicit
    bool enhancedSauce = false; // opt-in; old saved states remain exact
    bool analogSauce = false; // V13 parallel coloration above 20% only
    bool depthWidth = false; // V11 creative width, V10 states retain previous widening
    bool efficientDepth = false; // V15 skip inactive 3D stages; legacy states warm filters
    bool adaptiveCompression = false; // V15 only >30% compression, opt-in new sessions
    bool compressionMakeup = false; // V16 bounded partial level recovery
    double transients = 0.0;// 0..1
    double outputDb = 0.0;
    double clipper = 0.0;  // existing 0..1 CLIP automation ID, unchanged
    bool referenceClip = false; // v6 post-limit or v7 chain-ordered clip
    bool clipBeforeLimiter = false; // false preserves exact v6 topology
    bool refinedControlCurves = false; // v8 new-instance only; old states exact
    bool referenceCalibration = false; // v9 factory5 recovery/phase profile
    double ceilingDb = -1.0;
    bool bypass = false;
    bool deuceMatch = false; // opt-in pre-limiter five-band reference coloration
};

struct StereoFrame { float left = 0.0f, right = 0.0f; };

// Legacy host-rate sample-peak guard retained for the regression diagnostic.
// Production Skell Master now uses StereoTruePeakGuard below.
class StereoPeakGuard {
public:
    void prepare(double sampleRate) noexcept {
        release = std::exp(-1.0 / (std::max(1.0, sampleRate) * 0.085));
        reset();
    }
    void reset() noexcept { gain = 1.0; }
    StereoFrame process(StereoFrame in, double ceilingLinear,
                        bool bypass = false) noexcept {
        // Guard downsampled output as a final safety boundary. Do not let a
        // broken host buffer permanently poison the linked gain envelope.
        in.left = static_cast<float>(detail::finiteSample(in.left));
        in.right = static_cast<float>(detail::finiteSample(in.right));
        // Only the active sample-peak route should update its envelope.
        // DEFAULT/BYPASS and the reconstructed true-peak route previously
        // accumulated inaudible gain reduction here, making the next legacy
        // route start attenuated. This also skips needless guard arithmetic.
        if (bypass) {
            gain = 1.0;
            return in;
        }
        const double limit = detail::clamp(std::isfinite(ceilingLinear) ? ceilingLinear : 1.0,
                                           1.0e-5, 1.0);
        const double peak = std::max(std::abs(static_cast<double>(in.left)),
                                     std::abs(static_cast<double>(in.right)));
        const double wanted = peak > limit ? limit / peak : 1.0;
        if (wanted < gain) gain = wanted;
        else gain = release * gain + (1.0 - release) * wanted;
        const auto apply = [&](float sample) noexcept {
            return static_cast<float>(
                detail::clamp(static_cast<double>(sample) * gain, -limit, limit));
        };
        return {apply(in.left), apply(in.right)};
    }
    double currentGain() const noexcept { return gain; }
private:
    double gain = 1.0, release = 0.0;
};

// Host-output true-peak MONITOR; does not modify samples or guarantee a
// dBTP ceiling. ITU-R BS.1770-5 Annex 2 48-tap, four-phase interpolating
// FIR example, arranged as 12 taps per phase. This reports reconstructed
// intersample peaks, unlike the existing sample-peak limiter. No heap,
// allocations, or filter design occurs on the audio callback.
// https://www.itu.int/rec/R-REC-BS.1770/en
class StereoTruePeakMonitor {
public:
    void reset() noexcept {
        for (auto& channel : history) channel.fill(0.0);
        newest = 0;
    }
    // Maximum of the 4 reconstructed phases for this frame, across channels.
    // The JUCE wrapper can use this on a true mono bus. Only one channel
    // needs interpolation; the second ring is kept in sync so reset and
    // subsequent stereo calls remain correct. The meter never alters sound.
    float processMono(float sample) noexcept {
        const double value = detail::finiteSample(static_cast<double>(sample));
        const auto write = static_cast<std::size_t>(newest);
        history[0][write] = value;
        history[1][write] = value;
        double maximum = std::abs(value);
        double reconstructed[4] {};
        int idx = newest;
        for (int k = 0; k < 12; ++k) {
            const auto& taps = fir[static_cast<std::size_t>(k)];
            const double valueAtTap = history[0][static_cast<std::size_t>(idx)];
            for (int phase = 0; phase < 4; ++phase)
                reconstructed[phase] += valueAtTap * taps[static_cast<std::size_t>(phase)];
            if (--idx < 0) idx = 11;
        }
        for (double phase : reconstructed)
            maximum = std::max(maximum, std::abs(phase));
        if (++newest == 12) newest = 0;
        return static_cast<float>(maximum);
    }
    float process(float left, float right) noexcept {
        history[0][static_cast<std::size_t>(newest)] =
            detail::finiteSample(static_cast<double>(left));
        history[1][static_cast<std::size_t>(newest)] =
            detail::finiteSample(static_cast<double>(right));
        double maximum = std::max(std::abs(history[0][static_cast<std::size_t>(newest)]),
                                  std::abs(history[1][static_cast<std::size_t>(newest)]));
        // One traversal of the 12-frame ring reconstructs all four phases
        // for both channels. This preserves the reference FIR sum order for
        // every phase, while avoiding 8 redundant ring walks per sample.
        double reconstructed[2][4] {};
        int idx = newest;
        for (int k = 0; k < 12; ++k) {
            const auto& taps = fir[static_cast<std::size_t>(k)];
            const double l = history[0][static_cast<std::size_t>(idx)];
            const double r = history[1][static_cast<std::size_t>(idx)];
            for (int phase = 0; phase < 4; ++phase) {
                reconstructed[0][phase] += l * taps[static_cast<std::size_t>(phase)];
                reconstructed[1][phase] += r * taps[static_cast<std::size_t>(phase)];
            }
            if (--idx < 0) idx = 11;
        }
        for (const auto& channel : reconstructed)
            for (const auto phase : channel)
                maximum = std::max(maximum, std::abs(phase));
        if (++newest == 12) newest = 0;
        return static_cast<float>(maximum);
    }
private:
    // Original coefficients of the ITU Annex 2 reference interpolation FIR.
    static constexpr std::array<std::array<double,4>,12> fir {{
        {{ 0.001708984375,  -0.0291748046875, -0.0189208984375, -0.00830078125 }},
        {{ 0.010986328125,   0.029296875,      0.0330810546875,  0.014892578125 }},
        {{-0.0196533203125, -0.0517578125,    -0.0582275390625, -0.026611328125 }},
        {{ 0.033203125,      0.089111328125,   0.1015625,       0.047607421875 }},
        {{-0.0594482421875, -0.16650390625,   -0.2003173828125, -0.102294921875 }},
        {{ 0.1373291015625,  0.465087890625,   0.77978515625,    0.97216796875 }},
        {{ 0.97216796875,    0.77978515625,    0.465087890625,   0.1373291015625 }},
        {{-0.102294921875,  -0.2003173828125, -0.16650390625,  -0.0594482421875 }},
        {{ 0.047607421875,   0.1015625,       0.089111328125,   0.033203125 }},
        {{-0.026611328125,  -0.0582275390625, -0.0517578125,   -0.0196533203125 }},
        {{ 0.014892578125,   0.0330810546875,  0.029296875,      0.010986328125 }},
        {{-0.00830078125,   -0.0189208984375, -0.0291748046875,  0.001708984375 }}
    }};
    std::array<std::array<double,12>,2> history {};
    int newest = 0;
};

// Four-times reconstructed true-peak detector with a symmetric Kaiser FIR.
// Parabolic refinement estimates local peaks between the four reconstructed phases;
// a separate 32-sample sample-peak window gives the guard genuine lookahead for attack.
// Coefficients are prepared off the audio thread; processing uses fixed arrays only.
class StereoTruePeakDetector4x {
public:
    static constexpr int oversampling = 4;
    static constexpr int tapCount = 257;
    static constexpr int centerTap = 128;
    static constexpr int lookaheadSamples = 32;
    static constexpr int historyLength = 2 * lookaheadSamples + 1;

    void prepare() noexcept {
        constexpr double pi = 3.1415926535897932384626433832795;
        constexpr double beta = 12.0;
        const double i0Beta = besselI0(beta);
        double sum = 0.0;
        for (int n = 0; n < tapCount; ++n) {
            const double x = static_cast<double>(n - centerTap);
            const double normalized = x / static_cast<double>(centerTap);
            const double argument = pi * 0.25 * x;
            const double sinc = std::abs(x) < 1.0e-12
                ? 0.25 : 0.25 * std::sin(argument) / argument;
            const double window = besselI0(beta * std::sqrt(
                std::max(0.0, 1.0 - normalized * normalized))) / i0Beta;
            coefficients[static_cast<std::size_t>(n)] = sinc * window;
            sum += sinc * window;
        }
        if (std::isfinite(sum) && std::abs(sum) > 1.0e-12) {
            for (auto& value : coefficients)
                value /= sum;
        }
        reset();
    }

    void reset() noexcept {
        history.fill({});
        windowPeak.reset();
        writeIndex = 0;
    }

    // When measurement is disabled (for fixed-latency bypass), keep the
    // rolling history current but skip the four 257-tap reconstruction phases.
    // This lets Deuce Match engage immediately without a cold-history window.
    float process(float left, float right, bool measurePeak = true) noexcept {
        const float safeLeft = static_cast<float>(detail::clamp(
            detail::finiteSample(static_cast<double>(left)), -256.0, 256.0));
        const float safeRight = static_cast<float>(detail::clamp(
            detail::finiteSample(static_cast<double>(right)), -256.0, 256.0));
        history[writeIndex] = { safeLeft, safeRight };
        // Track the exact same sanitized host-rate samples even when the
        // reconstructed detector is bypassed, so reentry is never cold.
        windowPeak.push(std::max(std::abs(safeLeft),std::abs(safeRight)));

        double maximum = 0.0;
        if (measurePeak) {
            int delayedIndex = static_cast<int>(writeIndex) - lookaheadSamples;
            if (delayedIndex < 0)
                delayedIndex += historyLength;
            const auto& delayed = history[static_cast<std::size_t>(delayedIndex)];
            int endpointIndex = static_cast<int>(writeIndex) - (lookaheadSamples - 1);
            if (endpointIndex < 0)
                endpointIndex += historyLength;
            const auto& endpoint = history[static_cast<std::size_t>(endpointIndex)];
            double reconstructed[2][oversampling] {};

            // Each phase is one reconstructed point between the delayed sample
            // and its next host-rate neighbour. Keep the points so a local
            // three-point parabola can estimate the peak between 4x phases.
            // All four phases share one history traversal. Preserve the
            // increasing-q sum order of every phase, including the 257th
            // tap in phase zero. No change to FIR coefficients or PDC.
            int index=static_cast<int>(writeIndex);
            for(int q=0;q<tapCount/oversampling;++q) {
                const auto& sample=history[static_cast<std::size_t>(index)];
                for(int phase=0;phase<oversampling;++phase) {
                    const double weight=static_cast<double>(oversampling)
                        * coefficients[static_cast<std::size_t>(oversampling*q+phase)];
                    reconstructed[0][phase]+=weight*sample.left;
                    reconstructed[1][phase]+=weight*sample.right;
                }
                if(--index<0) index=historyLength-1;
            }
            const auto& last=history[static_cast<std::size_t>(index)];
            const double lastWeight=static_cast<double>(oversampling)*coefficients.back();
            reconstructed[0][0]+=lastWeight*last.left;
            reconstructed[1][0]+=lastWeight*last.right;
            for(int phase=0;phase<oversampling;++phase)
                maximum=std::max(maximum,std::max(std::abs(reconstructed[0][phase]),
                                                   std::abs(reconstructed[1][phase])));

            const auto refineLocalPeak = [](double previous, double current,
                                             double next) noexcept {
                const double yMinus = std::abs(previous);
                const double y0 = std::abs(current);
                const double yPlus = std::abs(next);
                if (y0 < yMinus || y0 < yPlus)
                    return y0;
                const double curvature = yMinus - 2.0 * y0 + yPlus;
                if (curvature >= -1.0e-12)
                    return y0;
                const double difference = yPlus - yMinus;
                const double estimate = y0
                    - (difference * difference) / (8.0 * curvature);
                return std::isfinite(estimate) ? std::max(y0, estimate) : y0;
            };

            maximum = std::max(maximum, std::max(
                std::abs(static_cast<double>(endpoint.left)),
                std::abs(static_cast<double>(endpoint.right))));
            const double leftPoints[5] = {
                static_cast<double>(delayed.left), reconstructed[0][1],
                reconstructed[0][2], reconstructed[0][3],
                static_cast<double>(endpoint.left)
            };
            const double rightPoints[5] = {
                static_cast<double>(delayed.right), reconstructed[1][1],
                reconstructed[1][2], reconstructed[1][3],
                static_cast<double>(endpoint.right)
            };
            for (int point = 1; point < 4; ++point) {
                maximum = std::max(maximum, refineLocalPeak(
                    leftPoints[point - 1], leftPoints[point], leftPoints[point + 1]));
                maximum = std::max(maximum, refineLocalPeak(
                    rightPoints[point - 1], rightPoints[point], rightPoints[point + 1]));
            }
        }

        if (++writeIndex == static_cast<std::size_t>(historyLength))
            writeIndex = 0;
        return static_cast<float>(maximum);
    }

    // After process(), writeIndex points to the next ring slot. Offsets 1..33
    // therefore cover the current input sample through the delayed output sample,
    // inclusive: a real 32-sample lookahead window for pre-attacking upcoming peaks.
    float lookaheadSamplePeak() const noexcept {
        return windowPeak.maximum();
    }

private:
    static double besselI0(double x) noexcept {
        double sum = 1.0;
        double term = 1.0;
        const double y = x * x * 0.25;
        for (int k = 1; k <= 80; ++k) {
            term *= y / (static_cast<double>(k) * k);
            sum += term;
            if (term < sum * 1.0e-16)
                break;
        }
        return sum;
    }

    std::array<double, tapCount> coefficients {};
    std::array<StereoFrame, historyLength> history {};
    PeakWindow<lookaheadSamples+1> windowPeak;
    std::size_t writeIndex = 0;
};

// Stereo-linked host-rate true-peak safety stage. The 257-tap / 4-phase Kaiser
// detector uses parabolic peak refinement; a separate 32-sample sample-peak
// window pre-attacks upcoming transients. The internal guard margin is rate-adaptive:
// 0.5 dB at 44.1 kHz, 0.45 dB at 48 kHz and 0.4 dB at 96 kHz.
// Bypass preserves the same 32-sample latency but never attenuates. No audio-thread allocation.
class StereoTruePeakGuard {
public:
    static constexpr int delayLength = StereoTruePeakDetector4x::lookaheadSamples + 1;
    static constexpr int lookaheadSamples = StereoTruePeakDetector4x::lookaheadSamples;

    void prepare(double sampleRate) noexcept {
        const double rate = std::isfinite(sampleRate)
            ? std::max(1.0, sampleRate) : 48000.0;
        release = std::exp(-1.0 / (rate * 0.085));
        // The rate-dependent margin cannot change during processing.
        // Cache the exact same conversion outside the audio callback.
        guardMarginGain = detail::dbToLinear(-(rate <= 46000.0 ? 0.5 : rate <= 72000.0 ? 0.45 : 0.4));
        detector.prepare();
        reset();
    }

    void reset() noexcept {
        detector.reset();
        for (auto& channel : delay)
            channel.fill(0.0f);
        writeIndex = 0;
        gain = 1.0;
    }

    StereoFrame process(StereoFrame input, double ceilingLinear,
                        bool bypass = false) noexcept {
        const float left = static_cast<float>(detail::clamp(
            detail::finiteSample(static_cast<double>(input.left)), -256.0, 256.0));
        const float right = static_cast<float>(detail::clamp(
            detail::finiteSample(static_cast<double>(input.right)), -256.0, 256.0));

        const double reconstructedPeak = static_cast<double>(
            detector.process(left, right, !bypass));
        // Detector history and the 32-sample audio delay remain current in
        // bypass, but measuring the 33-sample peak window has no effect when
        // guard gain is forced to unity. Avoid that work on the host thread.
        const double measuredPeak = bypass ? 0.0 : std::max(reconstructedPeak,
            static_cast<double>(detector.lookaheadSamplePeak()));
        const double requestedCeiling = std::clamp(
            std::isfinite(ceilingLinear) ? ceilingLinear : detail::dbToLinear(-1.0),
            1.0e-5, 1.0);
        const double truePeakLimit = requestedCeiling * guardMarginGain;
        const double wantedGain = (!bypass && measuredPeak > truePeakLimit)
            ? truePeakLimit / std::max(measuredPeak, 1.0e-12) : 1.0;

        if (bypass) {
            gain = 1.0;
        } else if (wantedGain < gain) {
            gain = wantedGain;
        } else {
            gain = release * gain + (1.0 - release) * wantedGain;
        }

        delay[0][writeIndex] = left;
        delay[1][writeIndex] = right;
        const std::size_t readIndex =
            (writeIndex + 1) % static_cast<std::size_t>(delayLength);
        const StereoFrame output {
            static_cast<float>(delay[0][readIndex] * gain),
            static_cast<float>(delay[1][readIndex] * gain)
        };
        writeIndex = (writeIndex + 1) % static_cast<std::size_t>(delayLength);
        return output;
    }

    double currentGain() const noexcept { return gain; }
    int getLatencySamples() const noexcept { return lookaheadSamples; }

private:
    StereoTruePeakDetector4x detector;
    std::array<std::array<float, delayLength>, 2> delay {};
    std::size_t writeIndex = 0;
    double gain = 1.0;
    double release = 0.0;
    double guardMarginGain = detail::dbToLinear(-0.3);
};

// Gentle mastering width enhancement with two safeguards:
// (1) a 220 Hz side low-pass leaves bass largely unaffected;
// (2) a slow mid/side energy estimate suppresses widening when L/R are
// strongly anti-correlated. All states run continuously when Amount == 0,
// avoiding sudden stale-filter transients on parameter automation.
class StereoImager {
public:
    void prepare(double sampleRate) noexcept {
        sideBass.prepare(sampleRate, 220.0);
        depth.prepare(sampleRate);
        energyCoefficient = std::exp(-1.0 / (std::max(8000.0, sampleRate) * 0.055));
        reset();
    }
    void reset() noexcept {
        sideBass.reset();
        depth.reset();
        depthWasActive=false;
        midPower = sidePower = 0.0;
    }
    void process(double& l, double& r, double amount, double widthOffset = 0.0,
                 double depthBlend = 0.0, bool efficientDepth = false) noexcept {
        const double mid = 0.5 * (l + r);
        const double side = 0.5 * (l - r);
        const double bassSide = sideBass.low(side);
        midPower = energyCoefficient * midPower + (1.0 - energyCoefficient) * mid * mid;
        sidePower = energyCoefficient * sidePower + (1.0 - energyCoefficient) * side * side;
        if (midPower < 1.0e-36) midPower = 0.0;
        if (sidePower < 1.0e-36) sidePower = 0.0;
        if (amount < 1.0e-6 && std::abs(widthOffset) < 1.0e-6) {
            if (efficientDepth) {
                if(depthWasActive) { depth.reset(); depthWasActive=false; }
            } else if(depthBlend>1.e-6) depth.process(mid,side,0.,0.);
            return;
        }

        const double correlation = (midPower - sidePower) / (midPower + sidePower + 1.0e-12);
        const double protection = detail::clamp((correlation + 0.15) / 0.60, 0.0, 1.0);
        const double width = 0.18 * detail::clamp(amount, 0.0, 1.0) * protection;
        double adjustedSide = side + (side - bassSide) * width;
        // Width is independent of the old Sauce imaging. Narrowing does not
        // depend on correlation; widening protects bass and anti-phase audio.
        // V11 states retain their always-warm filter histories. V15 new
        // sessions skip expensive inert filters, reset old histories on
        // disengage, then enter via the existing 35ms width smoother.
        const bool runDepth=depthBlend>1.e-6
            && (!efficientDepth || widthOffset>1.e-6);
        if(efficientDepth && !runDepth && depthWasActive) {
            depth.reset();
            depthWasActive=false;
        }
        const double expanded=runDepth
            ? depth.process(mid,adjustedSide,widthOffset,protection) : adjustedSide;
        if(runDepth) depthWasActive=true;
        if (widthOffset < -1.0e-6) adjustedSide *= 1.0 + detail::clamp(widthOffset,-1.0,0.0);
        else if (widthOffset > 1.0e-6) {
            const double previous=adjustedSide+(adjustedSide-bassSide)*detail::clamp(widthOffset,0.0,0.6)*protection;
            adjustedSide=depthBlend>1.e-6
                ? previous+depthBlend*(expanded-previous) : previous;
        }
        l = mid + adjustedSide;
        r = mid - adjustedSide;
    }
private:
    detail::OnePole sideBass;
    DepthWidth depth;
    bool depthWasActive=false;
    double energyCoefficient = 0.0;
    double midPower = 0.0, sidePower = 0.0;
};

// Engine runs at 4x the host rate. Feed it frames from a quality oversampling filter.
class MasteringEngine {
public:
    void prepare(double oversampledRate, int numChannels, int lookaheadInOversampledSamples) {
        rate = std::max(8000.0, oversampledRate);
        channels = std::clamp(numChannels, 1, 2);
        lookahead = std::max(4, lookaheadInOversampledSamples);
        for (auto& v : programDelay) v.assign(static_cast<std::size_t>(lookahead), 0.0);
        for (auto& v : dryDelay) v.assign(static_cast<std::size_t>(lookahead), 0.0);
        input.prepare(rate, 15, 1); drive.prepare(rate, 25, 0);
        glueAmt.prepare(rate, 30, 0); dynamicsAmt.prepare(rate, 30, 0);
        lows.prepare(rate, 35, 0); highs.prepare(rate, 35, 0);
        sauceAmt.prepare(rate, 30, 0); transientsAmt.prepare(rate, 30, 0);
        widthOffset.prepare(rate, 35, 0);
        spatialAmount.prepare(rate, 45, 0);
        spatialSeparation.prepare(rate);
        immersiveWidth.prepare(rate);
        creativeEnhancer.prepare(rate);
        creativeBlend.prepare(rate, 35, 0.0);
        sauceEnhanceMix.prepare(rate, 35, 0);
        analogSauceMix.prepare(rate, 45, 0);
        analogColor.prepare(rate);
        depthWidthMix.prepare(rate, 35, 0);
        output.prepare(rate, 15, 1); ceiling.prepare(rate, 15, detail::dbToLinear(-1));
        clipperMix.prepare(rate, 18, 0);
        referenceClipDrive.prepare(rate, 18, 0);
        bypassMix.prepare(rate, 12, 0);
        deuceMatchMakeup.prepare(rate, 12, 1.0);
        chainL2InputMakeup.prepare(rate, 12, 1.0);
        // Owner-reference phase fit used only the 20--85s main section;
        // intro/ending were held out. Causal allpasses preserve static gain.
        // Morph their analog frequencies with Clip, avoiding a permanent
        // parallel phase mix (and its comb filtering) at intermediate values.
        // This adds no delay buffer or reported-latency adjustment. All costly
        // coefficient work and storage allocation happen here, not in audio.
        referencePhaseBlend.prepare(rate,12,0.0);
        referencePhaseTable.resize(1025);
        constexpr std::array<double,4> frequencies {90.0,500.0,2594.0,9310.0};
        constexpr std::array<double,4> qualities {
            0.495856529115,0.617989725037,0.856550120135,1.298156850978};
        constexpr double referenceRate=176400.0;
        for(std::size_t point=0;point<referencePhaseTable.size();++point) {
            const double amount=static_cast<double>(point)/(referencePhaseTable.size()-1);
            for(std::size_t stage=0;stage<frequencies.size();++stage) {
                const double analogFrequency=referenceRate/detail::pi
                    *std::tan(detail::pi*frequencies[stage]/referenceRate);
                const double frequency=std::exp(std::log(2000000.0)*(1.0-amount)
                    +std::log(analogFrequency)*amount);
                const double omega=2.0*std::atan(detail::pi*frequency/rate);
                const double alpha=std::sin(omega)/(2.0*qualities[stage]);
                const double norm=1.0/(1.0+alpha);
                referencePhaseTable[point][stage]={-2.0*std::cos(omega)*norm,
                                                  (1.0-alpha)*norm};
            }
        }
        compressionMakeup.prepare(rate);
        glueComp.prepare(rate, 27, 220);
        dynamicsComp.prepare(rate, 8, 110);
        hf[0].prepare(rate, 2400); hf[1].prepare(rate, 2400);
        stereoImager.prepare(rate);
        fiveBandColor.prepare(rate);
        driveShaper.prepare();
        // Owner's full-length master-only vs StandardCLIP WAV identifies the
        // demonstrated clipping sound at approximately 35%, NOT at 100%.
        // A fitted nonlinear transfer at that reference point uses +2.23dB
        // drive, ~7.6% cubic saturation and a smooth 0.91FS shoulder
        // (relative to the original master-only WAV with a 0dBFS ceiling).
        // Extend the range above 35% for deliberately stronger clipping.
        // Precompute pow/db conversions ONLY during prepare, not per frame.
        for(std::size_t i=0;i<referenceClipGainTable.size();++i) {
            const double amount=static_cast<double>(i)/(referenceClipGainTable.size()-1);
            const double db=amount<=0.35
                ? 2.23*std::pow(amount/0.35,1.15)
                : 2.23+5.77*std::pow((amount-0.35)/0.65,1.15);
            referenceClipGainTable[i]=detail::dbToLinear(db);
            const double referenceProgress=std::min(1.0,amount/0.35);
            const double onset=referenceProgress*referenceProgress*
                (3.0-2.0*referenceProgress);
            const double above=std::max(0.0,(amount-0.35)/0.65);
            referenceClipThresholdTable[i]=detail::dbToLinear(
                -1.20*onset-4.8*above);
        }
        dcR = std::exp(-2.0 * detail::pi * 5.0 / rate);
        fastAtk = std::exp(-1.0 / (rate * 0.002));
        slowAtk = std::exp(-1.0 / (rate * 0.060));
        // Two-stage release: fast recovery for small peaks, longer release
        // after severe attenuation to prevent bass-induced pumping.
        limiterReleaseFast = std::exp(-1.0 / (rate * 0.065));
        limiterReleaseSlow = std::exp(-1.0 / (rate * 0.220));
        referenceReleaseFast = std::exp(-1.0 / (rate * 0.040));
        referenceReleaseSlow = std::exp(-1.0 / (rate * 0.150));
        reset();
    }

    void reset() noexcept {
        for (auto& c : programDelay) std::fill(c.begin(), c.end(), 0.0);
        for (auto& c : dryDelay) std::fill(c.begin(), c.end(), 0.0);
        for (auto& f : lowShelf) f.reset();
        for (auto& f : highShelf) f.reset();
        for (auto& f : hf) f.reset();
        stereoImager.reset();
        widthOffset.snap(0.0);
        spatialAmount.snap(0.0);
        spatialSeparation.reset();
        immersiveWidth.reset();
        creativeEnhancer.reset();
        creativeBlend.snap(0.0);
        creativeWasActive=false;
        spatialWasActive=false;
        sauceEnhanceMix.snap(0.0);
        analogSauceMix.snap(0.0);
        analogColor.reset();
        analogWasActive=false;
        depthWidthMix.snap(0.0);
        fiveBandColor.reset();
        deuceMatchMakeup.snap(1.0);
        chainL2InputMakeup.snap(1.0);
        referencePhaseBlend.snap(0.0);
        for(auto& channel : referencePhase)
            for(auto& filter : channel) filter.reset();
        referencePhaseActive=false;
        lastPhasePosition=-1.0;
        referenceClipDrive.snap(0.0);
        glueComp.reset(); dynamicsComp.reset();
        compressionMakeup.reset();
        dcInput.fill(0.0); dcOutput.fill(0.0);
        fastEnv = slowEnv = 0.0; limiterGain = 1.0; limiterHold = 0; writeIndex = 0;
        eqCounter = 0; lastEqLows = lastEqHighs = 1.0e9;
        gainReduction = 1.0; outputPeak = 0.0;
    }

    void setParameters(const MasterParameters& p) noexcept {
        referenceCalibration=p.referenceCalibration;
        efficientDepthMode=p.efficientDepth;
        if(immersiveWidthMode!=p.immersiveWidth) {
            spatialSeparation.reset(); immersiveWidth.reset();
            spatialWasActive=false;
        }
        immersiveWidthMode=p.immersiveWidth;
        creativeBlend.setTarget(p.fiveBandEnhancement && !p.bypass ? 1.0 : 0.0);
        adaptiveCompressionMode=p.adaptiveCompression && !p.bypass;
        compressionMakeupMode=p.compressionMakeup && !p.bypass;
        if(!compressionMakeupMode) compressionMakeup.reset();
        // Unexpected non-finite host automation must never become an active
        // effect. std::min/std::max on NaN used to select the upper bound,
        // e.g. accidentally setting DRIVE/GLUE to 100% and INPUT to +18dB.
        // Normal finite parameter values follow the exact previous path.
        const auto safe = [](double value, double lo, double hi) noexcept {
            return detail::clamp(detail::finiteSample(value), lo, hi);
        };
        input.setTarget(detail::dbToLinear(safe(p.inputDb, -18, 18)));
        drive.setTarget(safe(p.drive, 0, 1));
        const double gluePosition=safe(p.glue,0,1);
        const double dynamicsPosition=safe(p.dynamics,0,1);
        // Factory 1..9 compressor values are all <=24% GLUE and <=18%
        // DYNAMICS, so these settings are mathematically unchanged.
        // Old saved sessions use their exact published linear 0..100% map.
        glueAmt.setTarget(p.refinedControlCurves
            ? detail::refinedCompressorAmount(gluePosition,0.75) : gluePosition);
        dynamicsAmt.setTarget(p.refinedControlCurves
            ? detail::refinedCompressorAmount(dynamicsPosition,0.60) : dynamicsPosition);
        lows.setTarget(safe(p.lowsDb, -6, 6));
        highs.setTarget(safe(p.highsDb, -6, 6));
        sauceAmt.setTarget(safe(p.sauce, 0, 1));
        widthOffset.setTarget(std::isfinite(p.width) ? safe(p.width-1.0,-1.0,0.6) : 0.0);
        spatialAmount.setTarget(safe(p.spatialWidth,0.0,1.0));
        sauceEnhanceMix.setTarget(p.enhancedSauce ? 1.0 : 0.0);
        analogSauceMix.setTarget(p.analogSauce ? 1.0 : 0.0);
        depthWidthMix.setTarget(p.depthWidth ? 1.0 : 0.0);
        transientsAmt.setTarget(safe(p.transients, 0, 1));
        output.setTarget(detail::dbToLinear(safe(p.outputDb, -18, 12)));
        // Switch clip topology with two smoothly crossfading controls.
        // Old saved sessions still use their original pre-limiter soft clip.
        const double clipPosition=safe(p.clipper,0,1);
        referencePhaseBlend.setTarget(referenceCalibration && clipPosition>1.e-6
                                     && !p.bypass ? 1.0 : 0.0);
        clipperMix.setTarget(!p.referenceClip ? clipPosition : 0.0);
        referenceClipDrive.setTarget(p.referenceClip && !p.bypass ? clipPosition : 0.0);
        referenceClipBeforeLimiter=p.referenceClip && p.clipBeforeLimiter;
        // For an invalid ceiling, fall back to the established -1dBFS
        // factory value, not the upper edge of -0.1dBFS.
        ceiling.setTarget(detail::dbToLinear(detail::clamp(
            std::isfinite(p.ceilingDb) ? p.ceilingDb : -1.0, -3, -0.1)));
        bypassMix.setTarget(p.bypass ? 1.0 : 0.0);
        // The bypass path is latency-aligned dry. Fade the optional nonlinear
        // Deuce color stage to neutral while bypassed; FiveBandColorPrototype
        // still tracks its crossover history at zero wet for click-free re-enable.
        deuceMatch = p.deuceMatch && !p.bypass;
        // Previous v1-v6 projects keep their +2.4dB Deuce makeup AFTER
        // the internal limiter, sample-for-sample. The uploaded five-plugin
        // source has an L2 with -2.8dB threshold and -1.1dB out ceiling.
        // In v7 we instead put their +1.7dB difference BEFORE the limiter;
        // post-limiter makeup is neutral, so the final safety guard need not
        // re-flatten peaks that have already been lookahead limited.
        const bool chainL2=deuceMatch && p.referenceClip && p.clipBeforeLimiter;
        chainL2InputMakeup.setTarget(chainL2
            ? detail::fivePluginL2InputGain : 1.0);
        deuceMatchMakeup.setTarget(deuceMatch && !chainL2
            ? detail::deuceMatchMakeupGain : 1.0);
    }

    void beginBlock() noexcept { gainReduction = 1.0; outputPeak = 0.0; }

    StereoFrame processFrame(float inLeft, float inRight) noexcept {
        using namespace detail;
        // A NaN/Inf arriving from an upstream plugin must not persist inside
        // EQ, sidechain, feedback or limiter states for the rest of the session.
        // Share the +48dBFS input guard of the real JUCE host wrapper.
        // Portable calls with huge but finite float samples must not create
        // infinite intermediate/output values (especially in bypass).
        // Ordinary program audio is bit-identical to the previous path.
        const double rawL = detail::clamp(
            detail::finiteSample(static_cast<double>(inLeft)), -256.0, 256.0);
        const double rawR = channels == 2 ? detail::clamp(
            detail::finiteSample(static_cast<double>(inRight)), -256.0, 256.0) : rawL;
        double l = rawL, r = rawR;
        const double inputGain = input.next();
        const double d = drive.next();
        const double g = glueAmt.next(), dyn = dynamicsAmt.next();
        const double eqLo = lows.next(), eqHi = highs.next();
        const double s = sauceAmt.next(), tr = transientsAmt.next();
        const double widthChange=widthOffset.next();
        const double spatialChange=spatialAmount.next();
        const double mode=creativeBlend.next();
        const double sauceMix=sauceEnhanceMix.next();
        const double analogMix=analogSauceMix.next();
        const double depthMix=depthWidthMix.next();
        const double outGain = output.next();
        const double absoluteCeiling = ceiling.next();
        const double clipAmount = clipperMix.next();
        const double targetCeiling = absoluteCeiling * 0.977237220956; // 0.2dB reconstruction margin
        const double by = bypassMix.next();
        l *= inputGain; r *= inputGain;

        // Symmetric soft saturation, level matched at its operating point.
        if (d > 1.0e-5) {
            driveShaper.processStereo(l, r, d);
            // Post-saturation high-pass removes potential near-DC energy.
            const double yl = l - dcInput[0] + dcR * dcOutput[0];
            const double yr = r - dcInput[1] + dcR * dcOutput[1];
            dcInput[0] = l; dcInput[1] = r;
            dcOutput[0] = yl; dcOutput[1] = yr;
            l = yl; r = yr;
        } else {
            // Keep HP memory in step when drive is later enabled.
            dcInput[0] = l; dcInput[1] = r;
            dcOutput[0] = l; dcOutput[1] = r;
        }

        // 2nd-order RBJ shelves; retune every ~0.33ms at 192 kHz, with smoothed gains.
        if (--eqCounter <= 0) {
            eqCounter = 64;
            // Split coefficient recalculation: automating LOWS should not
            // repeatedly recompute the HIGH shelf's expensive trig/pow
            // functions, or vice versa. Both now retain their own threshold.
            if (std::abs(eqLo - lastEqLows) > 0.00003) {
                for (int c = 0; c < channels; ++c)
                    lowShelf[static_cast<std::size_t>(c)].setShelf(rate, 115, eqLo, false);
                lastEqLows = eqLo;
            }
            if (std::abs(eqHi - lastEqHighs) > 0.00003) {
                for (int c = 0; c < channels; ++c)
                    highShelf[static_cast<std::size_t>(c)].setShelf(rate, 8200, eqHi, true);
                lastEqHighs = eqHi;
            }
        }
        l = highShelf[0].process(lowShelf[0].process(l));
        if (channels == 2) r = highShelf[1].process(lowShelf[1].process(r));
        else r = l;

        const double detector = std::max(std::abs(l), std::abs(r));
        const double glueGain = glueComp.next(detector, g, -19, 2.5, adaptiveCompressionMode);
        const double dynamicsGain = dynamicsComp.next(detector, dyn, -26, 3.6, adaptiveCompressionMode);
        l *= glueGain * dynamicsGain;
        r *= glueGain * dynamicsGain;
        if(compressionMakeupMode) {
            const double makeup=compressionMakeup.next(glueGain,dynamicsGain,g,dyn);
            if(makeup>1.) { l*=makeup; r*=makeup; }
        }

        // Stereo-linked transient attack lift, no gain pumping on sustained tails.
        const double postCompressionPeak = std::max(std::abs(l), std::abs(r));
        fastEnv = fastAtk * fastEnv + (1 - fastAtk) * postCompressionPeak;
        slowEnv = slowAtk * slowEnv + (1 - slowAtk) * postCompressionPeak;
        if (fastEnv < 1.0e-27) fastEnv = 0.0;
        if (slowEnv < 1.0e-27) slowEnv = 0.0;
        const double onset = clamp((fastEnv - slowEnv) / (slowEnv + 0.08), 0.0, 1.3);
        const double transientGain = 1.0 + tr * 0.42 * onset;
        l *= transientGain; r *= transientGain;

        // Light band-limited harmonic sheen and mild side widening, always unity at 0.
        const double hL = l - hf[0].low(l);
        const double hR = r - hf[1].low(r);
        if (s > 1.0e-5) {
            l += 0.24 * s * (std::tanh(2.3 * hL) / 2.3 - hL);
            r += 0.24 * s * (std::tanh(2.3 * hR) / 2.3 - hR);
        }
        // Bass is left essentially unchanged, and highly anti-phase material
        // is not widened further. The imaging filters track even at 0% SAUCE.
        // Preserve every factory setting (all Sauce values <=20%) and the
        // legacy curve. Above 20% add smooth, bounded high-band enhancement.
        // The same band-limited signal feeds a soft rational shaper at 4x.
        const double extra = detail::clamp((s-0.20)/0.80,0.0,1.0);
        const double color = sauceMix*extra*extra*(3.0-2.0*extra)*(1.0-mode);
        if(color>1.e-8) {
            const auto sheen=[](double h) noexcept {
                return 0.45*h/(1.0+0.75*std::abs(h));
            };
            l += color*sheen(hL);
            r += color*sheen(hR);
        }
        // Color the signal in parallel, before clip/limiting. Keep factory
        // <=20% and all older sessions on their exact previous signal path.
        const double analogAmount=color*analogMix;
        if(analogAmount>1.e-8) {
            analogColor.process(l,r,analogAmount);
            analogWasActive=true;
        } else if(analogWasActive) {
            analogColor.reset();
            analogWasActive=false;
        }
        if (channels == 2)
            stereoImager.process(l, r, s-sauceMix*std::max(0.0,s-0.20), widthChange, depthMix, efficientDepthMode);
        // A separate, additive control leaves all old Width automation intact.
        // At zero, skip the new stage entirely and retain the bit-exact path.
        const double legacySpatial=spatialChange*(1.0-mode);
        if(channels==2 && legacySpatial>1.e-8) {
            if(immersiveWidthMode) immersiveWidth.process(l,r,legacySpatial);
            else spatialSeparation.process(l,r,legacySpatial);
            spatialWasActive=true;
        } else if(spatialWasActive) {
            spatialSeparation.reset(); immersiveWidth.reset();
            spatialWasActive=false;
        }

        // Independently validated SkellCore creative residuals. Crossfade
        // the previous upper-range Sauce and spatial Width, retaining the
        // legacy low-range sheen, Drive, Glue, EQ, phase and safety stages.
        // All factories are neutral for these extra amounts.
        if(mode>1.e-9) {
            const double driveExtra=clamp((d-.20)/.80,0.,1.);
            creativeEnhancer.setAmounts(mode*extra*extra*(3.-2.*extra),
                                        channels==2 ? mode*spatialChange : 0.,
                                        mode*.65*driveExtra);
            creativeEnhancer.process(l,r);
            creativeWasActive=true;
        } else if(creativeWasActive) {
            creativeEnhancer.reset();
            creativeWasActive=false;
        }

        // Deuce Match's five-band coloration approximates the reference's
        // pre-L2 color stage. Keep it INSIDE the 4x engine so the following
        // clipper and lookahead limiter see the colored peaks before limiting.
        experimental::FiveBandColorSettings colorSettings;
        colorSettings.wet = deuceMatch ? 0.75 : 0.0;
        colorSettings.intensity = {0.38, 0.55, 0.64, 0.72, 0.84};
        fiveBandColor.process(l, r, colorSettings);

        // Exactly one 4x-rate smoother tick, shared by phase and both clip
        // placements. Legacy calibration=false never enters the phase bank.
        const double modernClip=referenceClipDrive.next();
        const double phaseWet=referencePhaseBlend.next();
        if(phaseWet>1.e-12) {
            referencePhaseActive=true;
            const double position=clamp(modernClip/.35,0.0,1.0)
                *static_cast<double>(referencePhaseTable.size()-1);
            if(std::abs(position-lastPhasePosition)>1.e-8) {
                const auto lower=static_cast<std::size_t>(position);
                const auto upper=std::min(lower+1,referencePhaseTable.size()-1);
                const double fraction=position-static_cast<double>(lower);
                for(std::size_t stage=0;stage<4;++stage) {
                    const auto& a=referencePhaseTable[lower][stage];
                    const auto& b=referencePhaseTable[upper][stage];
                    const double first=a[0]+fraction*(b[0]-a[0]);
                    const double second=a[1]+fraction*(b[1]-a[1]);
                    for(auto& channel : referencePhase)
                        channel[stage].setAllPassCoefficients(first,second);
                }
                lastPhasePosition=position;
            }
            double phaseL=l,phaseR=r;
            for(auto& filter : referencePhase[0]) phaseL=filter.process(phaseL);
            for(auto& filter : referencePhase[1]) phaseR=filter.process(phaseR);
            l+=phaseWet*(phaseL-l);
            r+=phaseWet*(phaseR-r);
        } else if(referencePhaseActive) {
            for(auto& channel : referencePhase)
                for(auto& filter : channel) filter.reset();
            referencePhaseActive=false;
            lastPhasePosition=-1.0;
        }

        l *= outGain; r *= outGain;
        // The actual FL mixer routes StandardCLIP BEFORE Waves L2.
        // User snapshot at the CLIP knob's 35% anchor:
        // Soft Clip Pro, 11% saturator, -1.20 dB clip, 0 dB input/output.
        // This is an original 4x-oversampled approximation, not SIR DSP.
        // The reference explicitly uses 8x; an 8x replacement still needs
        // a separately benchmarked latency/aliasing-safe implementation.
        double newChainClipGain=1.0;
        if(referenceClipBeforeLimiter && modernClip>1.e-6) {
            const double atReference=detail::clamp(modernClip/0.35,0.0,1.0);
            const double aboveReference=detail::clamp(
                (modernClip-0.35)/0.65,0.0,1.0);
            const double onset=atReference*atReference*
                (3.0-2.0*atReference);
            const double saturation=0.11*onset+0.24*aboveReference;
            // CLIP threshold conversion is prepared once, never pow()
            // inside the realtime 4x audio callback.
            const double lutPos=modernClip*
                static_cast<double>(referenceClipThresholdTable.size()-1);
            const auto lutLo=static_cast<std::size_t>(lutPos);
            const auto lutHi=std::min(lutLo+1,
                referenceClipThresholdTable.size()-1);
            const double lutFraction=lutPos-static_cast<double>(lutLo);
            const double threshold=referenceClipThresholdTable[lutLo]
                +lutFraction*(referenceClipThresholdTable[lutHi]-
                             referenceClipThresholdTable[lutLo]);
            const double width=0.25-0.17*aboveReference;
            const double low=threshold*(1.0-width);
            const double high=threshold*(1.0+width);
            const double range=high-low;
            const auto curve=[&](double sample) noexcept {
                const double harmonic=sample*(1.0+saturation*sample*sample);
                const double magnitude=std::abs(harmonic);
                double clipped=magnitude;
                if(magnitude>=high) clipped=threshold;
                else if(magnitude>low) {
                    const double excess=magnitude-low;
                    clipped=low+excess-0.5*excess*excess/range;
                }
                const double shaped=std::copysign(clipped,harmonic);
                return sample+onset*(shaped-sample);
            };
            const double beforePeak=std::max(std::abs(l),std::abs(r));
            l=curve(l); r=curve(r);
            const double afterPeak=std::max(std::abs(l),std::abs(r));
            // Silence must NOT count as an infinite clip reduction:
            // an inactive/no-signal clip stage has 0dB GR.
            newChainClipGain=beforePeak>1.e-24
                ? std::min(1.0,afterPeak/beforePeak) : 1.0;
        }

        // Optional oversampled soft clipping ahead of the existing lookahead
        // limiter. At zero this is exactly a no-op for old projects. The
        // rational soft knee is continuous and avoids exp/tanh on the audio
        // thread; the linked limiter and post-downsample ceiling stay active.
        double clipGain = 1.0;
        if (clipAmount > 1.0e-5) {
            const double knee = absoluteCeiling * 0.82;
            const double headroom = absoluteCeiling - knee;
            // Derive ONE soft-clip gain from the dominant stereo channel.
            // Independent L/R shaping previously changed the stereo ratio
            // on hard transients. Linked gain preserves imaging, even when
            // one channel is much louder or the programme is anti-phase.
            const double magnitude = std::max(std::abs(l), std::abs(r));
            if (magnitude > knee) {
                const double excess = magnitude - knee;
                const double rounded = knee + headroom * excess / (headroom + excess);
                const double mixed = magnitude + clipAmount * (rounded - magnitude);
                clipGain = detail::clamp(mixed / magnitude, 0.0, 1.0);
                l *= clipGain;
                r *= clipGain;
            }
        }

        // v7 five-plugin chain: L2 threshold/ceiling makeup goes INTO the
        // lookahead detector. Earlier saved states have target unity here.
        const double inputToL2=chainL2InputMakeup.next();
        l*=inputToL2; r*=inputToL2;

        // Lookahead feed-forward brickwall-style limiter. Detector looks ahead by
        // delaying the programme signal; release is stereo-linked and smooth.
        const double peak = std::max(std::abs(l), std::abs(r));
        const double wanted = peak > targetCeiling ? targetCeiling / peak : 1.0;
        const double recovery = detail::clamp((1.0 - limiterGain) * 3.0, 0.0, 1.0);
        const double fast=referenceCalibration ? referenceReleaseFast : limiterReleaseFast;
        const double slow=referenceCalibration ? referenceReleaseSlow : limiterReleaseSlow;
        const double release = fast + (slow - fast) * recovery;
        // A peak is detected ahead of the delayed programme. Hold the
        // attenuation until that peak has actually cleared the output;
        // otherwise the release starts while the transient is still queued.
        // Bypass crossfades over 12ms, so keep limiting while any audible
        // wet signal remains. Once fully dry, stop tracking the bypassed
        // programme's peaks and clear gain/hold state. Previously a loud
        // passage played in BYPASS could leave hidden attenuation in the
        // internal 65-220ms release envelope. Re-engaging on a quiet section
        // would then duck its first transients despite no limiting demand.
        // When fully wet this is exactly the original limiter path.
        if (by >= 0.9999) {
            limiterGain = 1.0;
            limiterHold = 0;
        } else if (wanted < limiterGain) {
            limiterGain = wanted;
            limiterHold = lookahead + 1;
        } else if (limiterHold > 0) {
            --limiterHold;
        } else {
            limiterGain = release * limiterGain + (1.0 - release) * wanted;
        }
        const auto pos = static_cast<std::size_t>(writeIndex);
        const double delayedL = programDelay[0][pos];
        const double delayedR = programDelay[1][pos];
        const double dryL = dryDelay[0][pos];
        const double dryR = dryDelay[1][pos];
        programDelay[0][pos] = l; programDelay[1][pos] = r;
        dryDelay[0][pos] = rawL; dryDelay[1][pos] = rawR;
        if (++writeIndex == lookahead) writeIndex = 0;

        double wetL = delayedL * limiterGain;
        double wetR = delayedR * limiterGain;
        // Deuce Match makeup follows the internal lookahead limiter. Keep it
        // ahead of this stereo-linked sample guard; the JUCE wrapper's
        // reconstructed true-peak guard remains the final host-rate boundary.
        const double deuceMakeup = deuceMatchMakeup.next();
        wetL *= deuceMakeup;
        wetR *= deuceMakeup;

        // Keep the v6 reference CLIP *unchanged* for earlier saved states.
        // Only freshly created v7 states switch to pre-limiter clipping.
        if(!referenceClipBeforeLimiter && modernClip>1.0e-6) {
            const double tablePos=modernClip*(referenceClipGainTable.size()-1);
            const auto lo=static_cast<std::size_t>(tablePos);
            const auto hi=std::min(lo+1,referenceClipGainTable.size()-1);
            const double t=tablePos-static_cast<double>(lo);
            const double push=referenceClipGainTable[lo]
                +t*(referenceClipGainTable[hi]-referenceClipGainTable[lo]);

            const double atReference=detail::clamp(modernClip/0.35,0.0,1.0);
            const double pastReference=detail::clamp((modernClip-0.35)/0.65,0.0,1.0);
            // smoothstep fades the saturation/clip curve from a strict null
            // at 0%; turning CLIP on at 1% must never engage a full knee.
            const double curveMix=atReference*atReference*(3.0-2.0*atReference);
            const double cubic=0.076*atReference+0.074*pastReference;
            const double softWidth=0.336-0.236*pastReference;
            const double clipThreshold=absoluteCeiling*(1.020-0.120*pastReference);
            const double kneeLow=clipThreshold*(1.0-softWidth);
            const double kneeHigh=clipThreshold*(1.0+softWidth);
            const double kneeRange=kneeHigh-kneeLow;

            // Per-channel odd-harmonic saturation generates genuine waveform
            // harmonics rather than stereo-linked broadband gain reduction.
            // One continuous C1 quadratic knee transitions from linear to
            // the hard ceiling (no hard corner while softness is nonzero).
            // Both channels use IDENTICAL coefficients; no independent
            // gain envelope, sidechain latency, or memory is introduced.
            const auto shape=[&](double sample) noexcept {
                const double driven=sample*push;
                const double saturated=driven*(1.0+cubic*sample*sample);
                const double magnitude=std::abs(saturated);
                double clipped=magnitude;
                if(magnitude>=kneeHigh) {
                    clipped=clipThreshold;
                } else if(magnitude>kneeLow) {
                    const double excess=magnitude-kneeLow;
                    clipped=kneeLow+excess
                        -0.5*excess*excess/kneeRange;
                }
                const double shaped=std::copysign(clipped,saturated);
                return driven+curveMix*(shaped-driven);
            };
            const double incomingPeak=std::max(std::abs(wetL),std::abs(wetR))*push;
            const double shapedL=shape(wetL);
            const double shapedR=shape(wetR);
            wetL=shapedL;
            wetR=shapedR;
            const double shapedPeak=std::max(std::abs(shapedL),std::abs(shapedR));
            // Do not count pre-clip gain or harmonic addition as gain
            // *reduction* in the meter. Only peak attenuation qualifies.
            clipGain*=std::min(1.0,shapedPeak/std::max(incomingPeak,1.e-24));
        }
        // Last-resort stereo-linked ceiling guard: do not independently
        // hard-clip L/R, which can alter the stereo ratio on unexpected
        // delayed transients or automation. This is still a sample-peak
        // safety measure, not a certified reconstructed true-peak limiter.
        const double lastPeak=std::max(std::abs(wetL),std::abs(wetR));
        if (lastPeak > absoluteCeiling) {
            const double guardGain=absoluteCeiling/lastPeak;
            wetL *= guardGain;
            wetR *= guardGain;
        }
        const double resultL = wetL * (1 - by) + dryL * by;
        const double resultR = wetR * (1 - by) + dryR * by;
        const double effectiveCompression = 1.0 - (1.0 - glueGain * dynamicsGain * limiterGain * clipGain * newChainClipGain) * (1.0 - by);
        gainReduction = std::min(gainReduction, std::max(1.0e-9, effectiveCompression));
        outputPeak = std::max(outputPeak, std::max(std::abs(resultL), std::abs(resultR)));
        return { static_cast<float>(resultL), static_cast<float>(resultR) };
    }

    double getBlockGainReductionDb() const noexcept { return detail::clamp(-detail::linearToDb(gainReduction), 0, 48); }
    double getBlockOutputPeak() const noexcept { return outputPeak; }
    int getLookaheadSamples() const noexcept { return lookahead; }
    // Diagnostic for regression coverage of the optional pre-limiter color path.
    bool isPreLimiterColorEnabled() const noexcept { return deuceMatch; }
private:
    double rate = 192000, dcR = 0.999;
    int channels = 2, lookahead = 288, writeIndex = 0, eqCounter = 0;
    std::array<std::vector<double>, 2> programDelay, dryDelay;
    detail::Smoother input, drive, glueAmt, dynamicsAmt, lows, highs, sauceAmt, transientsAmt, output, ceiling, clipperMix, referenceClipDrive, bypassMix, deuceMatchMakeup, chainL2InputMakeup;
    detail::Smoother widthOffset, sauceEnhanceMix, depthWidthMix, spatialAmount, analogSauceMix;
    AnalogSauce analogColor;
    bool analogWasActive=false;
    SpatialSeparation spatialSeparation;
    ImmersiveWidth immersiveWidth;
    skellcore::experimental::FiveBandEnhancer creativeEnhancer;
    detail::Smoother creativeBlend;
    bool creativeWasActive=false;
    bool immersiveWidthMode=false;
    bool spatialWasActive=false;
    std::array<double,1025> referenceClipGainTable {};
    std::array<double,1025> referenceClipThresholdTable {};
    std::array<detail::Biquad, 2> lowShelf, highShelf;
    std::array<detail::OnePole, 2> hf;
    StereoImager stereoImager;
    experimental::FiveBandColorPrototype fiveBandColor;
    std::array<std::array<detail::Biquad,4>,2> referencePhase;
    std::vector<std::array<std::array<double,2>,4>> referencePhaseTable;
    detail::Smoother referencePhaseBlend;
    bool referencePhaseActive=false;
    double lastPhasePosition=-1.0;
    bool deuceMatch = false;
    bool referenceClipBeforeLimiter = false;
    detail::DriveShaper driveShaper;
    detail::Compressor glueComp, dynamicsComp;
    CompressionMakeup compressionMakeup;
    std::array<double, 2> dcInput{}, dcOutput{};
    double fastEnv = 0, slowEnv = 0, fastAtk = 0, slowAtk = 0;
    double limiterGain = 1, limiterReleaseFast = 0, limiterReleaseSlow = 0;
    double referenceReleaseFast = 0, referenceReleaseSlow = 0;
    bool referenceCalibration = false;
    bool efficientDepthMode = false, adaptiveCompressionMode = false, compressionMakeupMode = false;
    int limiterHold = 0;
    double lastEqLows = 1.0e9, lastEqHighs = 1.0e9;
    double gainReduction = 1, outputPeak = 0;
};
} // namespace skelly
