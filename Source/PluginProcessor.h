#pragma once
#include <JuceHeader.h>
#include "DSP/MasteringEngine.h"
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
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters;

    // Lock-free peak accumulation. GUI calls exchange(0) once per frame.
    float consumePeakLeft() noexcept { return peakLeft.exchange(0.0f, std::memory_order_relaxed); }
    float consumePeakRight() noexcept { return peakRight.exchange(0.0f, std::memory_order_relaxed); }
    float consumeMaxReduction() noexcept { return reduction.exchange(0.0f, std::memory_order_relaxed); }
    static constexpr int oversamplingFactor = 4;

private:
    static void atomicMax(std::atomic<float>& v, float input) noexcept;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    skelly::MasteringEngine engine;
    std::atomic<float> peakLeft { 0.0f }, peakRight { 0.0f }, reduction { 0.0f };
    int activeChannels = 2;
    int lookaheadSamples = 0;
    // Final host-rate stereo-linked peak protection, preserved across blocks.
    // Prepared outside the real-time callback; independent of the lookahead DSP.
    float safetyGain = 1.0f;
    float safetyReleaseCoefficient = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellyMasterAudioProcessor)
};
