#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

SkellyMasterAudioProcessor::SkellyMasterAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "SKELLY_MASTER_PARAMETERS", createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout SkellyMasterAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto add = [&](const char* id, const char* name, float min, float max,
                         float step, float initial, const char* units = "") {
        auto range = juce::NormalisableRange<float>(min, max, step);
        auto param = std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{id, 1}, name, range, initial,
            juce::AudioParameterFloatAttributes().withLabel(units));
        layout.add(std::move(param));
    };
    add("input",      "Input",       -18.0f, 18.0f, 1.0f,   0.0f, "dB");
    add("drive",      "Drive",         0.0f, 100.0f, 1.0f,  0.0f, "%");
    add("glue",       "Glue",          0.0f, 100.0f, 1.0f,  0.0f, "%");
    add("dynamics",   "Dynamics",      0.0f, 100.0f, 1.0f,  0.0f, "%");
    add("lows",       "Lows",         -6.0f,   6.0f, 1.0f,  0.0f, "dB");
    add("highs",      "Highs",        -6.0f,   6.0f, 1.0f,  0.0f, "dB");
    add("sauce",      "Sauce",         0.0f, 100.0f, 1.0f,  0.0f, "%");
    add("transients", "Transients",    0.0f, 100.0f, 1.0f,  0.0f, "%");
    add("output",     "Output Trim", -18.0f, 12.0f, 1.0f,  0.0f, "dB");
    add("ceiling",    "Ceiling Adjust", -3.0f, 3.0f, 1.0f, 0.0f, "step");
    add("clipper",    "Clip", 0.0f, 100.0f, 1.0f, 0.0f, "%");
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));
    // Appended so all pre-existing host parameter IDs and order remain stable.
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"deuceMatch", 1}, "Deuce Match", false));
    return layout;
}

bool SkellyMasterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto input = layout.getMainInputChannelSet();
    return (input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo())
        && input == layout.getMainOutputChannelSet();
}

void SkellyMasterAudioProcessor::prepareToPlay(double sampleRate, int maxBlock) {
    activeChannels = juce::jlimit(1, 2, getTotalNumInputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        static_cast<size_t>(activeChannels), 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true, true);
    oversampler->initProcessing(static_cast<size_t>(juce::jmax(16384, maxBlock)));
    oversampler->reset();

    // 1.5 ms of lookahead, exactly divisible by 4 to keep host latency integral.
    const double highRate = sampleRate * static_cast<double>(oversamplingFactor);
    lookaheadSamples = juce::jmax(4, static_cast<int>(std::ceil(sampleRate * 0.0015)) * oversamplingFactor);
    engine.prepare(highRate, activeChannels, lookaheadSamples);
    referenceClipGuardHold=0;
    postLimiterPresence.prepare(sampleRate);
    postLimiterMidPresence.prepare(sampleRate);
    legacySafetyGuard.prepare(sampleRate);
    safetyGuard.prepare(sampleRate);
    setLatencySamples(static_cast<int>(std::lround(oversampler->getLatencyInSamples()))
                      + lookaheadSamples / oversamplingFactor
                      + safetyGuard.getLatencySamples());
    truePeakMonitor.reset();
    loudnessMonitor.prepare(sampleRate,activeChannels);
    peakLeft.store(0); peakRight.store(0); reduction.store(0); truePeak.store(0);
    momentaryLufs.store(-100.0f); shortTermLufs.store(-100.0f);
}

void SkellyMasterAudioProcessor::releaseResources() {
    postLimiterPresence.reset();
    postLimiterMidPresence.reset();
    legacySafetyGuard.reset();
    safetyGuard.reset();
    truePeakMonitor.reset();
    loudnessMonitor.reset();
    oversampler.reset();
    setLatencySamples(0);
}

void SkellyMasterAudioProcessor::reset() {
    if (oversampler) oversampler->reset();
    engine.reset();
    referenceClipGuardHold=0;
    postLimiterPresence.reset();
    postLimiterMidPresence.reset();
    legacySafetyGuard.reset();
    safetyGuard.reset();
    truePeakMonitor.reset();
    loudnessMonitor.reset();
    peakLeft.store(0); peakRight.store(0); reduction.store(0); truePeak.store(0);
    momentaryLufs.store(-100.0f); shortTermLufs.store(-100.0f);
}

