#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
#include <memory>

// All visual elements are original JUCE vector artwork.
// The established APVTS IDs and DSP path remain unchanged.
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
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                                  bool, bool) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
        void drawLabel(juce::Graphics&, juce::Label&) override;
        juce::Font getLabelFont(juce::Label&) override;
    } lookAndFeel;

    void timerCallback() override;
    void paintStatic(juce::Graphics&);
    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float, float,
                   const juce::String&) const;
    void drawReduction(juce::Graphics&) const;
    void drawScrew(juce::Graphics&, float, float) const;
    void resetToDefaults();

    SkellyMasterAudioProcessor& processor;
    static constexpr int designW = 1200, designH = 760;
    static constexpr int knobCount = 10;
    std::array<juce::Slider, knobCount> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, knobCount> attachments;
    juce::TextButton bypass { "BYPASS" }, reset { "RESET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::Image staticBackdrop;
    std::array<float, 2> meterValues {0.0f, 0.0f};
    std::array<float, 2> peakHoldValues {0.0f, 0.0f};
    std::array<int, 2> peakHoldTicks {0, 0};
    float displayedReduction = 0.0f;
    int lastTextBoxWidth = -1, lastTextBoxHeight = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellyMasterAudioProcessorEditor)
};
