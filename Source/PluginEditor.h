#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

class SkellyMasterAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::Timer {
public:
    explicit SkellyMasterAudioProcessorEditor(SkellyMasterAudioProcessor&);
    ~SkellyMasterAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class SkellyLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        SkellyLookAndFeel();
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float,
                              juce::Slider&) override;
    } lookAndFeel;

    void timerCallback() override;
    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float, const juce::String&, float) const;
    void drawSkullMark(juce::Graphics&, juce::Rectangle<float>) const;
    void drawScrew(juce::Graphics&, float, float) const;
    void resetToDefaults();
    static juce::String valueToText(double v, const juce::String& unit, bool signedValue);

    SkellyMasterAudioProcessor& processor;
    static constexpr int designW = 1120, designH = 720;
    static constexpr int knobCount = 10;
    std::array<juce::Slider, knobCount> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, knobCount> attachments;
    juce::TextButton bypass { "BYPASS" }, reset { "RESET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::array<float, 2> meterValues {0.0f, 0.0f};
    float displayedReduction = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellyMasterAudioProcessorEditor)
};