void SkellyMasterAudioProcessor::atomicMax(std::atomic<float>& dest, float next) noexcept {
    float old = dest.load(std::memory_order_relaxed);
    while (next > old && !dest.compare_exchange_weak(
               old, next, std::memory_order_relaxed, std::memory_order_relaxed)) {}
}

void SkellyMasterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(activeChannels, buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    for (int c = channels; c < buffer.getNumChannels(); ++c)
        buffer.clear(c, 0, samples);
    if (channels < 1 || !oversampler || samples == 0) return;

    skelly::MasterParameters p;
    p.inputDb = parameters.getRawParameterValue("input")->load(std::memory_order_relaxed);
    p.drive = parameters.getRawParameterValue("drive")->load(std::memory_order_relaxed) * 0.01;
    p.glue = parameters.getRawParameterValue("glue")->load(std::memory_order_relaxed) * 0.01;
    p.dynamics = parameters.getRawParameterValue("dynamics")->load(std::memory_order_relaxed) * 0.01;
    p.lowsDb = parameters.getRawParameterValue("lows")->load(std::memory_order_relaxed);
    p.highsDb = parameters.getRawParameterValue("highs")->load(std::memory_order_relaxed);
    p.sauce = parameters.getRawParameterValue("sauce")->load(std::memory_order_relaxed) * 0.01;
    p.transients = parameters.getRawParameterValue("transients")->load(std::memory_order_relaxed) * 0.01;
    p.outputDb = parameters.getRawParameterValue("output")->load(std::memory_order_relaxed);
    p.clipper = parameters.getRawParameterValue("clipper")->load(std::memory_order_relaxed)*0.01;
    p.referenceClip = usesReferenceClip();
    p.ceilingDb = skelly::detail::ceilingOffsetToDb(
        parameters.getRawParameterValue("ceiling")->load(std::memory_order_relaxed));
    p.bypass = parameters.getRawParameterValue("bypass")->load(std::memory_order_relaxed) >= 0.5f;
    // Keep the mode flag in the portable engine so the five-band stage runs
    // before its internal lookahead limiter. Bypass still returns the aligned
    // dry path; the wrapper's post-limiter presence and safety stages stay gated.
    p.deuceMatch = parameters.getRawParameterValue("deuceMatch")
        ->load(std::memory_order_relaxed) >= 0.5f;
    engine.setParameters(p);
    engine.beginBlock();
    const bool deuceMatch = p.deuceMatch && !p.bypass;
    // Maintain final true-peak safety when reference CLIP is active even
    // outside Deuce Match, keeping it through the 18ms automation ramp and
    // 85ms release after the CLIP knob returns to zero.
    if(p.referenceClip && p.clipper>0.0 && !p.bypass)
        referenceClipGuardHold=static_cast<int>(std::ceil(getSampleRate()*.12));
    else referenceClipGuardHold=std::max(0,referenceClipGuardHold-samples);
    const bool requireTruePeak=deuceMatch || (referenceClipGuardHold>0 && !p.bypass);

    // Reject malformed input BEFORE JUCE's oversampling filter stages.
    // Sanitizing only the oversampled samples is too late: NaN or Inf can
    // contaminate IIR filter memory and persist long after the bad sample.
    // The generous +48 dBFS guard only affects otherwise pathological audio
    // and keeps extreme finite values from overflowing internal arithmetic.
    constexpr float maxHostSample = 256.0f;
    for (int channel = 0; channel < channels; ++channel) {
        float* inputSamples = buffer.getWritePointer(channel);
        for (int i = 0; i < samples; ++i) {
            const float value = inputSamples[i];
            if (!std::isfinite(value)) inputSamples[i] = 0.0f;
            else if (value > maxHostSample) inputSamples[i] = maxHostSample;
            else if (value < -maxHostSample) inputSamples[i] = -maxHostSample;
        }
    }

    juce::dsp::AudioBlock<float> original(buffer);
    auto highRateBlock = oversampler->processSamplesUp(original);
    float* left = highRateBlock.getChannelPointer(0);
    float* right = channels == 2 ? highRateBlock.getChannelPointer(1) : nullptr;
    const size_t highRateFrames = highRateBlock.getNumSamples();
    for (size_t i = 0; i < highRateFrames; ++i) {
        const auto out = engine.processFrame(left[i], channels == 2 ? right[i] : left[i]);
        left[i] = out.left;
        if (channels == 2) right[i] = out.right;
    }
    oversampler->processSamplesDown(original);

    // The Deuce reference's final +2dB / 5kHz EQ sits after its limiter.
    // Keep it opt-in; legacy factories retain their original sample limiter.
    const double ceiling = skelly::detail::dbToLinear(p.ceilingDb);
    auto* outL = buffer.getWritePointer(0);
    auto* outR = channels == 2 ? buffer.getWritePointer(1) : nullptr;
    const double presenceWet = deuceMatch ? 1.0 : 0.0;
    for (int i = 0; i < samples; ++i) {
        double postL = outL[i];
        double postR = outR ? outR[i] : postL;
        postLimiterPresence.process(postL, postR, presenceWet);
        postLimiterMidPresence.process(postL, postR, presenceWet);
        const skelly::StereoFrame postFrame {
            static_cast<float>(postL), static_cast<float>(postR)
        };
        // Preserve the original sample-peak guard for all legacy factories.
        // Only the opt-in SKELL MASTER / Deuce Match path uses reconstructed
        // true-peak limiting. The fixed 32-sample delay remains in both paths
        // so host PDC never changes when the automatable mode is switched.
        const auto legacyLimited = legacySafetyGuard.process(postFrame, ceiling);
        const auto inputToTruePeak = requireTruePeak ? postFrame
            : (p.bypass ? postFrame : legacyLimited);
        const auto corrected = safetyGuard.process(
            inputToTruePeak, ceiling, p.bypass || !requireTruePeak);
        outL[i] = corrected.left;
        if (outR) outR[i] = corrected.right;
    }
    float maximumL = 0.0f, maximumR = 0.0f, maximumTP = 0.0f;
    const auto* readL = buffer.getReadPointer(0);
    const auto* readR = channels == 2 ? buffer.getReadPointer(1) : readL;
    for (int i = 0; i < samples; ++i) {
        maximumL = juce::jmax(maximumL, std::abs(readL[i]));
        maximumR = juce::jmax(maximumR, std::abs(readR[i]));
        // On true mono buses, avoid reconstructing an identical second
        // channel. Stereo keeps the original sample-for-sample FIR path.
        maximumTP = juce::jmax(maximumTP, channels == 1
            ? truePeakMonitor.processMono(readL[i])
            : truePeakMonitor.process(readL[i], readR[i]));
        loudnessMonitor.process(readL[i], readR[i]);
    }
    atomicMax(peakLeft, maximumL);
    atomicMax(peakRight, maximumR);
    atomicMax(truePeak, maximumTP);
    momentaryLufs.store(static_cast<float>(loudnessMonitor.momentaryLufs()),std::memory_order_relaxed);
    shortTermLufs.store(static_cast<float>(loudnessMonitor.shortTermLufs()),std::memory_order_relaxed);
    // Report the strongest gain reduction actually applied, including the
    // final safety stage after the JUCE downsampler.
    const double activeGuardGain = requireTruePeak ? safetyGuard.currentGain()
        : (p.bypass ? 1.0 : legacySafetyGuard.currentGain());
    const float postReductionDb = static_cast<float>(
        juce::jmax(0.0, -skelly::detail::linearToDb(activeGuardGain)));
    atomicMax(reduction, juce::jmax(static_cast<float>(engine.getBlockGainReductionDb()),
                                   p.bypass ? 0.0f : postReductionDb));
}

