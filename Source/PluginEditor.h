#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <SkellCore/SkullTheme.h>
#include <array>
#include <memory>

// Live UI is JUCE and SkellCore; static artwork remains isolated from audio.
class SkellyMasterAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                private juce::Timer {
public:
    explicit SkellyMasterAudioProcessorEditor(SkellyMasterAudioProcessor&);
    ~SkellyMasterAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    skellcore::ui::SkullSkin selectedSkin() const noexcept { return skin; }

private:
    class SkellyLookAndFeel final : public juce::LookAndFeel_V4 {
    public:
        SkellyLookAndFeel();
        void setSkin(skellcore::ui::SkullSkin selected);
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
    void switchSkin();
    void recolourStaticBackdrop(juce::Image&) const;
    static skellcore::ui::SkullSkin loadSkin();
    static void saveSkin(skellcore::ui::SkullSkin selected);

    // Invisible hit target: only the left artwork is clickable.
    class SkullHitTarget final : public juce::Button {
    public:
        SkullHitTarget() : juce::Button("SkellCore theme skull") {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            setTooltip("Change SKELL theme (SkellCore)");
        }
        void paintButton(juce::Graphics&, bool, bool) override {}
    };
    SkullHitTarget skullHitTarget;
    skellcore::ui::SkullSkin skin = skellcore::ui::SkullSkin::Ossuary;

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
    // Compare against the values last submitted for painting, not last timer
    // tick: otherwise tiny continuous decreases may never be drawn.
    std::array<float, 2> lastPaintedMeterValues {0.0f, 0.0f};
    std::array<float, 2> lastPaintedPeakHoldValues {0.0f, 0.0f};
    float lastPaintedReduction = 0.0f;
    bool lastDisplayedBypass = false;
    int lastTextBoxWidth = -1, lastTextBoxHeight = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellyMasterAudioProcessorEditor)
};
