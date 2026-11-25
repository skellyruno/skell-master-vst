#pragma once
#include <JuceHeader.h>
#include "DSP/MasteringEngine.h"
#include "DSP/LoudnessMeter.h"
#include "DSP/PostLimiterPresencePrototype.h"
#include <atomic>
#include <memory>

class SkellyMasterAudioProcessor final : public juce::AudioProcessor {
public:
    SkellyMasterAudioProcessor();
    ~SkellyMasterAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int index) override {
        // VST3 requires a non-empty name for every exposed program.
        // The five factory mastering choices remain in the dedicated UI selector.
        return index == 0 ? juce::String("Default") : juce::String();
    }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;

    // Factory selector identity is UI state, not an automatable parameter.
    void setFactoryPresetId(int presetId) noexcept {
        factoryPresetId.store(juce::jlimit(0, 9, presetId), std::memory_order_relaxed);
    }
    int getFactoryPresetId() const noexcept {
        return factoryPresetId.load(std::memory_order_relaxed);
    }
    // Non-automatable mode and topology stored in state v7; no host/APVTS IDs change.
    bool usesReferenceClip() const noexcept {
        return referenceClipEnabled.load(std::memory_order_relaxed);
    }
    void setReferenceClipEnabled(bool enabled) noexcept {
        referenceClipEnabled.store(enabled,std::memory_order_relaxed);
    }
    bool usesChainOrderedClip() const noexcept {
        return chainOrderedClipEnabled.load(std::memory_order_relaxed);
    }
    void setChainOrderedClipEnabled(bool enabled) noexcept {
        chainOrderedClipEnabled.store(enabled,std::memory_order_relaxed);
    }
    bool usesRefinedControlCurves() const noexcept {
        return refinedControlCurvesEnabled.load(std::memory_order_relaxed);
    }
    void setRefinedControlCurvesEnabled(bool enabled) noexcept {
        refinedControlCurvesEnabled.store(enabled,std::memory_order_relaxed);
    }
    bool usesReferenceCalibration() const noexcept {
        return referenceCalibrationEnabled.load(std::memory_order_relaxed);
    }
    void setReferenceCalibrationEnabled(bool enabled) noexcept {
        referenceCalibrationEnabled.store(enabled,std::memory_order_relaxed);
    }

    bool usesDepthWidth() const noexcept { return depthWidthEnabled.load(std::memory_order_relaxed); }
    void setDepthWidthEnabled(bool value) noexcept { depthWidthEnabled.store(value,std::memory_order_relaxed); }
    bool usesAnalogSauce() const noexcept { return analogSauceEnabled.load(std::memory_order_relaxed); }
    void setAnalogSauceEnabled(bool value) noexcept { analogSauceEnabled.store(value,std::memory_order_relaxed); }
    bool usesEnhancedSauce() const noexcept { return enhancedSauceEnabled.load(std::memory_order_relaxed); }
    void setEnhancedSauceEnabled(bool value) noexcept { enhancedSauceEnabled.store(value,std::memory_order_relaxed); }

    // Lock-free peak accumulation. GUI calls exchange(0) once per frame.
    float consumePeakLeft() noexcept { return peakLeft.exchange(0.0f, std::memory_order_relaxed); }
    float consumePeakRight() noexcept { return peakRight.exchange(0.0f, std::memory_order_relaxed); }
    float consumeMaxReduction() noexcept { return reduction.exchange(0.0f, std::memory_order_relaxed); }
    float consumeTruePeak() noexcept { return truePeak.exchange(0.0f, std::memory_order_relaxed); }
    float getMomentaryLufs() const noexcept { return momentaryLufs.load(std::memory_order_relaxed); }
    float getShortTermLufs() const noexcept { return shortTermLufs.load(std::memory_order_relaxed); }
    static constexpr int oversamplingFactor = 4;

private:
    static void atomicMax(std::atomic<float>& v, float input) noexcept;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    skelly::MasteringEngine engine;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> defaultDryDelay;
    juce::AudioBuffer<float> defaultDry;
    juce::SmoothedValue<float> defaultDryMix;
    bool defaultDryMixInitialized = false;
    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f }, reduction { 0.0f };
    std::atomic<float> truePeak { 0.0f };
    skelly::StereoTruePeakMonitor truePeakMonitor;
    // Opt-in five-plugin factory #6 only. Final host-rate EQ after L2.
    skelly::detail::ReferenceFinalEq referenceFinalEq;
    skelly::detail::ReferenceToneCorrection referenceToneCorrection;
    skelly::StereoLoudnessMonitor loudnessMonitor;
    std::atomic<float> momentaryLufs {-100.0f}, shortTermLufs {-100.0f};
    std::atomic<int> factoryPresetId { 0 };
    std::atomic<bool> referenceClipEnabled { true };
    // New instances: StandardCLIP -> limiter. Old v6 host states: post-L2.
    std::atomic<bool> chainOrderedClipEnabled { true };
    // V8 new sessions taper extreme compressor settings; v1-v7 restore
    // the original, exact unmodified GLUE / DYNAMICS response.
    std::atomic<bool> refinedControlCurvesEnabled { true };
    std::atomic<bool> referenceCalibrationEnabled { true };
    std::atomic<bool> enhancedSauceEnabled { true };
    std::atomic<bool> analogSauceEnabled { true };
    std::atomic<bool> depthWidthEnabled { true };
    int referenceClipGuardHold = 0;
    int activeChannels = 2;
    int lookaheadSamples = 0;
    // Five-band Deuce coloration lives before the internal limiter. The
    // 5kHz reference bell and measured 2.5kHz contour correction are both
    // Deuce-Match-only and remain ahead of the final true-peak guard.
    skelly::experimental::PostLimiterPresencePrototype postLimiterPresence;
    skelly::experimental::PostLimiterMidPresencePrototype postLimiterMidPresence;
    // Legacy sample guard plus Deuce-only fixed-size true-peak lookahead.
    skelly::StereoPeakGuard legacySafetyGuard;
    skelly::StereoTruePeakGuard safetyGuard;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellyMasterAudioProcessor)
};