juce::AudioProcessorEditor* SkellyMasterAudioProcessor::createEditor() {
    return new SkellyMasterAudioProcessorEditor(*this);
}

void SkellyMasterAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=parameters.copyState();
    snapshot.setProperty("skellMasterStateVersion",6,nullptr);
    snapshot.setProperty("skellMasterFactoryPreset",getFactoryPresetId(),nullptr);
    snapshot.setProperty("skellMasterClipAlgorithm",usesReferenceClip() ? 1 : 0,nullptr);
    if (auto xml=snapshot.createXml()) copyXmlToBinary(*xml,dest);
}

void SkellyMasterAudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml=getXmlFromBinary(data,size)) {
        if (!xml->hasTagName(parameters.state.getType())) return;
        auto snapshot=juce::ValueTree::fromXml(*xml);
        if (!snapshot.isValid()) return;
        // Sessions saved before v6 did not encode the CLIP signal path.
        // Retain their old sound, even when CLIP automation is nonzero.
        const int previousVersion=static_cast<int>(
            snapshot.getProperty("skellMasterStateVersion",1));
        const juce::String clipMode=snapshot.getProperty(
            "skellMasterClipAlgorithm",{}).toString().trim();
        const bool useReference=previousVersion>=6 && clipMode=="1";
        setReferenceClipEnabled(useReference);
        snapshot.setProperty("skellMasterClipAlgorithm",useReference ? 1 : 0,nullptr);
        // ValueTree XML attributes can be surfaced as strings after reload.
        // Parse the saved UI identity and clamp it; absent/malformed legacy
        // values restore the neutral SELECT label (0).
        const auto savedFactoryPreset=snapshot.getProperty("skellMasterFactoryPreset",{});
        const auto factoryText=savedFactoryPreset.toString().trim();
        int parsedFactoryPreset=0;
        bool validFactoryPreset=!factoryText.isEmpty();
        for(const char* cursor=factoryText.toRawUTF8();*cursor!='\0';++cursor) {
            const auto character=*cursor;
            if(character<'0'||character>'9') {
                validFactoryPreset=false;
                break;
            }
            // Only values 0..6 matter: 6 is a saturating sentinel for all
            // positive out-of-range IDs, avoiding integer overflow on hostile XML.
            parsedFactoryPreset=juce::jmin(
                6,parsedFactoryPreset*10+static_cast<int>(character-'0'));
        }
        const int restoredFactoryPreset=validFactoryPreset
            ? juce::jmin(5,parsedFactoryPreset) : 0;
        setFactoryPresetId(restoredFactoryPreset);
        snapshot.setProperty("skellMasterFactoryPreset",restoredFactoryPreset,nullptr);
        // Version-one presets contain the actual -3..-0.1 dBFS ceiling.
        // Migrate just that value to the new centred adjustment, retaining
        // the exact effective threshold without changing any other setting.
        if (static_cast<int>(snapshot.getProperty("skellMasterStateVersion",1))<2) {
            for (int index=0;index<snapshot.getNumChildren();++index) {
                auto child=snapshot.getChild(index);
                if (child.getProperty("id").toString()=="ceiling") {
                    const double previous=static_cast<double>(
                        child.getProperty("value",-1.0));
                    child.setProperty("value",
                        skelly::detail::ceilingDbToOffset(previous),nullptr);
                    break;
                }
            }
        }
        // V1/V2 projects predate the CLIP control. Insert its neutral
        // state explicitly, even if a previous preset left the knob nonzero.
        bool hasClipper=false;
        for(int index=0;index<snapshot.getNumChildren();++index) {
            if(snapshot.getChild(index).getProperty("id").toString()=="clipper") {
                hasClipper=true;
                break;
            }
        }
        if(!hasClipper && snapshot.getNumChildren()>0) {
            juce::ValueTree clipperState(snapshot.getChild(0).getType());
            clipperState.setProperty("id","clipper",nullptr);
            clipperState.setProperty("value",0.0f,nullptr);
            snapshot.appendChild(clipperState,nullptr);
        }
        // V1/V2/V3 sessions predate Deuce Match. Add an explicit neutral
        // value so the new reference-only stages never activate on old projects.
        bool hasDeuceMatch=false;
        for(int index=0;index<snapshot.getNumChildren();++index) {
            if(snapshot.getChild(index).getProperty("id").toString()=="deuceMatch") {
                hasDeuceMatch=true;
                break;
            }
        }
        if(!hasDeuceMatch && snapshot.getNumChildren()>0) {
            juce::ValueTree deuceMatchState(snapshot.getChild(0).getType());
            deuceMatchState.setProperty("id","deuceMatch",nullptr);
            deuceMatchState.setProperty("value",0.0f,nullptr);
            snapshot.appendChild(deuceMatchState,nullptr);
        }
        snapshot.setProperty("skellMasterStateVersion",6,nullptr);
        parameters.replaceState(snapshot);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SkellyMasterAudioProcessor(); }
