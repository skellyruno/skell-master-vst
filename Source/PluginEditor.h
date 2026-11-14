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
        skellcore::ui::SkullSkin getSkin() const noexcept { return activeSkin; }
    private:
        skellcore::ui::SkullSkin activeSkin = skellcore::ui::SkullSkin::Ossuary;
    public:
        void drawRotarySlider(juce::Graphics&, int, int, int, int, float,
                              float, float, juce::Slider&) override;
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                                  bool, bool) override;
        void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
        void drawLabel(juce::Graphics&, juce::Label&) override;
        void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
        juce::Font getComboBoxFont(juce::ComboBox&) override;
        juce::Font getLabelFont(juce::Label&) override;
    } lookAndFeel;

    void timerCallback() override;
    void paintStatic(juce::Graphics&);
    void drawMeter(juce::Graphics&, juce::Rectangle<float>, float, float,
                   const juce::String&) const;
    void drawReduction(juce::Graphics&) const;
    void drawScrew(juce::Graphics&, float, float) const;
    void resetToDefaults();
    void applyFactoryPreset(int presetId);
    void clearMeterHolds();
    void switchSkin();
    static skellcore::ui::SkullSkin loadSkin();
    static void saveSkin(skellcore::ui::SkullSkin selected);

    // Invisible hit target: only the left artwork is clickable.
    class SkullHitTarget final : public juce::Button {
    public:
        SkullHitTarget() : juce::Button("SkellCore theme skull") {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            setTooltip("Change SKELL theme (SkellCore)");
        }
        void paintButton(juce::Graphics& g, bool over, bool down) override {
            if (!over && !down) return;
            const auto accent=getLookAndFeel().findColour(juce::Slider::thumbColourId);
            const auto b=getLocalBounds().toFloat().reduced(1.5f);
            g.setColour(juce::Colours::black.withAlpha(down ? 0.28f : 0.14f));
            g.fillRoundedRectangle(b,8.0f);
            g.setColour(accent.withAlpha(down ? 0.92f : 0.56f));
            g.drawRoundedRectangle(b,8.0f,down ? 1.8f : 1.2f);
        }
    };
    SkullHitTarget skullHitTarget;
    skellcore::ui::SkullSkin skin = skellcore::ui::SkullSkin::Ossuary;

    SkellyMasterAudioProcessor& processor;
    static constexpr int designW = 1200, designH = 760;
    static constexpr int knobCount = 11;
    std::array<juce::Slider, knobCount> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, knobCount> attachments;
    juce::TextButton bypass { "BYPASS" }, reset { "RESET" };
    juce::ComboBox presetSelector;
    std::array<juce::TextButton,3> monitorButtons {juce::TextButton("PEAK"), juce::TextButton("TRUE"), juce::TextButton("CLEAR")};
    bool monitorTruePeak = true;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    juce::Image staticBackdrop;
    std::array<float, 2> meterValues {0.0f, 0.0f};
    std::array<float, 2> peakHoldValues {0.0f, 0.0f};
    std::array<int, 2> peakHoldTicks {0, 0};
    float displayedReduction = 0.0f;
    float displayedTruePeak = 0.0f;
    // Compare against the values last submitted for painting, not last timer
    // tick: otherwise tiny continuous decreases may never be drawn.
    std::array<float, 2> lastPaintedMeterValues {0.0f, 0.0f};
    std::array<float, 2> lastPaintedPeakHoldValues {0.0f, 0.0f};
    float lastPaintedReduction = 0.0f;
    float lastPaintedTruePeak = 0.0f;
    bool lastDisplayedBypass = false;
    int lastTextBoxWidth = -1, lastTextBoxHeight = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellyMasterAudioProcessorEditor)
};
