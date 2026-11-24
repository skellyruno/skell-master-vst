#include "../Source/DSP/MasteringEngine.h"
#include "../Source/DSP/LoudnessMeter.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

static void require(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

// Locked scalar reference from the original 4x/12-tap-per-phase BS.1770 FIR.
// Keep this deliberately independent of the optimized live monitor: an
// incorrect coefficient order or channel/ring index must fail bitwise-level
// streaming comparisons, including non-finite inputs and circular wrap.
class ScalarTruePeakReference {
public:
    float process(float left, float right) noexcept {
        state[0][index] = skelly::detail::finiteSample(left);
        state[1][index] = skelly::detail::finiteSample(right);
        double maximum = std::max(std::abs(state[0][index]),
                                  std::abs(state[1][index]));
        for (int channel=0;channel<2;++channel)
            for (int phase=0;phase<4;++phase) {
                double sum=0.0;
                int position=index;
                for(int tap=0;tap<12;++tap) {
                    sum += state[channel][position]*coefficients[tap][phase];
                    if (--position < 0) position=11;
                }
                maximum=std::max(maximum,std::abs(sum));
            }
        if (++index == 12) index=0;
        return static_cast<float>(maximum);
    }
    void reset() noexcept {
        for(auto& c:state)c.fill(0.0);
        index=0;
    }
private:
    inline static constexpr double coefficients[12][4] = {
        { 0.001708984375, -0.0291748046875,-0.0189208984375,-0.00830078125},
        { 0.010986328125,  0.029296875,     0.0330810546875, 0.014892578125},
        {-0.0196533203125,-0.0517578125,   -0.0582275390625,-0.026611328125},
        { 0.033203125,     0.089111328125,  0.1015625,       0.047607421875},
        {-0.0594482421875,-0.16650390625,  -0.2003173828125,-0.102294921875},
        { 0.1373291015625, 0.465087890625,  0.77978515625,   0.97216796875},
        { 0.97216796875,   0.77978515625,   0.465087890625,  0.1373291015625},
        {-0.102294921875, -0.2003173828125,-0.16650390625, -0.0594482421875},
        { 0.047607421875,  0.1015625,      0.089111328125,  0.033203125},
        {-0.026611328125, -0.0582275390625,-0.0517578125,  -0.0196533203125},
        { 0.014892578125,  0.0330810546875,0.029296875,     0.010986328125},
        {-0.00830078125,  -0.0189208984375,-0.0291748046875,0.001708984375}
    };
    std::array<std::array<double,12>,2> state{};
    int index=0;
};

int main() {
    // Windows CTest captures stdout through a pipe: flush each regression
    // stage so a late test crash identifies the last passing block.
    std::cout << std::unitbuf;
    using namespace skelly;
    constexpr double rate = 192000;
    const int lookahead = static_cast<int>(rate * 0.0015);
    // K-weighting output metering: 400 ms and 3 s rectangular windows.
    // 997 Hz at -20 dBFS peak stereo should read close to -20 LUFS.
    // Mono is 3.01 LU lower and MUST NOT be counted twice.
    for (const int sr : {44100, 48000, 96000}) {
        StereoLoudnessMonitor stereo;
        StereoLoudnessMonitor mono;
        stereo.prepare(sr, 2);
        mono.prepare(sr, 1);
        require(stereo.momentaryLufs() <= -99.0 && stereo.shortTermLufs() <= -99.0,
                "LUFS meter must be silent before audio");
        constexpr double amplitude = 0.1;
        for (int i=0; i<sr*4; ++i) {
            const float x=static_cast<float>(
                amplitude*std::sin(2.0*detail::pi*997.0*i/sr));
            stereo.process(x,x);
            mono.process(x,x);
        }
        const double m=stereo.momentaryLufs();
        const double st=stereo.shortTermLufs();
        const double mMono=mono.momentaryLufs();
        require(std::abs(m+20.0)<0.32, "K-weighted momentary stereo tone calibration");
        require(std::abs(st+20.0)<0.32, "K-weighted 3s stereo tone calibration");
        require(std::abs((m-mMono)-3.01029995664)<0.03,
                "mono output must not be treated as phantom dual mono");
        for(int i=0;i<sr*4;++i) stereo.process(0.0f,0.0f);
        require(stereo.momentaryLufs()<=-99.0 && stereo.shortTermLufs()<=-99.0,
                "meter must decay to silence without stale LUFS values");
        stereo.reset();
        require(stereo.momentaryLufs()<=-99.0, "meter reset clears history");
    }
    StereoLoudnessMonitor malformedLoudness;
    malformedLoudness.prepare(48000,2);
    for (int i=0;i<1000;++i)
        malformedLoudness.process(i%2
            ? std::numeric_limits<float>::quiet_NaN()
            : std::numeric_limits<float>::infinity(), 0.0f);
    require(std::isfinite(malformedLoudness.momentaryLufs())
            && std::isfinite(malformedLoudness.shortTermLufs()),
            "malformed audio must not poison loudness monitor");
    std::cout << "PASS: 400ms/3s LUFS-M/S, 44.1/48/96k, stereo/mono, silence\n";

    // Drive normalization must track the original transfer within audible
    // precision without repeating tanh(drive) per oversampled stereo frame.
    detail::DriveShaper shaper;
    shaper.prepare();
    for (double amount : {0.0, 0.1, 0.37, 0.69, 1.0}) {
        for (double sample : {-0.7, -0.2, 0.0, 0.27, 0.75}) {
            const double a = 1.0 + 4.0 * amount;
            const double reference = sample + amount *
                         (std::tanh(sample * a) / std::tanh(a) - sample);
            require(std::abs(shaper.process(sample, amount) - reference) < 1.e-6,
                    "drive lookup retains the reference saturation shape");
        }
    }

    // DRIVE must retain the previously approved mono transfer while applying
    // the SAME instantaneous nonlinear gain to both stereo channels.
    // This guards against pan/phase shifts when one channel has stronger peaks.
    double worstMonoNull=0.0, worstStereoError=0.0;
    for (double amount : {0.0, 0.12, 0.45, 0.83, 1.0}) {
        for (double x : {-1.7, -0.8, -0.13, 0.0, 0.0001, 0.18, 0.77, 2.0}) {
            double monoL=x, monoR=x;
            shaper.processStereo(monoL,monoR,amount);
            worstMonoNull=std::max(worstMonoNull,std::abs(monoL-shaper.process(x,amount)));
            require(std::abs(monoL-monoR)<1.e-12,"linked DRIVE preserves mono");
            for (double ratio : {-1.0, -0.72, 0.0, 0.17, 0.35, 0.95}) {
                double l=x, r=x*ratio;
                shaper.processStereo(l,r,amount);
                require(std::isfinite(l) && std::isfinite(r),"linked DRIVE is finite");
                worstStereoError=std::max(worstStereoError,std::abs(r-ratio*l));
                if(amount==0.0)
                    require(l==x && r==x*ratio,"zero DRIVE is exactly neutral");
            }
        }
    }
    require(worstMonoNull<1.e-12,"DRIVE changes original mono sound");
    require(worstStereoError<1.e-12,"DRIVE changes stereo magnitude ratio");
    std::cout << "PASS: DRIVE mono null and stereo-linked saturation, max error "
              << worstStereoError << "\n";

    // 4x ITU-reference intersample monitor detects a reconstructed peak
    // higher than every actual host-rate sample. This is a monitor, not a
    // proof that the independent limiter enforces a true-peak ceiling.
    StereoTruePeakMonitor truePeak;
    truePeak.reset();
    double sampleMaximum=0.0, reconstructedMaximum=0.0;
    for(int i=0;i<2500;++i) {
        const float sample=static_cast<float>(
            0.95*std::sin(2.0*detail::pi*0.25*i+detail::pi/4.0));
        sampleMaximum=std::max(sampleMaximum,static_cast<double>(std::abs(sample)));
        const double tp=truePeak.process(sample, sample*0.7f);
        if(i>128) reconstructedMaximum=std::max(reconstructedMaximum,tp);
    }
    require(reconstructedMaximum>0.88 && reconstructedMaximum>sampleMaximum*1.25,
            "4x true-peak monitor must detect intersample peaks");
    truePeak.reset();
    require(truePeak.process(0.0f,0.0f)==0.0f,
            "true-peak history must reset to true silence");
    require(std::isfinite(truePeak.process(
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity())),
            "true-peak monitor must reject invalid input");
    std::cout << "PASS: streaming intersample peak reconstruction and reset\n";
    // A/B the optimized production implementation against the independent
    // original scalar FIR, frame by frame. These inputs cover startup,
    // steady signals, isolated impulses, stereo polarity, ring wrapping,
    // reset and malformed audio. The output audio path is unchanged.
    StereoTruePeakMonitor optimizedMonitor;
    ScalarTruePeakReference scalarMonitor;
    optimizedMonitor.reset();
    scalarMonitor.reset();
    unsigned int randomState=0x91a7f31bu;
    float worstPeakError=0.0f;
    for(int i=0;i<32000;++i) {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;
        const float noise=(static_cast<float>(randomState & 65535u)/65535.0f-0.5f);
        float left=0.75f*static_cast<float>(std::sin(0.31*i))+0.38f*noise;
        float right=(i/137)%2 ? -0.69f*left : 0.41f*left;
        if (i%1093 == 1) {left=2.3f;right=-0.62f;}
        if (i%1307 == 0) {left=0.0f;right=0.0f;}
        if (i==1010) left=std::numeric_limits<float>::quiet_NaN();
        if (i==1011) right=std::numeric_limits<float>::infinity();
        if (i==16000) {optimizedMonitor.reset();scalarMonitor.reset();}
        const float observed=optimizedMonitor.process(left,right);
        const float expected=scalarMonitor.process(left,right);
        require(std::isfinite(observed),"optimized true-peak monitor must remain finite");
        worstPeakError=std::max(worstPeakError,std::abs(observed-expected));
        require(std::abs(observed-expected)<1.0e-6f,
                "optimized true-peak phase/channel interpolation differs from reference");
    }
    std::cout << "PASS: 32k-frame true-peak scalar equivalence (max error "
              << worstPeakError << "), reset and hostile stereo input\n";

    require(std::abs(detail::ceilingOffsetToDb(0.0)+1.0)<1.e-12,
            "zero ceiling adjustment preserves -1 dBFS factory limiter");
    require(std::abs(detail::ceilingOffsetToDb(-3.0)+3.0)<1.e-12,
            "negative ceiling extreme maps to -3 dBFS");
    require(std::abs(detail::ceilingOffsetToDb(3.0)+0.1)<1.e-12,
            "positive ceiling extreme maps to -0.1 dBFS");
    for (double oldDb : {-3.0,-2.4,-1.0,-0.6,-0.1})
        require(std::abs(detail::ceilingOffsetToDb(
            detail::ceilingDbToOffset(oldDb))-oldDb)<1.e-11,
            "previous saved limiter threshold is preserved");
    std::cout << "PASS: centred ceiling map and preset migration\n";

    // Functional coverage of ALL 11 published host control identities.
    // Every knob must produce a measurable *native DSP* change on a
    // transient-rich stereo fixture, while remaining finite/stereo safe.
    // This is deliberately independent of the GUI number readout.
    struct KnobDspCase {
        const char* name;
        double MasterParameters::*field;
        double extreme;
    };
    const std::array<KnobDspCase,11> knobDspCases {{
        {"INPUT",&MasterParameters::inputDb,6.0},
        {"DRIVE",&MasterParameters::drive,0.75},
        {"GLUE",&MasterParameters::glue,0.65},
        {"DYNAMICS",&MasterParameters::dynamics,0.65},
        {"LOWS",&MasterParameters::lowsDb,3.0},
        {"HIGHS",&MasterParameters::highsDb,3.0},
        {"SAUCE",&MasterParameters::sauce,0.75},
        {"TRANSIENTS",&MasterParameters::transients,0.75},
        {"OUTPUT",&MasterParameters::outputDb,-6.0},
        {"CEILING",&MasterParameters::ceilingDb,-3.0},
        {"CLIP",&MasterParameters::clipper,0.35}
    }};
    constexpr int fixtureFrames=76000;
    const auto fixtureFrame=[&](int frame) {
        const double t=static_cast<double>(frame)/rate;
        const int offset=frame%static_cast<int>(rate*.11);
        const double kick=offset<180
            ? .33*std::exp(-static_cast<double>(offset)/37.0) : 0.0;
        const double bass=std::sin(2.0*detail::pi*73.0*t);
        const double mids=std::sin(2.0*detail::pi*730.0*t);
        const double highs=std::sin(2.0*detail::pi*10200.0*t);
        return StereoFrame {
            static_cast<float>(.53*bass+.21*mids+.10*highs+kick),
            static_cast<float>(.43*bass+.14*mids-.08*highs+.71*kick)
        };
    };
    MasterParameters knobNeutral;
    knobNeutral.referenceClip=true;
    knobNeutral.clipBeforeLimiter=true;
    // Allocate large native engine states on the heap rather than growing
    // MSVC's 1MB default test-thread stack. Linux has a larger stack, which
    // hid a Windows-only SkellyDSPTests stack overflow during CI.
    auto knobBaseEngine=std::make_unique<MasteringEngine>();
    knobBaseEngine->prepare(rate,2,lookahead);
    knobBaseEngine->setParameters(knobNeutral);
    std::vector<StereoFrame> knobBaseline;
    knobBaseline.reserve(fixtureFrames);
    for(int i=0;i<fixtureFrames;++i) {
        const auto inputFrame=fixtureFrame(i);
        knobBaseline.push_back(knobBaseEngine->processFrame(
            inputFrame.left,inputFrame.right));
    }
    for(const auto& knob : knobDspCases) {
        auto setting=knobNeutral;
        setting.*(knob.field)=knob.extreme;
        auto testedKnob=std::make_unique<MasteringEngine>();
        testedKnob->prepare(rate,2,lookahead);
        testedKnob->setParameters(setting);
        double differenceEnergy=0.0;
        double baselineEnergy=0.0;
        for(int i=0;i<fixtureFrames;++i) {
            const auto inputFrame=fixtureFrame(i);
            const auto outputFrame=testedKnob->processFrame(
                inputFrame.left,inputFrame.right);
            require(std::isfinite(outputFrame.left)&&std::isfinite(outputFrame.right),
                    "knob sweep produced non-finite audio");
            if(i>18000) {
                const double diffL=static_cast<double>(outputFrame.left)-
                    knobBaseline[static_cast<size_t>(i)].left;
                const double diffR=static_cast<double>(outputFrame.right)-
                    knobBaseline[static_cast<size_t>(i)].right;
                differenceEnergy+=diffL*diffL+diffR*diffR;
                baselineEnergy+=
                    knobBaseline[static_cast<size_t>(i)].left*
                    knobBaseline[static_cast<size_t>(i)].left+
                    knobBaseline[static_cast<size_t>(i)].right*
                    knobBaseline[static_cast<size_t>(i)].right;
            }
        }
        require(baselineEnergy>1.0 && differenceEnergy/baselineEnergy>1.e-8,
                "published knob has no measurable DSP response");
        std::cout << "PASS: host knob " << knob.name
                  << " native 4x DSP signal response (relative rms "
                  << std::sqrt(differenceEnergy/baselineEnergy) << ")\\n";
    }

    // V8 compressor knobs: published factory positions are IDENTICAL,
    // while the extreme 100% end no longer drops programme level by
    // 10-13dB unexpectedly. Their mapping remains continuous and monotone.
    for(const double maxEffective : {0.75,0.60}) {
        double prior=-1.0;
        for(int percent=0;percent<=100;++percent) {
            const double x=percent*.01;
            const double y=detail::refinedCompressorAmount(x,maxEffective);
            require(std::isfinite(y) && y>=prior-1.e-12
                    && y>=0 && y<=maxEffective+1.e-12,
                    "refined compressor curve must be finite/monotone/bounded");
            if(percent<=30)
                require(y==x,"published 0..30% compressor positions must be exact");
            prior=y;
        }
        require(std::abs(prior-maxEffective)<1.e-12,
                "refined compressor at 100% must reach intended limit");
    }
    const auto compressorProbe=[&](bool dynamics,bool refined) {
        auto dsp=std::make_unique<MasteringEngine>();
        dsp->prepare(rate,2,lookahead);
        MasterParameters p;
        p.referenceClip=true;
        p.clipBeforeLimiter=true;
        p.refinedControlCurves=refined;
        if(dynamics) p.dynamics=1.0; else p.glue=1.0;
        dsp->setParameters(p);
        double energy=0.0;
        for(int i=0;i<76000;++i) {
            const auto inputFrame=fixtureFrame(i);
            const auto out=dsp->processFrame(inputFrame.left,inputFrame.right);
            require(std::isfinite(out.left)&&std::isfinite(out.right),
                    "refined compressor generated invalid audio");
            if(i>18000)
                energy+=out.left*out.left+out.right*out.right;
        }
        return energy;
    };
    require(compressorProbe(false,true)>compressorProbe(false,false)*1.15,
            "refined 100% GLUE must avoid excessive legacy attenuation");
    require(compressorProbe(true,true)>compressorProbe(true,false)*1.15,
            "refined 100% DYNAMICS must avoid excessive legacy attenuation");

    // Exact audio null in the entire old factory compressor range. The
    // default portable engine remains legacy so restored v1-v7 sessions
    // use exactly the historic compressor response.
    MasterParameters classicPreset,refinedPreset;
    classicPreset.glue=refinedPreset.glue=0.24;
    classicPreset.dynamics=refinedPreset.dynamics=0.18;
    refinedPreset.refinedControlCurves=true;
    auto classicEngine=std::make_unique<MasteringEngine>();
    auto refinedEngine=std::make_unique<MasteringEngine>();
    classicEngine->prepare(rate,2,lookahead);
    refinedEngine->prepare(rate,2,lookahead);
    classicEngine->setParameters(classicPreset);
    refinedEngine->setParameters(refinedPreset);
    for(int i=0;i<42000;++i) {
        const auto x=fixtureFrame(i);
        const auto a=classicEngine->processFrame(x.left,x.right);
        const auto b=refinedEngine->processFrame(x.left,x.right);
        require(a.left==b.left && a.right==b.right,
                "refined compressor curves must null at old factory values");
    }
    std::cout << "PASS: all 11 knob DSP paths, v8 gentle compressor extremes, "
                 "bit-exact published compressor range\\n";

    // The new clip amount is a genuine 4x audio process; zero retains
    // all earlier sessions' transparent limiting behavior by default.
    require(MasterParameters{}.clipper == 0.0, "legacy clipper default must be zero");
    MasteringEngine referenceEngine, clipperEngine;
    referenceEngine.prepare(rate, 2, lookahead);
    clipperEngine.prepare(rate, 2, lookahead);
    MasterParameters neutral, clipped;
    clipped.clipper = 1.0;
    referenceEngine.setParameters(neutral);
    clipperEngine.setParameters(clipped);
    double changed = 0.0;
    for(int i=0;i<16000;++i) {
        const float signal=static_cast<float>(1.45*std::sin(2.0*detail::pi*820.0*i/rate));
        const auto a=referenceEngine.processFrame(signal,signal*.81f);
        const auto b=clipperEngine.processFrame(signal,signal*.81f);
        require(std::isfinite(b.left) && std::isfinite(b.right), "clipper output non-finite");
        require(std::abs(b.left)<1.0f && std::abs(b.right)<1.0f,
                "clipper must preserve output sample peak ceiling");
        if(i>8000) changed+=std::abs(a.left-b.left);
    }
    require(changed>2.0, "clipper must measurably change driven audio");
    std::cout << "PASS: 4x soft clipping, neutral default, ceiling safety\n";
    // CLIP must be stereo-linked, not two unrelated nonlinear curves.
    // Check a sustained asymmetric waveform after smoothing and lookahead.
    MasteringEngine stereoClip;
    stereoClip.prepare(rate,2,lookahead);
    MasterParameters stereoClipParams;
    stereoClipParams.clipper=1.0;
    stereoClip.setParameters(stereoClipParams);
    double clipStereoError=0.0;
    int comparedFrames=0;
    for(int i=0;i<16000;++i) {
        const float x=(i%3072<900) ? 1.9f : 0.35f;
        const auto out=stereoClip.processFrame(x,x*0.31f);
        if(i>lookahead+3000 && std::abs(out.left)>0.05f) {
            clipStereoError=std::max(clipStereoError,
                std::abs(static_cast<double>(out.right)/out.left-0.31));
            ++comparedFrames;
        }
    }
    require(comparedFrames>500 && clipStereoError<0.001,
            "oversampled CLIP must preserve asymmetric stereo ratio");
    std::cout << "PASS: linked soft-clip imaging under asymmetric transients\\n";
    // Combined DRIVE + CLIP + compressors through repeated hard hits and
    // sustained bass. Verify safe sample ceiling, finite recovery and the
    // same L/R ratio even when dynamics engage aggressively.
    MasteringEngine masteringStress;
    masteringStress.prepare(rate, 2, lookahead);
    MasterParameters masteringP;
    masteringP.drive=.83; masteringP.clipper=.64;
    masteringP.glue=.16; masteringP.dynamics=.12;
    masteringStress.setParameters(masteringP);
    double stressPeak=0.0, stressError=0.0, stressEnergy=0.0;
    int stressCompared=0;
    for(int i=0;i<static_cast<int>(rate*2.0);++i) {
        const double time=i/rate;
        const bool attack=(i%24000)<18;
        const double bass=0.16*std::sin(2*detail::pi*67.0*time);
        const float x=static_cast<float>(bass+(attack?2.1*std::exp(-(i%24000)/11.0):0.0));
        const auto out=masteringStress.processFrame(x,-x*0.36f);
        require(std::isfinite(out.left) && std::isfinite(out.right),
                "hard limiting or saturation produced invalid audio");
        stressPeak=std::max(stressPeak,static_cast<double>(std::max(std::abs(out.left),std::abs(out.right))));
        stressEnergy+=static_cast<double>(out.left)*out.left;
        if(i>lookahead+15000 && std::abs(out.left)>0.004f) {
            stressError=std::max(stressError,std::abs(out.right/out.left+0.36));
            ++stressCompared;
        }
    }
    require(stressPeak<=detail::dbToLinear(-1.0)+5.e-6,
            "hard transient train exceeded the sample ceiling");
    require(stressPeak>0.6 && stressEnergy>50.0,"transient train is muted");
    require(stressCompared>100000 && stressError<.001,
            "linked mastering chain modified stereo ratio under heavy limiting");
    std::cout << "PASS: severe DRIVE+CLIP+limiter stereo transient stress, peak "
              << stressPeak << " ratio error " << stressError << "\n";


    // Invalid input must be isolated at the boundary; no persistent poisoning.
    const auto NaN = std::numeric_limits<float>::quiet_NaN();
    const auto Inf = std::numeric_limits<float>::infinity();
    StereoPeakGuard invalidGuard;
    invalidGuard.prepare(48000.0);
    const auto invalidGuardOut = invalidGuard.process({NaN, Inf}, detail::dbToLinear(-1));
    require(std::isfinite(invalidGuardOut.left) && std::isfinite(invalidGuardOut.right),
            "final safety guard rejects non-finite input");

    MasteringEngine invalidEngine;
    invalidEngine.prepare(rate, 2, lookahead);
    MasterParameters nonneutral;
    nonneutral.drive = .7; nonneutral.glue = .4; nonneutral.dynamics = .4;
    nonneutral.lowsDb = 3.0; nonneutral.highsDb = 2.0;
    nonneutral.sauce = .6; nonneutral.transients = .6;
    invalidEngine.setParameters(nonneutral);
    for (int i=0; i<8000; ++i) {
        const auto in = i == 300 ? NaN : i == 301 ? Inf :
                        i == 302 ? -Inf : 0.05f;
        const auto out = invalidEngine.processFrame(in, -in);
        require(std::isfinite(out.left) && std::isfinite(out.right),
                "non-finite samples never poison the mastering engine");
    }
    detail::OnePole tail;
    tail.prepare(rate, 2400);
    tail.low(1.0);
    for (int i=0;i<150000;++i) tail.low(0.0);
    require(std::fpclassify(tail.low(0.0)) == FP_ZERO,
            "lowpass state decays to exact zero, without denormals");
    std::cout << "PASS: NaN/Inf recovery and denormal-safe filter tails\n";

    // The new factory5 post-L2 tonal correction is measured offline and
    // must remain stable, bounded and completely transparent when bypassed.
    detail::ReferenceToneCorrection tone;
    tone.prepare(44100.0);
    for(int i=0;i<512;++i) {
        double l=0.15*std::sin(i*2.0*detail::pi*997.0/44100.0);
        double r=-0.61*l;
        const double dryL=l,dryR=r;
        tone.process(l,r);
        require(l==dryL && r==dryR,
                "inactive reference tonal correction must be bit-exact dry");
    }
    tone.setEnabled(true);
    double originalEnergy=0.0,correctedEnergy=0.0;
    for(int i=0;i<44100;++i) {
        const double original=0.08*std::sin(i*2.0*detail::pi*1350.0/44100.0);
        double l=original,r=-0.61*original;
        tone.process(l,r);
        require(std::isfinite(l)&&std::isfinite(r),
                "tonal correction introduced invalid or unstable samples");
        if(i>22050) {
            originalEnergy+=original*original;
            correctedEnergy+=l*l;
            // The factory5 match intentionally narrows sides by -0.38dB,
            // rather than retaining the pre-correction r/l ratio. Both
            // channels must still be phase-coherent after linked M/S trim.
            constexpr double sideGain = 0.957194071294;
            constexpr double expectedStereoRatio =
                (0.195 - 0.805*sideGain)/(0.195 + 0.805*sideGain);
            require(std::abs(r-expectedStereoRatio*l)<1.e-6,
                    "reference correction must maintain linked, predictable stereo imaging");
        }
    }
    const double toneDb=10.0*std::log10(correctedEnergy/originalEnergy);
    require(toneDb>0.35 && toneDb<1.8,
            "reference tonal correction does not restore measurable mid presence");
    detail::ReferenceToneCorrection monoLowClip,monoHighClip;
    monoLowClip.prepare(44100.0); monoHighClip.prepare(44100.0);
    monoLowClip.setEnabled(true); monoHighClip.setEnabled(true);
    monoLowClip.setClipAmount(0.0,true);
    monoHighClip.setClipAmount(0.35,true);
    for(int i=0;i<12000;++i) {
        if(i==6000) monoLowClip.setClipAmount(0.35,true);
        if(i==8000) monoHighClip.setClipAmount(std::numeric_limits<double>::quiet_NaN(),true);
        const double input=.2*std::sin(i*2.0*detail::pi*733.0/44100.0);
        double a=input,b=input,c=input,d=input;
        monoLowClip.process(a,b); monoHighClip.process(c,d);
        require(std::isfinite(a) && a==b && a==c && c==d,
                "reference stereo calibration must preserve centered mono through CLIP automation");
    }
    tone.setEnabled(false);
    double maxOffError=0.0;
    for(int i=0;i<18000;++i) {
        const double dry=0.06*std::sin(i*2.0*detail::pi*439.0/44100.0);
        double l=dry,r=-0.61*dry;
        tone.process(l,r);
        if(i>5500) maxOffError=std::max(maxOffError,std::abs(l-dry));
        // The explicit wet==0 threshold eventually gives a literal null.
        // One-pole 12ms smoothing cannot be bit-exact after only 136ms.
        if(i>17500)
            require(l==dry && r==-0.61*dry,
                    "settled tonal-correction bypass must return exactly dry");
    }
    require(maxOffError<2.e-7,
            "tone disable must reach inaudible residual within 125ms");
    std::cout << "PASS: factory5 reference tone, neutral bypass, stereo link, smooth disable\\n";

    // The v9 phase/recovery profile must remain stereo-linked and bounded
    // through fast Clip automation, including invalid host values. Exercise
    // the actual engine at each oversampled Windows host rate.
    for(double hostRate : {44100.0,48000.0,96000.0}) {
        auto automated=std::make_unique<MasteringEngine>();
        automated->prepare(hostRate*4.0,2,static_cast<int>(hostRate*4.0*.0015));
        MasterParameters calibrated;
        calibrated.referenceCalibration=true;
        calibrated.referenceClip=true;
        calibrated.clipBeforeLimiter=true;
        calibrated.deuceMatch=true;
        calibrated.inputDb=5.0;
        calibrated.glue=.20;
        calibrated.sauce=.12;
        double peak=0.0;
        for(int n=0;n<90000;++n) {
            if(n%257==0) {
                constexpr double amounts[] {0.0,.07,.14,.21,.28,.35,1.0,
                    std::numeric_limits<double>::quiet_NaN()};
                calibrated.clipper=amounts[(n/257)%8];
                automated->setParameters(calibrated);
            }
            const float x=static_cast<float>(.8*std::sin(n*2.0*detail::pi*67.0/(hostRate*4.0))
                +.15*std::sin(n*2.0*detail::pi*13000.0/(hostRate*4.0)));
            const auto result=automated->processFrame(x,x);
            require(std::isfinite(result.left) && result.left==result.right,
                    "calibrated Clip automation must preserve finite centered mono");
            peak=std::max(peak,static_cast<double>(std::abs(result.left)));
        }
        require(peak<=detail::dbToLinear(-1.0)+2.e-5,
                "calibrated Clip automation escaped the existing sample ceiling");
    }
    std::cout<<"PASS: calibrated Clip automation, mono and rate safety\n";

    // Host-rate final sample-peak guard regression:
    // both channels must share one gain to preserve image and phase.
    StereoPeakGuard guard;
    guard.prepare(48000.0);
    const double guardCeiling = detail::dbToLinear(-1.0);
    const auto linked = guard.process({2.0f, -1.0f}, guardCeiling);
    require(std::abs(linked.left) <= guardCeiling + 1.e-6,
            "linked safety limiter holds left sample ceiling");
    require(std::abs(linked.right) <= guardCeiling + 1.e-6,
            "linked safety limiter holds right sample ceiling");
    require(std::abs(linked.left + 2.0f * linked.right) < 1.e-6f,
            "linked safety limiter retains stereo ratio and phase");
    const auto releaseFirst = guard.process({0.25f, -0.125f}, guardCeiling);
    require(releaseFirst.left < 0.24f, "safety recovery is smooth after over");
    for (int i = 0; i < 48000; ++i) guard.process({0.0f, 0.0f}, guardCeiling);
    require(guard.currentGain() > 0.99, "safety limiter eventually recovers unity");
    guard.reset();
    const auto neutralGuard = guard.process({0.1f, -0.1f}, guardCeiling);
    require(std::abs(neutralGuard.left - 0.1f) < 1.e-6f,
            "safety limiter transparent under ceiling");
    // Only the selected guard should track gain. Repeated hot dry frames
    // must not preload the legacy limiter with 85ms of inaudible reduction.
    guard.process({2.0f,-1.0f},guardCeiling);
    require(guard.currentGain()<0.9,
            "active legacy sample-peak guard must reduce hot frames");
    for(int i=0;i<2048;++i) {
        const auto dry=guard.process({1.75f,-0.875f},guardCeiling,true);
        require(dry.left==1.75f && dry.right==-0.875f,
                "disabled legacy guard must be byte-transparent");
    }
    require(guard.currentGain()==1.0,
            "disabled legacy guard must not retain old limiting history");
    const auto resumed=guard.process({0.20f,-0.10f},guardCeiling);
    require(std::abs(resumed.left-0.20f)<1.e-7f
            && std::abs(resumed.right+0.10f)<1.e-7f,
            "reactivated legacy guard must not duck quiet program audio");

    // Disabled true-peak reconstruction must keep the delay and detector
    // history warm; switching back to active must immediately protect peaks.
    StereoTruePeakGuard fastSwitch;
    fastSwitch.prepare(48000.0);
    for(int i=0;i<384;++i) {
        const auto dry=fastSwitch.process({1.20f,-0.60f},guardCeiling,true);
        if(i>64)
            require(std::abs(dry.left-1.20f)<1.e-6f
                    && std::abs(dry.right+0.60f)<1.e-6f,
                    "true-peak bypass must preserve exact delayed dry audio");
    }
    require(fastSwitch.currentGain()==1.0,
            "true-peak bypass cannot accumulate stale limiter gain");
    const auto reenabled=fastSwitch.process(
        {1.20f,-0.60f},guardCeiling,false);
    require(std::abs(reenabled.left)<=guardCeiling+1.e-4
            && std::abs(reenabled.right)<=guardCeiling+1.e-4,
            "true-peak guard must protect first post-bypass hot sample");
    std::cout << "PASS: both guards state-neutral during bypass, "
                 "warm true-peak history and safe immediate reactivation\n";
    std::cout << "PASS: linked final safety, gain release, neutral guard\n";

    // Test high-band only stereo imaging, bass stability and anti-phase guard.
    const auto sideGain = [rate](double sideHz, bool invertPhase) {
        StereoImager imager;
        imager.prepare(rate);
        double initialSide = 0.0, imagedSide = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 0.6); ++i) {
            const double t = static_cast<double>(i) / rate;
            const double mid = invertPhase ? 0.0 : 0.20 * std::sin(2.0 * detail::pi * 1000.0 * t);
            const double side = 0.06 * std::sin(2.0 * detail::pi * sideHz * t);
            double l = mid + side, r = mid - side;
            imager.process(l, r, 1.0);
            if (t > 0.3) {
                initialSide += side * side;
                const double imaged = 0.5 * (l-r);
                imagedSide += imaged * imaged;
            }
        }
        return std::sqrt(imagedSide / initialSide);
    };
    const double bassWidth = sideGain(60.0, false);
    const double highWidth = sideGain(3500.0, false);
    const double antiPhaseWidth = sideGain(3500.0, true);
    require(bassWidth < 1.07, "sauce protects low frequency stereo width");
    require(highWidth > 1.10 && highWidth < 1.21,
            "sauce gently widens high frequency stereo detail");
    require(std::abs(antiPhaseWidth - 1.0) < 0.005,
            "sauce avoids widening highly anti-phase stereo");
    StereoImager neutralImager;
    neutralImager.prepare(rate);
    double neutralL = 0.1, neutralR = -0.05;
    neutralImager.process(neutralL, neutralR, 0.0);
    require(std::abs(neutralL - 0.1) < 1.e-12 && std::abs(neutralR + 0.05) < 1.e-12,
            "zero-percent sauce is exactly transparent");
    std::cout << "PASS: bass-safe mid-side imaging, anti-phase protection, neutral signal\n";

    // main retains many independent engine fixtures. Keep this long-lived
    // engine on the heap so nested response probes fit Windows' 1 MB stack.
    auto dspStorage=std::make_unique<MasteringEngine>();
    auto& dsp=*dspStorage;
    dsp.prepare(rate, 2, lookahead);
    MasterParameters p;
    dsp.setParameters(p);
    dsp.beginBlock();

    // Neutral controls must be transparent, apart from the precisely specified delay.
    std::vector<float> impulse(lookahead + 512, 0.0f), output(impulse.size());
    impulse[0] = 0.2f;
    for (std::size_t i=0;i<impulse.size();++i) {
        const auto s = dsp.processFrame(impulse[i], impulse[i]);
        output[i] = s.left;
        require(std::isfinite(s.left) && std::isfinite(s.right), "finite impulse output");
    }
    for (int i=0;i<lookahead;i++) require(std::abs(output[static_cast<size_t>(i)]) < 1.e-6f, "lookahead delay initial silence");
    require(std::abs(output[static_cast<size_t>(lookahead)] - .2f) < 1.e-4f, "neutral impulse preserved at reported delay");
    require(dsp.getLookaheadSamples() == lookahead, "reported latency equals configured lookahead");

    // A peak above the ceiling must be contained even in a zero-attack transient.
    dsp.reset();
    p.ceilingDb = -1.0;
    dsp.setParameters(p);
    double maxObserved = 0;
    for (int i=0;i<220000;i++) {
        const float x = i%2048 < 9 ? 2.4f : 0.0f;
        const auto v = dsp.processFrame(x,-x);
        maxObserved = std::max(maxObserved,static_cast<double>(std::max(std::abs(v.left),std::abs(v.right))));
        require(std::isfinite(v.left) && std::isfinite(v.right), "finite hard transient output");
    }
    require(maxObserved <= std::pow(10.0,-1.0/20.0)+1.e-6, "oversampled processing ceiling is respected");
    require(maxObserved > .6, "limiter does not mute the transient");
    std::cout << "Limiting max: " << maxObserved << " at -1 dBFS ceiling\n";
    
    // A heavily limited asymmetric stereo signal must retain its L/R ratio
    // without independent hard-clipping at the final oversampled ceiling.
    dsp.prepare(rate,2,lookahead);
    p={}; p.ceilingDb=-1.0;
    dsp.setParameters(p);
    for (int i=0;i<6000;++i) dsp.processFrame(0,0);
    double maxStereoError=0.0;
    for (int i=0;i<15000;++i) {
        const float x=(i%4096<380) ? 2.2f : 0.4f;
        const auto result=dsp.processFrame(x, x*0.27f);
        if (std::abs(result.left)>0.1f) {
            maxStereoError=std::max(maxStereoError,
                std::abs(static_cast<double>(result.right)
                            / static_cast<double>(result.left)-0.27));
        }
    }
    require(maxStereoError<0.001,"limiter preserves asymmetric stereo ratios");

    // Feed an isolated transient into a settled neutral limiter. With 1.5ms
    // lookahead, release must not begin before its delayed output arrives.
    dsp.prepare(rate, 2, lookahead);
    p = {};
    dsp.setParameters(p);
    for (int i=0;i<2000;++i) dsp.processFrame(0,0);
    dsp.processFrame(2.0f, -2.0f);
    StereoFrame impulseAfterDelay;
    for (int i=0;i<lookahead;++i)
        impulseAfterDelay=dsp.processFrame(0,0);
    const double headroom = detail::dbToLinear(-1.0) * 0.977237220956;
    require(std::abs(impulseAfterDelay.left) <= headroom + 2.e-4,
            "limiter holds attenuation through delayed peak");
    require(std::abs(impulseAfterDelay.left + impulseAfterDelay.right) < 1.e-5,
            "held limiter stereo phase remains linked");

    // Two musical 2nd-order shelves: measure *steady-state* response after smoothing.
    const auto shelfGain = [rate, lookahead](double hz, double lo, double hi) {
        MasteringEngine filter;
        filter.prepare(rate, 2, lookahead);
        MasterParameters eq;
        eq.lowsDb = lo; eq.highsDb = hi;
        filter.setParameters(eq);
        double inPower = 0.0, outPower = 0.0;
        for (int i = 0; i < static_cast<int>(rate * 0.65); ++i) {
            const float x = static_cast<float>(0.04 * std::sin(i * 2.0 * detail::pi * hz / rate));
            const auto v = filter.processFrame(x, x);
            if (i >= static_cast<int>(rate * 0.40)) {
                inPower += x*x;
                outPower += v.left*v.left;
            }
        }
        return 10.0 * std::log10(outPower / inPower);
    };
    require(shelfGain(35.0, 6, 0) > 5.5, "low shelf adds six dB in bass");
    require(std::abs(shelfGain(1000.0, 6, 0)) < 0.25, "low shelf preserves mids");
    require(shelfGain(18000.0, 0, 6) > 5.0, "high shelf adds six dB in upper treble");
    require(std::abs(shelfGain(1000.0, 0, 6)) < 0.25, "high shelf preserves mids");

    dsp.prepare(rate, 2, lookahead);
    p = {};
    p.glue = 1.0;
    dsp.setParameters(p);
    dsp.beginBlock();
    for (int i=0;i<static_cast<int>(rate*0.5);++i) {
        const float x = static_cast<float>(0.7 * std::sin(2*detail::pi*120*i/rate));
        dsp.processFrame(x, x);
    }
    require(dsp.getBlockGainReductionDb() > 2, "stereo-linked glue engages when requested");

    // Bypass retains exactly the same lookahead delay without attenuating peaks.
    dsp.prepare(rate, 2, lookahead);
    p = {}; p.bypass = true;
    dsp.setParameters(p);
    for (int i=0;i<static_cast<int>(rate*0.3);++i) dsp.processFrame(0,0);
    dsp.processFrame(1.3f, -1.3f);
    StereoFrame recovered;
    for (int i=0;i<lookahead;++i) recovered = dsp.processFrame(0,0);
    require(std::abs(recovered.left-1.3f) < 1.e-5f, "bypass is dry and latency aligned");
    require(std::abs(recovered.right+1.3f) < 1.e-5f, "bypass preserves stereo phase");

    // Limiter recovery across the actual bypass state is not equivalent to
    // attenuation of a continuous hot programme. The limiter may need to
    // protect a wet/dry transition, but once the dry mix has settled it must
    // never retain hidden release/hold gain from loud bypassed samples.
    MasteringEngine reactivated;
    reactivated.prepare(rate,2,lookahead);
    MasterParameters switchMode;
    switchMode.bypass=true;
    reactivated.setParameters(switchMode);
    for(int i=0;i<static_cast<int>(rate*0.35);++i) {
        const auto dry=reactivated.processFrame(2.0f,-1.0f);
        if(i>static_cast<int>(rate*0.25))
            require(std::abs(dry.left-2.0f)<1.e-5f
                    && std::abs(dry.right+1.0f)<1.e-5f,
                    "settled bypass must keep hot samples dry and stereo coherent");
    }
    switchMode.bypass=false;
    reactivated.setParameters(switchMode);
    double resumeError=0.0;
    int resumeSamples=0;
    for(int i=0;i<static_cast<int>(rate*0.070);++i) {
        const auto wet=reactivated.processFrame(0.12f,-0.06f);
        if(i>static_cast<int>(rate*0.042)) {
            resumeError=std::max(resumeError,
                std::max(std::abs(static_cast<double>(wet.left)-0.12),
                         std::abs(static_cast<double>(wet.right)+0.06)));
            ++resumeSamples;
        }
    }
    require(resumeSamples>1000 && resumeError<0.003,
            "hot bypass must not cause stale gain reduction on quiet wet re-entry");
    std::cout<<"PASS: hot bypass to quiet wet restores limiter unity, "
             <<"maximum post-fade error "<<resumeError<<"\n";

    // Long-running stereo stress: automated controls, mono compatibility, and NaN guard.
    dsp.prepare(rate, 2, lookahead);
    for (int i=0;i<250000;i++) {
        if (i % 4096 == 0) {
            const auto k = static_cast<double>(i / 4096 % 101)/100.0;
            p.drive=k; p.glue=k; p.dynamics=k; p.sauce=k; p.transients=k;
            p.lowsDb=-6+12*k; p.highsDb=6-12*k;
            p.inputDb=8-12*k; p.outputDb=4-8*k;
            p.ceilingDb=-3+2.9*k; p.bypass=(i/4096)%7==0;
            p.deuceMatch=(i/4096)%5==0;
            dsp.setParameters(p);
        }
        const float a = static_cast<float>(1.4*std::sin(i*2*3.14159265*240.0/rate));
        const auto v=dsp.processFrame(a,static_cast<float>(a*0.16));
        require(std::isfinite(v.left) && std::isfinite(v.right), "no nonfinite output during automation stress");
        require(std::abs(v.left) < 10.0f && std::abs(v.right) < 10.0f, "numerically stable state");
    }
    dsp.prepare(rate,1,lookahead);
    p={}; dsp.setParameters(p);
    for(int i=0;i<10000;i++) {
        const auto v=dsp.processFrame(i==0?0.2f:0,0);
        require(std::isfinite(v.left), "mono finite");
    }
    std::cout << "PASS: impulse, limiter, automation, stereo, mono tests\n";

    // The user-reference SKELL MASTER factory values must be safe under a
    // dense stereo bass/transient train. This portable engine test does NOT
    // claim a perceptual match to the user's FL Studio render.
    MasteringEngine skellMasterReference;
    skellMasterReference.prepare(rate, 2, lookahead);
    MasterParameters skellPreset;
    skellPreset.inputDb=4.0; skellPreset.drive=.08;
    skellPreset.glue=.17; skellPreset.dynamics=.11;
    skellPreset.lowsDb=-1.0; skellPreset.highsDb=2.0; // Matches SKELL MASTER factory preset 5.
    skellPreset.sauce=.20; skellPreset.transients=0.0;
    skellPreset.outputDb=0.0; skellPreset.clipper=.14;
    skellPreset.ceilingDb=-1.0;
    skellMasterReference.setParameters(skellPreset);
    double referencePeak=0.0, referenceEnergy=0.0;
    for(int i=0;i<static_cast<int>(rate);++i) {
        const double time=static_cast<double>(i)/rate;
        const double kick=(i%23000)<90
            ? 1.3*std::exp(-static_cast<double>(i%23000)/29.0):0.0;
        const double bass=.36*std::sin(2*detail::pi*58.0*time);
        const double vocal=.12*std::sin(2*detail::pi*910.0*time);
        const float l=static_cast<float>(kick+bass+vocal);
        const float r=static_cast<float>(kick*.92+bass*.89+vocal*.58);
        const auto outputFrame=skellMasterReference.processFrame(l,r);
        require(std::isfinite(outputFrame.left) && std::isfinite(outputFrame.right),
                "SKELL MASTER reference preset generated nonfinite audio");
        referencePeak=std::max(referencePeak,
            static_cast<double>(std::max(std::abs(outputFrame.left),
                                         std::abs(outputFrame.right))));
        referenceEnergy+=static_cast<double>(outputFrame.left)*outputFrame.left;
    }
    require(referencePeak<=detail::dbToLinear(-1.0)+1.e-5,
            "SKELL MASTER reference preset exceeded its sample ceiling");
    require(referenceEnergy>10.0,"SKELL MASTER reference preset muted program");
    std::cout << "PASS: SKELL MASTER first-pass portable DSP stress and sample ceiling "
              << referencePeak << "\\n";

    // Deuce Match makeup is deliberately after the engine's internal limiter.
    // At low level it should add the measured reference loudness offset without
    // changing the default/legacy path, and bypass must remain latency-aligned dry.
    MasteringEngine deuceMatchOff, deuceMatchOn;
    deuceMatchOff.prepare(rate, 2, lookahead);
    deuceMatchOn.prepare(rate, 2, lookahead);
    MasterParameters deuceOff, deuceOn;
    deuceOn.deuceMatch = true;
    deuceMatchOff.setParameters(deuceOff);
    deuceMatchOn.setParameters(deuceOn);
    double offEnergy = 0.0, onEnergy = 0.0;
    for (int i = 0; i < static_cast<int>(rate * 0.5); ++i) {
        const float x = static_cast<float>(0.01 * std::sin(
            2.0 * detail::pi * 997.0 * i / rate));
        const auto offFrame = deuceMatchOff.processFrame(x, x * 0.83f);
        const auto onFrame = deuceMatchOn.processFrame(x, x * 0.83f);
        if (i > static_cast<int>(rate * 0.1)) {
            offEnergy += static_cast<double>(offFrame.left) * offFrame.left
                       + static_cast<double>(offFrame.right) * offFrame.right;
            onEnergy += static_cast<double>(onFrame.left) * onFrame.left
                      + static_cast<double>(onFrame.right) * onFrame.right;
        }
        require(std::isfinite(onFrame.left) && std::isfinite(onFrame.right),
                "Deuce Match makeup must remain finite");
    }
    // At this 997 Hz probe the enabled five-band color contributes about
    // 1.15 dB; the new post-limiter makeup contributes another 2.4 dB.
    // Compare the complete Deuce Match mode against neutral, not makeup alone.
    const double deuceMatchGainDb = 10.0 * std::log10(onEnergy / offEnergy);
    require(deuceMatchGainDb > 3.3 && deuceMatchGainDb < 3.8,
            "Deuce Match must include five-band color plus 2.4 dB post-limiter makeup");
    std::cout << "PASS: Deuce Match combined color + post-limiter makeup "
              << deuceMatchGainDb << " dB\\n";

    MasteringEngine bypassWithDeuce, bypassNeutral;
    bypassWithDeuce.prepare(rate, 2, lookahead);
    bypassNeutral.prepare(rate, 2, lookahead);
    MasterParameters bypassDeuce, bypassPlain;
    bypassDeuce.bypass = true;
    bypassDeuce.deuceMatch = true;
    bypassPlain.bypass = true;
    bypassWithDeuce.setParameters(bypassDeuce);
    bypassNeutral.setParameters(bypassPlain);
    double bypassNull = 0.0;
    for (int i = 0; i < 12000; ++i) {
        const float x = static_cast<float>(0.02 * std::sin(
            2.0 * detail::pi * 440.0 * i / rate));
        const auto a = bypassWithDeuce.processFrame(x, x * 0.77f);
        const auto b = bypassNeutral.processFrame(x, x * 0.77f);
        bypassNull = std::max(bypassNull,
            std::max(std::abs(static_cast<double>(a.left) - b.left),
                     std::abs(static_cast<double>(a.right) - b.right)));
    }
    require(bypassNull < 1.0e-7,
            "Deuce Match makeup must not change the latency-aligned bypass path");
    std::cout << "PASS: Deuce Match makeup is absent in bypass (null "
              << bypassNull << ")\\n";

    // The new owner-reference CLIP is a second, 4x oversampled signal path.
    // Its zero position must null with the old engine; 50/100 must actually
    // drive the output into soft clipping, not merely blend its wet fraction.
    MasteringEngine modernZero, oldZero;
    modernZero.prepare(rate,2,lookahead);
    oldZero.prepare(rate,2,lookahead);
    MasterParameters modernZeroP;
    modernZeroP.referenceClip=true;
    modernZero.setParameters(modernZeroP);
    oldZero.setParameters(MasterParameters{});
    double nullError=0.0;
    for(int i=0;i<16000;++i) {
        const float x=static_cast<float>(.31*std::sin(2*detail::pi*740*i/rate));
        const auto a=modernZero.processFrame(x,x*.63f);
        const auto b=oldZero.processFrame(x,x*.63f);
        nullError=std::max(nullError,std::max(
            std::abs(static_cast<double>(a.left)-b.left),
            std::abs(static_cast<double>(a.right)-b.right)));
    }
    require(nullError<1.e-7,"Reference CLIP at zero must be identical to legacy neutral");

    // Very low knob values should be a small continuous change, not
    // full-strength clipping abruptly enabled after a zero setting.
    MasteringEngine refOnePercent, refOnePercentDry;
    refOnePercent.prepare(rate,2,lookahead);
    refOnePercentDry.prepare(rate,2,lookahead);
    MasterParameters pOne;
    pOne.referenceClip=true;
    pOne.clipper=.01;
    MasterParameters pZero;
    pZero.referenceClip=true;
    refOnePercent.setParameters(pOne);
    refOnePercentDry.setParameters(pZero);
    double maxLowKnobDiff=0.0;
    for(int i=0;i<16000;++i) {
        const float x=static_cast<float>(.85*std::sin(2*detail::pi*630*i/rate));
        const auto a=refOnePercent.processFrame(x,x*.79f);
        const auto b=refOnePercentDry.processFrame(x,x*.79f);
        if(i>8000)
            maxLowKnobDiff=std::max(maxLowKnobDiff,
                std::abs(static_cast<double>(a.left)-b.left));
    }
    require(maxLowKnobDiff<.03,
            "Reference CLIP 1% must not suddenly engage its full nonlinear knee");

    MasteringEngine refClean,ref35,refMedium,refHard,refHardSwapped;
    for(auto* e : {&refClean,&ref35,&refMedium,&refHard,&refHardSwapped})
        e->prepare(rate,2,lookahead);
    MasterParameters baseClip;
    baseClip.referenceClip=true;
    MasterParameters reference35=baseClip, mediumClip=baseClip, hardClip=baseClip;
    reference35.clipper=.35;
    mediumClip.clipper=.5;
    hardClip.clipper=1.0;
    refClean.setParameters(baseClip);
    ref35.setParameters(reference35);
    refMedium.setParameters(mediumClip);
    refHard.setParameters(hardClip);
    refHardSwapped.setParameters(hardClip);
    double cleanEnergy=0.0, refEnergy=0.0, medEnergy=0.0, hardEnergy=0.0;
    double refPeak=0.0, medPeak=0.0, hardPeak=0.0, swapError=0.0;
    for(int i=0;i<static_cast<int>(rate*.8);++i) {
        const double t=static_cast<double>(i)/rate;
        const float x=static_cast<float>(.30*std::sin(2*detail::pi*440*t)
                                         +.13*std::sin(2*detail::pi*63*t));
        const auto c=refClean.processFrame(x,x*.71f);
        const auto r=ref35.processFrame(x,x*.71f);
        const auto m=refMedium.processFrame(x,x*.71f);
        const auto h=refHard.processFrame(x,x*.71f);
        const auto swapped=refHardSwapped.processFrame(x*.71f,x);
        require(std::isfinite(r.left)&&std::isfinite(r.right)
                &&std::isfinite(h.left)&&std::isfinite(h.right),
                "Reference CLIP generated a non-finite sample");
        if(i>static_cast<int>(rate*.20)) {
            cleanEnergy+=c.left*c.left;
            refEnergy+=r.left*r.left;
            medEnergy+=m.left*m.left;
            hardEnergy+=h.left*h.left;
            refPeak=std::max(refPeak,std::abs(static_cast<double>(r.left)));
            medPeak=std::max(medPeak,std::abs(static_cast<double>(m.left)));
            hardPeak=std::max(hardPeak,std::abs(static_cast<double>(h.left)));
            // Unlike the OLD linked pre-limit clip, this is actual
            // per-channel waveshaping. Preserve channel-swap symmetry,
            // not the exact L/R amplitude ratio (that would forbid it).
            swapError=std::max(swapError,std::max(
                std::abs(static_cast<double>(h.left)-swapped.right),
                std::abs(static_cast<double>(h.right)-swapped.left)));
        }
    }
    require(refEnergy>cleanEnergy*1.05 && medEnergy>refEnergy*1.02
            && hardEnergy>medEnergy*1.02,
            "Reference CLIP must progressively raise program energy from 0 to 35/50/100%");
    require(refPeak<detail::dbToLinear(-1.0)+1.e-5 &&
            medPeak<detail::dbToLinear(-1.0)+1.e-5 &&
            hardPeak<detail::dbToLinear(-1.0)+1.e-5,
            "Reference CLIP exceeded the configured sample ceiling");
    require(swapError<1.e-6,"Stereo channel swap must commute with clipping");

    // A 35% clip must make *new harmonics*, not just apply gain. Compare
    // the best level-matched linear waveform to the shaped waveform; a
    // nonzero residual proves nonlinear distortion at this reference point.
    MasteringEngine toneDry, tone35;
    toneDry.prepare(rate,2,lookahead);
    tone35.prepare(rate,2,lookahead);
    toneDry.setParameters(baseClip);
    tone35.setParameters(reference35);
    double xx=0.0,xy=0.0,yy=0.0;
    int toneFrames=0;
    for(int i=0;i<static_cast<int>(rate*.6);++i) {
        const float sine=static_cast<float>(
            .45*std::sin(2.0*detail::pi*330.0*i/rate));
        const auto dry=toneDry.processFrame(sine,sine);
        const auto wet=tone35.processFrame(sine,sine);
        if(i>static_cast<int>(rate*.25)) {
            xx+=dry.left*dry.left;
            xy+=dry.left*wet.left;
            yy+=wet.left*wet.left;
            ++toneFrames;
        }
    }
    const double nonlinearResidual=std::sqrt(
        std::max(0.0,(yy-xy*xy/std::max(xx,1.e-20))/toneFrames));
    require(nonlinearResidual>1.e-4,
            "35% reference CLIP must add audible-band saturation, not only volume");
    std::cout << "PASS: 35% nonlinear reference tone, progressive clip, ceiling and stereo symmetry\\n";

    // 35% owner snapshot is NOT +2.23dB input drive. Verify v7
    // Soft Clip Pro approximation actually clips peak transients ahead of
    // the L2-style lookahead limiter and keeps the old engine's latency.
    MasteringEngine beforeL2Clean, beforeL235, beforeL270, beforeL2Swapped;
    for(auto* e : {&beforeL2Clean,&beforeL235,&beforeL270,&beforeL2Swapped})
        e->prepare(rate,2,lookahead);
    MasterParameters beforeParams;
    beforeParams.referenceClip=true;
    beforeParams.clipBeforeLimiter=true;
    MasterParameters before35=beforeParams, before70=beforeParams;
    before35.clipper=.35;
    before70.clipper=.70;
    beforeL2Clean.setParameters(beforeParams);
    beforeL235.setParameters(before35);
    beforeL270.setParameters(before70);
    beforeL2Swapped.setParameters(before70);
    double curveDelta35=0.0, curveDelta70=0.0, swapDelta=0.0;
    double cleanPower35=0.0, clippedPower35=0.0;
    int compared=0;
    for(int i=0;i<static_cast<int>(rate*.65);++i) {
        const double t=static_cast<double>(i)/rate;
        const float value=static_cast<float>(
            .77*std::sin(2*detail::pi*1213.0*t)
            +.10*std::sin(2*detail::pi*67.0*t));
        const auto dry=beforeL2Clean.processFrame(value,value*.73f);
        const auto snap=beforeL235.processFrame(value,value*.73f);
        const auto hard=beforeL270.processFrame(value,value*.73f);
        const auto rev=beforeL2Swapped.processFrame(value*.73f,value);
        require(std::isfinite(snap.left)&&std::isfinite(hard.left),
                "chain-ordered clip must remain finite");
        if(i>static_cast<int>(rate*.25)) {
            curveDelta35+=std::abs(static_cast<double>(snap.left-dry.left));
            curveDelta70+=std::abs(static_cast<double>(hard.left-dry.left));
            cleanPower35+=dry.left*dry.left;
            clippedPower35+=snap.left*snap.left;
            swapDelta=std::max(swapDelta,std::max(
                std::abs(static_cast<double>(hard.left-rev.right)),
                std::abs(static_cast<double>(hard.right-rev.left))));
            ++compared;
        }
    }
    require(curveDelta35/compared>.0001,
            "v7 CLIP at 35% must make actual nonlinear shape, not fake wet gain");
    require(curveDelta70>curveDelta35*1.02,
            "v7 CLIP must develop progressively more peak shaping above 35%");
    // Silence must never report huge fake clip gain reduction when
    // CLIP is enabled. This is important for live UI GR metering.
    MasteringEngine chainSilence;
    chainSilence.prepare(rate,2,lookahead);
    chainSilence.setParameters(before35);
    chainSilence.beginBlock();
    for(int i=0;i<static_cast<int>(rate*.10);++i)
        chainSilence.processFrame(0.0f,0.0f);
    require(chainSilence.getBlockGainReductionDb()<0.01,
            "silent v7 CLIP must not report bogus clip gain reduction");

    // The screenshot has 0dB I/O gain, but harmonic saturation can
    // legitimately change RMS for loud material. Test near-linear low-level
    // gain directly instead of incorrectly requiring driven RMS to fall.
    MasteringEngine low35,low0;
    low35.prepare(rate,2,lookahead);
    low0.prepare(rate,2,lookahead);
    low35.setParameters(before35);
    low0.setParameters(beforeParams);
    double lowWet=0.0,lowDry=0.0;
    for(int i=0;i<static_cast<int>(rate*.50);++i) {
        const float sample=static_cast<float>(
            .04*std::sin(2*detail::pi*487.0*i/rate));
        const auto a=low35.processFrame(sample,sample);
        const auto b=low0.processFrame(sample,sample);
        if(i>static_cast<int>(rate*.20)) {
            lowWet+=a.left*a.left;
            lowDry+=b.left*b.left;
        }
    }
    require(lowDry>1.e-8 && std::abs(
            10.0*std::log10(lowWet/lowDry))<0.05,
            "35% clip must retain ~0dB low-level gain when I/O gains are zero");
    require(swapDelta<1.e-6,
            "v7 per-channel clip must preserve channel-swap symmetry");
    require(beforeL235.getLookaheadSamples()==beforeL2Clean.getLookaheadSamples(),
            "v7 pre-L2 clip must retain original fixed plugin latency");
    std::cout << "PASS: v7 pre-L2 35% shape, stronger 70%, symmetry, no extra PDC\\n";

    // Owner L2 uses threshold -2.8dB / ceiling -1.1dB.
    // v7 places the +1.7dB makeup before limiting, while v6 stays on
    // its original opt-in +2.4dB post-limiter path.
    MasteringEngine chainL2, historicDeuce;
    chainL2.prepare(rate,2,lookahead);
    historicDeuce.prepare(rate,2,lookahead);
    MasterParameters chainP,historicP;
    chainP.referenceClip=historicP.referenceClip=true;
    chainP.clipBeforeLimiter=true;
    historicP.clipBeforeLimiter=false;
    chainP.deuceMatch=historicP.deuceMatch=true;
    chainP.clipper=historicP.clipper=0.0;
    chainL2.setParameters(chainP);
    historicDeuce.setParameters(historicP);
    double chainE=0.0,historicE=0.0;
    for(int i=0;i<static_cast<int>(rate*.65);++i) {
        const float x=static_cast<float>(
            .035*std::sin(2.0*detail::pi*390.0*i/rate));
        const auto newer=chainL2.processFrame(x,x*.8f);
        const auto older=historicDeuce.processFrame(x,x*.8f);
        require(std::isfinite(newer.left)&&std::isfinite(older.left),
                "both L2-style makeup placements must stay finite");
        if(i>static_cast<int>(rate*.25)) {
            chainE+=newer.left*newer.left;
            historicE+=older.left*older.left;
        }
    }
    const double inputVsPostDb=10.0*std::log10(
        chainE/std::max(historicE,1.e-20));
    require(inputVsPostDb>-1.2 && inputVsPostDb<-0.2,
            "v7 pre-L2 +1.7dB and v6 post-L2 +2.4dB makeup must remain distinct");
    require(chainL2.getLookaheadSamples()==historicDeuce.getLookaheadSamples(),
            "v7 L2 makeup placement must not change host latency");
    std::cout << "PASS: five-plugin pre-L2 +1.7dB, stable v6 post-L2 +2.4dB and PDC\\n";

    // New opt-in final API-550-like EQ. The curve is post-limiter,
    // center ~1.4kHz, +0.8dB Q~0.78, warm states, no signal delay.
    // Existing factory paths process at wet=0 and must remain exact.
    detail::ReferenceFinalEq neutralFinal;
    neutralFinal.prepare(44100.0);
    double eqNeutralLeft=.34, eqNeutralRight=-.13;
    neutralFinal.process(eqNeutralLeft,eqNeutralRight);
    require(eqNeutralLeft==.34 && eqNeutralRight==-.13,
            "disabled final EQ must be bit-identical, not alter old presets");
    const auto measureEqGain=[](double hz) {
        detail::ReferenceFinalEq finalTone;
        finalTone.prepare(48000.0);
        finalTone.setEnabled(true);
        double inEnergy=0.0,outEnergy=0.0,stereoError=0.0;
        for(int i=0;i<48000;++i) {
            const double phase=2.0*detail::pi*hz*i/48000.0;
            const double source=.15*std::sin(phase);
            double left=source, right=source*.71;
            finalTone.process(left,right);
            require(std::isfinite(left)&&std::isfinite(right),
                    "final EQ must stay finite");
            if(i>12000) {
                inEnergy+=source*source;
                outEnergy+=left*left;
                if(std::abs(left)>.02)
                    stereoError=std::max(stereoError,std::abs(right/left-.71));
            }
        }
        require(stereoError<1.e-5,"final EQ must preserve stereo-linked tone");
        return 10.0*std::log10(outEnergy/std::max(inEnergy,1.e-20));
    };
    const double midGain=measureEqGain(1400.0);
    const double bassGain=measureEqGain(80.0);
    const double trebleGain=measureEqGain(16000.0);
    require(midGain>.68 && midGain<.91,
            "reference API-like bell must boost 1.4kHz by ~0.8dB");
    require(std::abs(bassGain)<.08 && std::abs(trebleGain)<.08,
            "new 1.4kHz bell must not cause unintended bass/air boosts");
    std::cout << "PASS: opt-in final mid bell, neutral old factory, stereo and PDC\\n";

    // A restored pre-v6 project uses the original CLIP engine. No new
    // algorithm may alter its 14% SKELL MASTER or other old CLIP automation.
    require(!MasterParameters{}.referenceClip,
            "Portable default must preserve legacy preset compatibility");
    MasteringEngine old14,old14Again;
    old14.prepare(rate,2,lookahead);
    old14Again.prepare(rate,2,lookahead);
    MasterParameters oldClip;
    oldClip.clipper=.14;
    old14.setParameters(oldClip);
    old14Again.setParameters(oldClip);
    double oldNull=0.0;
    for(int i=0;i<16000;++i) {
        const float x=static_cast<float>(.53*std::sin(2*detail::pi*217*i/rate));
        const auto a=old14.processFrame(x,x*.84f);
        const auto b=old14Again.processFrame(x,x*.84f);
        oldNull=std::max(oldNull,std::max(
            std::abs(static_cast<double>(a.left)-b.left),
            std::abs(static_cast<double>(a.right)-b.right)));
    }
    require(oldNull==0.0,"Legacy 14% CLIP DSP must stay deterministic");
    std::cout << "PASS: new 4x 35%-anchored CLIP, legacy path deterministic\\n";

    // Across the *entire* mastering chain, hostile NaN/Inf automation
    // should fall back to the old neutral settings, never unexpectedly
    // switch on maximum drive/compression/treble or an unsafe ceiling.
    MasteringEngine safeHost, neutralHost;
    safeHost.prepare(rate, 2, lookahead);
    neutralHost.prepare(rate, 2, lookahead);
    MasterParameters malformedParams;
    const double invalidNumber = std::numeric_limits<double>::quiet_NaN();
    malformedParams.inputDb = invalidNumber;
    malformedParams.drive = invalidNumber;
    malformedParams.glue = invalidNumber;
    malformedParams.dynamics = invalidNumber;
    malformedParams.lowsDb = invalidNumber;
    malformedParams.highsDb = invalidNumber;
    malformedParams.sauce = invalidNumber;
    malformedParams.transients = invalidNumber;
    malformedParams.outputDb = invalidNumber;
    malformedParams.clipper = invalidNumber;
    malformedParams.ceilingDb = invalidNumber;
    safeHost.setParameters(malformedParams);
    neutralHost.setParameters(MasterParameters{});
    double invalidParameterNull = 0.0;
    for (int i = 0; i < 36000; ++i) {
        const double x = 0.23 * std::sin(2.0 * detail::pi * 330.0 * i / rate)
                       + 0.07 * std::sin(2.0 * detail::pi * 4720.0 * i / rate);
        const float l = static_cast<float>(x);
        const float r = static_cast<float>(x * 0.61);
        const auto a = safeHost.processFrame(l, r);
        const auto b = neutralHost.processFrame(l, r);
        require(std::isfinite(a.left) && std::isfinite(a.right),
                "nonfinite automation poisoned full DSP");
        invalidParameterNull = std::max(invalidParameterNull,
            std::max(std::abs(static_cast<double>(a.left - b.left)),
                     std::abs(static_cast<double>(a.right - b.right))));
    }
    require(invalidParameterNull < 1.0e-7,
            "invalid mastering parameters must all fall back to neutral sound");
    // Large but finite samples are supported by the portable processing
    // boundary as well as the production JUCE host wrapper. Test both wet
    // and the latency-aligned dry/bypass route.
    safeHost.reset();
    MasterParameters dryAttack;
    dryAttack.bypass = true;
    safeHost.setParameters(dryAttack);
    for (int i = 0; i < 10000; ++i) {
        const float huge = (i % 47 == 0)
            ? std::numeric_limits<float>::max() : 0.0f;
        const auto v = safeHost.processFrame(huge, -huge);
        require(std::isfinite(v.left) && std::isfinite(v.right),
                "very large finite audio overflowed wet or bypass route");
        require(std::abs(v.left) <= 256.001f && std::abs(v.right) <= 256.001f,
                "portable input boundary disagrees with JUCE +48dBFS guard");
    }
    // During aggressive LOWS and HIGHS automation, independent shelf
    // recalculation must preserve finite stereo audio and pre-existing
    // host-rate sample peaks.
    MasteringEngine modulatedEq;
    modulatedEq.prepare(rate, 2, lookahead);
    MasterParameters modulated;
    double modulatedPeak = 0.0;
    for (int i = 0; i < 70000; ++i) {
        if ((i % 800) == 0) {
            modulated.lowsDb = (i / 800) % 2 ? -6.0 : 6.0;
            modulated.highsDb = (i / 800) % 3 ? 0.0 : 6.0;
            modulatedEq.setParameters(modulated);
        }
        const float x = static_cast<float>(
            0.28 * std::sin(2.0 * detail::pi * 88.0 * i / rate)
          + 0.22 * std::sin(2.0 * detail::pi * 11000.0 * i / rate));
        const auto v = modulatedEq.processFrame(x, -0.6f * x);
        require(std::isfinite(v.left) && std::isfinite(v.right),
                "fast multiband EQ automation poisoned audio");
        modulatedPeak = std::max(modulatedPeak,
            static_cast<double>(std::max(std::abs(v.left), std::abs(v.right))));
    }
    require(modulatedPeak <= detail::dbToLinear(-1.0) + 2.0e-5,
            "automated EQ escaped the mastering sample ceiling");
    std::cout << "PASS: full-chain invalid automation neutral null "
              << invalidParameterNull
              << "; huge input/bypass, independent EQ coefficient automation "
              << modulatedPeak << "\\n";

    // Offline CPU sanity probe (time is recorded, not treated as a universal
    // benchmark across shared CI runners). Real-time ratio uses 4x oversampling:
    // rate is the actual processing rate of processFrame(), not the host rate.
    MasteringEngine bench;
    bench.prepare(rate, 2, lookahead);
    MasterParameters busy;
    busy.drive=.8; busy.glue=.65; busy.dynamics=.55;
    busy.lowsDb=3; busy.highsDb=3; busy.sauce=.8; busy.transients=.55;
    bench.setParameters(busy);
    constexpr int frames = 192000;
    const auto start = std::chrono::steady_clock::now();
    volatile float sink = 0.0f;
    for (int i=0;i<frames;++i) {
        const float a = static_cast<float>(0.3*std::sin(i*2.0*detail::pi*213.0/rate));
        const auto v=bench.processFrame(a,-a*.72f);
        require(std::isfinite(v.left) && std::isfinite(v.right), "busy DSP stays finite");
        sink = sink + v.left * 1.e-9f;
    }
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now()-start).count();
    const double audioSeconds = static_cast<double>(frames)/rate;
    std::cout << "DSP CPU probe: " << elapsed*1000.0 << " ms / "
              << audioSeconds*1000.0 << " ms audio ("
              << elapsed/audioSeconds*100.0 << "% one core realtime), sink="
              << sink << "\n";
    require(elapsed < audioSeconds*8.0, "DSP performance pathological slowdown");
}
