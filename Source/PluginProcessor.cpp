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
    add("clipper",    "Limiter / Clipper", 0.0f, 100.0f, 1.0f, 0.0f, "%");
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));
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
    setLatencySamples(static_cast<int>(std::lround(oversampler->getLatencyInSamples()))
                      + lookaheadSamples / oversamplingFactor);
    safetyGuard.prepare(sampleRate);
    peakLeft.store(0); peakRight.store(0); reduction.store(0);
}

void SkellyMasterAudioProcessor::releaseResources() {
    safetyGuard.reset();
    oversampler.reset();
    setLatencySamples(0);
}

void SkellyMasterAudioProcessor::reset() {
    if (oversampler) oversampler->reset();
    engine.reset();
    safetyGuard.reset();
    peakLeft.store(0); peakRight.store(0); reduction.store(0);
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
    p.ceilingDb = skelly::detail::ceilingOffsetToDb(
        parameters.getRawParameterValue("ceiling")->load(std::memory_order_relaxed));
    p.bypass = parameters.getRawParameterValue("bypass")->load(std::memory_order_relaxed) >= 0.5f;
    engine.setParameters(p);
    engine.beginBlock();

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

    // Stereo-linked post-oversampling limiter. Shared and regression-tested
    // with the standalone C++ DSP module: immediate attenuation, smooth
    // recovery, no audio-thread allocations. Ceiling is sample-peak only.
    if (!p.bypass) {
        const double ceiling = skelly::detail::dbToLinear(p.ceilingDb);
        auto* outL = buffer.getWritePointer(0);
        auto* outR = channels == 2 ? buffer.getWritePointer(1) : nullptr;
        for (int i = 0; i < samples; ++i) {
            const auto corrected = safetyGuard.process(
                {outL[i], outR ? outR[i] : outL[i]}, ceiling);
            outL[i] = corrected.left;
            if (outR) outR[i] = corrected.right;
        }
    } else {
        // Bypass must be unmodified, apart from the existing aligned delay.
        safetyGuard.reset();
    }
    float maximumL = 0.0f, maximumR = 0.0f;
    const auto* readL = buffer.getReadPointer(0);
    const auto* readR = channels == 2 ? buffer.getReadPointer(1) : readL;
    for (int i = 0; i < samples; ++i) {
        maximumL = juce::jmax(maximumL, std::abs(readL[i]));
        maximumR = juce::jmax(maximumR, std::abs(readR[i]));
    }
    atomicMax(peakLeft, maximumL);
    atomicMax(peakRight, maximumR);
    // Report the strongest gain reduction actually applied, including the
    // final safety stage after the JUCE downsampler.
    const float postReductionDb = static_cast<float>(
        juce::jmax(0.0, -skelly::detail::linearToDb(safetyGuard.currentGain())));
    atomicMax(reduction, juce::jmax(static_cast<float>(engine.getBlockGainReductionDb()),
                                   p.bypass ? 0.0f : postReductionDb));
}

juce::AudioProcessorEditor* SkellyMasterAudioProcessor::createEditor() {
    return new SkellyMasterAudioProcessorEditor(*this);
}

void SkellyMasterAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto snapshot=parameters.copyState();
    snapshot.setProperty("skellMasterStateVersion",2,nullptr);
    if (auto xml=snapshot.createXml()) copyXmlToBinary(*xml,dest);
}

void SkellyMasterAudioProcessor::setStateInformation(const void* data, int size) {
    if (auto xml=getXmlFromBinary(data,size)) {
        if (!xml->hasTagName(parameters.state.getType())) return;
        auto snapshot=juce::ValueTree::fromXml(*xml);
        if (!snapshot.isValid()) return;
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
        snapshot.setProperty("skellMasterStateVersion",2,nullptr);
        parameters.replaceState(snapshot);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SkellyMasterAudioProcessor(); }
