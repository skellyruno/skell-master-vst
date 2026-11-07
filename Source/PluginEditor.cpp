#include "PluginEditor.h"
#include "UI/StarSkullMark.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float pi = juce::MathConstants<float>::pi;
const juce::Colour neon(0xff61f77a);
const juce::Colour mint(0xff40cb73);
const juce::Colour edge(0xff3c5145);
const juce::Colour ink(0xff090e0d);
const juce::Colour panel(0xff151b19);
const juce::Colour pale(0xffeaf0ec);
const juce::Colour muted(0xff83948b);

constexpr const char* labels[10] = {
    "INPUT", "DRIVE", "GLUE", "DYNAMICS", "LOWS", "HIGHS",
    "SAUCE", "TRANSIENTS", "OUTPUT", "CEILING"
};
constexpr const char* ids[10] = {
    "input", "drive", "glue", "dynamics", "lows", "highs",
    "sauce", "transients", "output", "ceiling"
};
constexpr const char* subtitles[8] = {
    "GAIN STAGE", "HARMONIC DRIVE", "BUS COMPRESSION", "LEVEL CONTROL",
    "115 Hz SHELF", "8.2 kHz SHELF", "ANALOG ENHANCE", "ATTACK SHAPE"
};

void font(juce::Graphics& g, float size, bool bold = false) {
    g.setFont(juce::Font(juce::FontOptions(size).withStyle(bold ? "Bold" : "Regular")));
}
void text(juce::Graphics& g, const juce::String& s, float x, float y,
          float w, float h, juce::Justification j = juce::Justification::centred) {
    g.drawText(s, juce::Rectangle<float>(x, y, w, h), j, false);
}
void plate(juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f) {
    // Hardware-inspired anodized aluminum faceplate with engraved edge.
    // Decorative panels are rendered into the static GUI cache.
    const auto top = juce::Colour(0xff303936);
    const auto bottom = juce::Colour(0xff121917);
    juce::ColourGradient metal(top, r.getX(), r.getY(),
                              bottom, r.getX(), r.getBottom(), false);
    metal.addColour(0.18, juce::Colour(0xff242e29));
    metal.addColour(0.52, juce::Colour(0xff1a2320));
    metal.addColour(0.85, juce::Colour(0xff111815));
    g.setGradientFill(metal);
    g.fillRoundedRectangle(r, radius);
    // The directional brushing is restrained so type remains legible.
    for (float y = r.getY() + 6.0f; y < r.getBottom() - 5.0f; y += 6.0f) {
        g.setColour(juce::Colours::white.withAlpha(0.014f));
        g.drawHorizontalLine(static_cast<int>(y), r.getX() + 7.0f, r.getRight() - 7.0f);
    }
    g.setColour(juce::Colour(0xff9aa89d).withAlpha(0.48f));
    g.drawRoundedRectangle(r.reduced(0.8f), radius, 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawLine(r.getX() + 12.0f, r.getY() + 2.0f,
               r.getRight() - 12.0f, r.getY() + 2.0f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.70f));
    g.drawRoundedRectangle(r.reduced(3.0f), juce::jmax(0.0f, radius - 2.0f), 1.2f);
    g.setColour(juce::Colours::white.withAlpha(0.04f));
    g.drawLine(r.getX() + 10.0f, r.getBottom() - 2.0f,
               r.getRight() - 10.0f, r.getBottom() - 2.0f, 0.8f);
}
float peakDb(float value) {
    return juce::jmax(-80.0f, juce::Decibels::gainToDecibels(juce::jmax(value, 0.000001f), -80.0f));
}
float normalizedPeak(float value) {
    return juce::jlimit(0.0f, 1.0f, (peakDb(value) + 48.0f) / 48.0f);
}
} // namespace

SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::SkellyLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, pale);
    setColour(juce::Label::textColourId, pale);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3b8b52));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0d1410));
    setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff296039));
    setColour(juce::Slider::thumbColourId, neon);
    setColour(juce::TextButton::textColourOffId, pale);
    setColour(juce::TextButton::textColourOnId, pale);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff153322));
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawLabel(
    juce::Graphics& g, juce::Label& label) {
    const auto bounds = label.getLocalBounds().toFloat().reduced(0.75f);
    g.setColour(juce::Colour(0xff040807));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour(0xff1b2922));
    g.fillRoundedRectangle(bounds.reduced(2.0f), 4.0f);
    g.setColour(juce::Colour(0xff6b8070).withAlpha(0.70f));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
    g.setColour(juce::Colour(0xff5eed79).withAlpha(0.26f));
    g.drawLine(bounds.getX() + 5.0f, bounds.getBottom() - 2.0f,
               bounds.getRight() - 5.0f, bounds.getBottom() - 2.0f, 0.8f);
    if (!label.isBeingEdited()) {
        g.setColour(pale);
        g.setFont(getLabelFont(label));
        g.drawFittedText(label.getText(), label.getLocalBounds().reduced(4, 1),
                         label.getJustificationType(), 1);
    }
}

juce::Font SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::getLabelFont(juce::Label& label) {
    return juce::Font(juce::FontOptions(juce::jlimit(10.0f, 15.0f,
                    static_cast<float>(label.getHeight()) * 0.85f)).withStyle("Bold"));
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) {
    const auto r = b.getLocalBounds().toFloat().reduced(1.5f);
    const bool active = b.getToggleState();
    const bool isReset = b.getButtonText() == "RESET";
    const juce::Colour stroke = active ? juce::Colour(0xffffb36a)
                               : isReset ? juce::Colour(0xff708079)
                               : neon;
    g.setColour(juce::Colour(0xff020605));
    g.fillRoundedRectangle(r.expanded(1.0f), 8.0f);
    juce::ColourGradient shade(down ? juce::Colour(0xff141e17) :
                              juce::Colour(0xff435047),
                              r.getX(), r.getY(),
                              down ? juce::Colour(0xff2e4735) :
                              juce::Colour(0xff101815),
                              r.getX(), r.getBottom(), false);
    shade.addColour(0.42f, juce::Colour(0xff202c26));
    g.setGradientFill(shade);
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(stroke.withAlpha(active || hover ? 0.98f : 0.67f));
    g.drawRoundedRectangle(r, 7.0f, active || hover ? 1.6f : 1.0f);
    g.setColour(juce::Colours::white.withAlpha(down ? 0.035f : 0.21f));
    g.drawLine(r.getX()+8.0f, r.getY()+2.5f,
               r.getRight()-8.0f, r.getY()+2.5f, 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawLine(r.getX()+8.0f, r.getBottom()-2.0f,
               r.getRight()-8.0f, r.getBottom()-2.0f, 1.0f);
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawButtonText(
    juce::Graphics& g, juce::TextButton& b, bool, bool) {
    font(g, juce::jlimit(9.0f, 14.0f, static_cast<float>(b.getHeight()) * 0.35f), true);
    g.setColour(b.getToggleState() ? juce::Colour(0xffffd3a5) : pale);
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(5),
                     juce::Justification::centred, 1);
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float position,
    float start, float end, juce::Slider& slider) {

    const float cx = static_cast<float>(x) + width * 0.5f;
    const float cy = static_cast<float>(y) + height * 0.50f;
    const bool utility = slider.getComponentID() == "utility";
    const float radius = std::min(static_cast<float>(width), static_cast<float>(height))
                         * (utility ? 0.455f : 0.47f);
    const float startAngle = start + 0.065f, endAngle = end - 0.065f;
    const float angle = startAngle + position * (endAngle - startAngle);
    const bool hasZero = slider.getMinimum() < 0 && slider.getMaximum() > 0;
    const float neutral = hasZero
        ? static_cast<float>(slider.valueToProportionOfLength(0.0))
        : 0.0f;
    const float startOfActive = startAngle + neutral * (endAngle - startAngle);

    // Precision tick scale with subdued inactive markings.
    for (int i = 0; i <= 40; ++i) {
        const float a = startAngle + (endAngle - startAngle) * i / 40.0f;
        const float si = std::sin(a), co = -std::cos(a);
        const bool major = i % 5 == 0;
        const float outer = radius * 1.08f;
        const float inner = radius * (major ? 0.94f : 0.99f);
        g.setColour(juce::Colour(0xff78877c).withAlpha(major ? 0.78f : 0.36f));
        g.drawLine(cx + si * inner, cy + co * inner,
                   cx + si * outer, cy + co * outer, major ? 1.3f : 0.75f);
    }

    // Recessed circular chassis, thin glowing progress indication.
    g.setColour(juce::Colour(0xff050808));
    g.fillEllipse(cx - radius * .93f, cy - radius * .93f,
                  radius * 1.86f, radius * 1.86f);
    g.setColour(edge);
    g.drawEllipse(cx - radius * .91f, cy - radius * .91f,
                  radius * 1.82f, radius * 1.82f, 1.0f);
    // Raised circular bezel reflects the faceplate lighting.
    g.setColour(juce::Colour(0xffa1b0a4).withAlpha(0.34f));
    juce::Path bezelHighlight;
    bezelHighlight.addCentredArc(cx, cy, radius*.935f, radius*.935f,
                                 0.0f, -0.6f, 1.7f, true);
    g.strokePath(bezelHighlight, juce::PathStrokeType(1.5f));
    g.setColour(juce::Colours::black.withAlpha(.55f));
    g.drawEllipse(cx-radius*.84f, cy-radius*.84f,
                  radius*1.68f, radius*1.68f, 1.25f);
    juce::Path groove;
    groove.addCentredArc(cx, cy, radius * .86f, radius * .86f, 0,
                         startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff22332a));
    g.strokePath(groove, juce::PathStrokeType(4.5f));
    if (std::abs(angle - startOfActive) > 0.003f) {
        juce::Path active;
        active.addCentredArc(cx, cy, radius * .86f, radius * .86f, 0,
                             std::min(angle, startOfActive),
                             std::max(angle, startOfActive), true);
        g.setColour(neon.withAlpha(0.15f));
        g.strokePath(active, juce::PathStrokeType(8.0f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(neon);
        g.strokePath(active, juce::PathStrokeType(2.5f,
                     juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Machined aluminum: three separate beveled rings, brushed radial face.
    const float face = radius * 0.74f;
    g.setColour(juce::Colour(0xff050b08));
    g.fillEllipse(cx - face - 3, cy - face - 3, 2 * (face + 3), 2 * (face + 3));
    juce::ColourGradient bevel(juce::Colour(0xffa2b2a8),
                                cx - radius * 0.65f, cy - radius * 0.75f,
                                juce::Colour(0xff131d19),
                                cx + radius * 0.7f, cy + radius * 0.8f, false);
    bevel.addColour(0.4, juce::Colour(0xff46564d));
    bevel.addColour(0.65, juce::Colour(0xff1a2822));
    g.setGradientFill(bevel);
    g.fillEllipse(cx - face, cy - face, face * 2, face * 2);
    g.setColour(juce::Colour(0xff73877a).withAlpha(0.66f));
    g.drawEllipse(cx - face + 1, cy - face + 1, face * 2 - 2, face * 2 - 2, 1.3f);
    // A restrained knurled grip reads as physical hardware at compact sizes.
    // Each segment is a narrow engraved groove in the metallic outer rim.
    for (int k = 0; k < 48; ++k) {
        const float a = 2.0f*pi*static_cast<float>(k)/48.0f;
        const float cs = std::cos(a), sn = std::sin(a);
        const float inner = face * 0.885f, outer = face * 0.975f;
        g.setColour(k % 4 == 0 ? juce::Colours::black.withAlpha(0.42f) :
                                juce::Colours::black.withAlpha(0.23f));
        g.drawLine(cx+cs*inner,cy+sn*inner,
                   cx+cs*outer,cy+sn*outer,0.8f);
    }
    g.setColour(juce::Colour(0xffc3cec2).withAlpha(0.17f));
    juce::Path gripHighlight;
    gripHighlight.addCentredArc(cx, cy, face-2.0f, face-2.0f,
                                0.0f, -0.3f, 1.3f, true);
    g.strokePath(gripHighlight, juce::PathStrokeType(1.0f));

    const float inset = face * 0.86f;
    juce::ColourGradient brushed(juce::Colour(0xff4d5a53),
                                 cx - inset * .72f, cy - inset * .92f,
                                 juce::Colour(0xff0a110f),
                                 cx + inset * .75f, cy + inset * .82f, false);
    brushed.addColour(.35, juce::Colour(0xff53645a));
    brushed.addColour(.63, juce::Colour(0xff1f2f28));
    g.setGradientFill(brushed);
    g.fillEllipse(cx - inset, cy - inset, inset * 2, inset * 2);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    juce::Path faceHighlight;
    faceHighlight.addCentredArc(cx, cy, inset-1.0f, inset-1.0f,
                                0.0f, -0.9f, 0.55f, true);
    g.strokePath(faceHighlight, juce::PathStrokeType(1.0f));
    for (int i = 0; i < 5; ++i) {
        const float r = inset * (0.41f + i * .115f);
        g.setColour(juce::Colours::white.withAlpha(0.013f + i * .003f));
        g.drawEllipse(cx-r, cy-r, r*2, r*2, 0.6f);
    }

    // Short, readable LED position pointer.
    const float si = std::sin(angle), co = -std::cos(angle);
    g.setColour(neon.withAlpha(.20f));
    g.drawLine(cx + si*inset*.33f, cy + co*inset*.33f,
               cx + si*inset*.83f, cy + co*inset*.83f, 6);
    g.setColour(neon);
    g.drawLine(cx + si*inset*.32f, cy + co*inset*.32f,
               cx + si*inset*.85f, cy + co*inset*.85f, 2.5f);
    g.setColour(juce::Colour(0xff09100d));
    g.fillEllipse(cx - 4.2f, cy - 4.2f, 8.4f, 8.4f);
    g.setColour(neon.withAlpha(.82f));
    g.fillEllipse(cx - 1.7f, cy - 1.7f, 3.4f, 3.4f);
}

SkellyMasterAudioProcessorEditor::SkellyMasterAudioProcessorEditor(SkellyMasterAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p) {
    setLookAndFeel(&lookAndFeel);

    for (int i = 0; i < knobCount; ++i) {
        auto& s = knobs[static_cast<size_t>(i)];
        s.setComponentID(i >= 8 ? "utility" : "primary");
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, i >= 8 ? 85 : 87, 27);
        s.setRotaryParameters(pi * 1.25f, pi * 2.75f, true);
        s.setMouseDragSensitivity(190);
        s.setVelocityBasedMode(false);
        s.setDoubleClickReturnValue(true, i == 9 ? -1.0 : 0.0);
        const bool db = i == 0 || i == 4 || i == 5 || i == 8 || i == 9;
        s.setTextValueSuffix(db ? " dB" : " %");
        const juce::String sub = i < 8 ? subtitles[i] : (i == 8 ? "OUTPUT TRIM" : "PEAK CEILING");
        s.setTooltip(juce::String(labels[i]) + " - " + sub +
                     " | drag to adjust, double-click to reset, right-click for menu");
        addAndMakeVisible(s);
        attachments[static_cast<size_t>(i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.parameters, ids[i], s);
    }
    bypass.setClickingTogglesState(true);
    bypass.setTooltip("Toggle the latency-aligned mastering bypass");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "bypass", bypass);
    addAndMakeVisible(bypass);
    reset.setTooltip("Restore the default values of all mastering controls");
    reset.onClick = [this] { resetToDefaults(); };
    addAndMakeVisible(reset);

    setResizable(true, true);
    setResizeLimits(620, 393, 1800, 1140);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(designW) / designH);
    setSize(702, 445);
    startTimerHz(30);
}

SkellyMasterAudioProcessorEditor::~SkellyMasterAudioProcessorEditor() {
    stopTimer();
    for (auto& attachment : attachments) attachment.reset();
    bypassAttachment.reset();
    setLookAndFeel(nullptr);
}

void SkellyMasterAudioProcessorEditor::resetToDefaults() {
    for (const char* id : ids)
        if (auto* param = processor.parameters.getParameter(id)) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->getDefaultValue());
            param->endChangeGesture();
        }
    if (auto* param = processor.parameters.getParameter("bypass")) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->getDefaultValue());
        param->endChangeGesture();
    }
}

void SkellyMasterAudioProcessorEditor::timerCallback() {
    const std::array<float, 2> peaks {processor.consumePeakLeft(), processor.consumePeakRight()};
    for (size_t channel = 0; channel < meterValues.size(); ++channel) {
        meterValues[channel] = juce::jmax(peaks[channel], meterValues[channel] * 0.89f);
        if (peaks[channel] > peakHoldValues[channel]) {
            peakHoldValues[channel] = peaks[channel];
            peakHoldTicks[channel] = 45; // 1.5 second hold at 30 Hz
        } else if (peakHoldTicks[channel] > 0) {
            --peakHoldTicks[channel];
        } else {
            peakHoldValues[channel] *= 0.975f; // gentle peak cursor fall
        }
    }
    displayedReduction = juce::jmax(processor.consumeMaxReduction(), displayedReduction * 0.86f);
    // At 30 Hz only the meters and status LED change. Repaint clipped
    // rectangles instead of the whole window and all 10 controls.
    const float sx = static_cast<float>(getWidth()) / designW;
    const float sy = static_cast<float>(getHeight()) / designH;
    const auto dirty = [this, sx, sy](int x, int y, int w, int h) {
        repaint(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                juce::roundToInt(w * sx) + 2, juce::roundToInt(h * sy) + 2);
    };
    dirty(270, 105, 901, 177);
    dirty(174, 243, 85, 24);
}

void SkellyMasterAudioProcessorEditor::drawScrew(juce::Graphics& g, float x, float y) const {
    g.setColour(juce::Colours::black.withAlpha(.72f));
    g.fillEllipse(x-6.0f, y-5.5f, 12.0f, 12.0f);
    juce::ColourGradient steel(juce::Colour(0xffb2beb3),x-4,y-4,
                               juce::Colour(0xff354039),x+5,y+5,false);
    steel.addColour(.46,juce::Colour(0xff78857b));
    g.setGradientFill(steel);
    g.fillEllipse(x-4.7f,y-4.7f,9.4f,9.4f);
    g.setColour(juce::Colour(0xff101915));
    g.drawLine(x-2.0f,y+2.0f,x+2.0f,y-2.0f,1.4f);
    g.setColour(juce::Colours::white.withAlpha(.25f));
    g.drawLine(x-2.4f,y+1.3f,x+1.3f,y-2.4f,.6f);
}

void SkellyMasterAudioProcessorEditor::drawMeter(
    juce::Graphics& g, juce::Rectangle<float> r, float value,
    float heldValue, const juce::String& name) const {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(r.getWidth()/310.0f, r.getHeight()/156.0f)
                       .translated(r.getX(), r.getY()));

    plate(g, {0,0,310,156}, 9);
    g.setColour(juce::Colour(0xff81978a).withAlpha(.55f));
    g.drawRoundedRectangle(juce::Rectangle<float>(3,3,304,150),8.0f,1.0f);
    g.setColour(juce::Colour(0xff030706));
    g.fillRoundedRectangle(6,6,298,144,7);
    g.setColour(juce::Colour(0xff1d3026));
    g.drawRoundedRectangle(juce::Rectangle<float>(8,8,294,140),6.0f,1.2f);
    juce::ColourGradient glass(juce::Colour(0xff1b2d24),155,5,
                              juce::Colour(0xff070f0b),155,151,false);
    glass.addColour(0.55,juce::Colour(0xff102118));
    g.setGradientFill(glass);
    g.fillRoundedRectangle(10,9,290,137,6);
    g.setColour(juce::Colour(0xff385343));
    g.drawRoundedRectangle(10,9,290,137,6,1);

    font(g,11,true); g.setColour(pale);
    text(g,name,20,12,270,15);
    const float pivotX=155.0f, pivotY=146.0f, radius=103.0f;
    for (int i=0;i<=24;++i) {
        const float t=static_cast<float>(i)/24.0f;
        const float a=(-153.0f+126.0f*t)*pi/180.0f;
        const float si=std::sin(a), co=std::cos(a);
        const float outer=radius;
        const float inner=radius-(i%4==0 ? 12.5f : 6.5f);
        g.setColour(i>20 ? neon : juce::Colour(0xff83b693));
        g.drawLine(pivotX+co*inner,pivotY+si*inner,
                   pivotX+co*outer,pivotY+si*outer,i%4==0?1.5f:0.8f);
    }
    font(g,12,true); g.setColour(pale.withAlpha(.85f));
    text(g,"-48",35,100,34,15);
    text(g,"-36",56,79,32,15);
    text(g,"-24",106,51,40,15);
    text(g,"-12",170,51,40,15);
    text(g,"-6",231,79,24,15);
    text(g,"0",262,100,22,15);

    // Clip the needle to the inset window and mask its pivot beneath the readout.
    // Previously the needle visibly escaped below both frames in FL Studio.
    const float angle=(-153.0f+126.0f*normalizedPeak(value))*pi/180.0f;
    const float tipX=pivotX+std::cos(angle)*radius*.92f;
    const float tipY=pivotY+std::sin(angle)*radius*.92f;
    {
        juce::Graphics::ScopedSaveState needleClip(g);
        g.reduceClipRegion(juce::Rectangle<int>(13,35,284,109));
        g.setColour(neon.withAlpha(.17f));
        g.drawLine(pivotX,pivotY,tipX,tipY,5.0f);
        g.setColour(neon);
        g.drawLine(pivotX,pivotY,tipX,tipY,1.7f);
    }
    g.setColour(juce::Colour(0xff0b1810));
    g.fillRoundedRectangle(101.0f,127.0f,108.0f,19.0f,4.0f);
    font(g,12,true); g.setColour(neon);
    text(g,juce::String(peakDb(value),1)+" dBFS",101,127,108,19);

    // Glass specular line, constrained to the top edge of the meter window.
    g.setColour(juce::Colours::white.withAlpha(.075f));
    g.fillRoundedRectangle(16,10,278,10,4);
    // Angled reflection gives the smoked meter glass a physical lens.
    juce::Path lens;
    lens.startNewSubPath(18.0f,24.0f);
    lens.lineTo(105.0f,24.0f);
    lens.lineTo(43.0f,113.0f);
    lens.lineTo(16.0f,113.0f);
    lens.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(.022f));
    g.fillPath(lens);
    // Hold marker lives on the calibrated needle scale (no extra text clutter).
    if (heldValue > 0.0001f) {
        const float holdAngle = (-153.0f + 126.0f * normalizedPeak(heldValue))
                                * pi / 180.0f;
        const float hx = pivotX + std::cos(holdAngle) * radius;
        const float hy = pivotY + std::sin(holdAngle) * radius;
        g.setColour(juce::Colour(0xffffcf78));
        g.fillEllipse(hx - 2.6f, hy - 2.6f, 5.2f, 5.2f);
    }
}

void SkellyMasterAudioProcessorEditor::drawReduction(juce::Graphics& g) const {
    plate(g, {953,108,216,170}, 8);
    g.setColour(juce::Colour(0xff050b08));
    g.fillRoundedRectangle(958,113,206,159,7);
    g.setColour(juce::Colour(0xff54715b).withAlpha(.55f));
    g.drawRoundedRectangle(juce::Rectangle<float>(958,113,206,159),7.0f,1.0f);
    g.setColour(juce::Colours::white.withAlpha(.04f));
    g.drawLine(969,116,1152,116,1.0f);
    font(g,11,true); g.setColour(pale);
    text(g,"GAIN REDUCTION",967,127,188,19);
    font(g,37,true); g.setColour(neon);
    text(g,juce::String(displayedReduction,1),974,154,140,51);
    font(g,13); g.setColour(muted);
    text(g,"dB",1114,178,34,16);
    for (int i=0;i<16;++i) {
        const float lit = juce::jlimit(0.0f,16.0f,displayedReduction * (16.0f/18.0f));
        g.setColour(i<lit ? (i>12 ? juce::Colour(0xffefbf75) : neon)
                          : juce::Colour(0xff28372e));
        g.fillRoundedRectangle(972+i*11.0f,232,7.5f,16,2);
    }
}

void SkellyMasterAudioProcessorEditor::paintStatic(juce::Graphics& g) {
    juce::ColourGradient backdrop(juce::Colour(0xff202925),0,0,
                                   juce::Colour(0xff080d0b),0,designH,false);
    backdrop.addColour(.20,juce::Colour(0xff151c18));
    backdrop.addColour(.68,juce::Colour(0xff0c1210));
    g.setGradientFill(backdrop);
    g.fillRect(0,0,designW,designH);

    // Fine brushed texture (in a cached image; never re-created per meter frame).
    for (int y=0;y<designH;y+=4) {
        g.setColour(juce::Colours::white.withAlpha((y%12==0)? .012f:.005f));
        g.drawHorizontalLine(y,0,static_cast<float>(designW));
    }

    // Header: compact branding, large title, restrained accents.
    g.setColour(juce::Colour(0xff0f1512));
    g.fillRect(0,0,designW,88);
    g.setColour(juce::Colour(0xff4d6152)); g.drawHorizontalLine(87,0,designW);
    g.setColour(mint); g.fillRect(0,86,designW,1);
    skelly::ui::drawStarSkull(g,{31,17,56,49},.91f);
    font(g,22,true); g.setColour(pale);
    text(g,"SKELLY",100,16,173,30,juce::Justification::centredLeft);
    font(g,11,true); g.setColour(neon);
    text(g,"AUDIO LABS",102,48,160,20,juce::Justification::centredLeft);
    g.setColour(edge); g.drawVerticalLine(317,18,70);
    font(g,36,true); g.setColour(pale);
    text(g,"SKELLY",390,12,202,44,juce::Justification::centredRight);
    g.setColour(neon);
    text(g,"MASTER",602,12,213,44,juce::Justification::centredLeft);
    font(g,12,true); g.setColour(muted);
    text(g,"STEREO MASTERING SUITE  /  M-01",447,56,355,17);
    g.setColour(mint); g.fillRoundedRectangle(351,39,29,2,1);
    g.fillRoundedRectangle(826,39,29,2,1);
    font(g,12,true); g.setColour(muted);
    text(g,"PRECISION / POWER",864,31,204,18,juce::Justification::centredRight);
    // New star-eyed graphic at top-right, no text or raster branding.
    skelly::ui::drawStarSkull(g,{1108,12,74,60},0.92f);

    // Meter bridge and typography arranged on a single beveled panel.
    plate(g,{22,95,1156,193},11);
    for (float x : {38.0f,1162.0f})
        for (float y : {111.0f,273.0f}) drawScrew(g,x,y);
    g.setColour(juce::Colour(0xff445149));
    g.drawVerticalLine(261,113,271);

    font(g,17,true); g.setColour(pale);
    text(g,"SKELLY MASTER",48,125,201,27);
    font(g,12,true); g.setColour(muted);
    text(g,"PRECISION MASTER BUS  /  M-01",43,154,211,21);
    g.setColour(edge); g.drawHorizontalLine(181,70,237);
    font(g,13,true); g.setColour(muted);
    text(g,"4X OVERSAMPLING",52,191,194,16);
    text(g,"STEREO-LINKED DSP",52,209,194,16);
    text(g,"LOOKAHEAD LIMITING",52,227,194,16);
    g.setColour(neon.withAlpha(.35f)); g.fillEllipse(72,251,11,11);
    g.setColour(neon); g.fillEllipse(75,254,5,5);
    font(g,12,true); g.setColour(neon);
    text(g,"ENGINE STATUS",91,246,147,19,juce::Justification::centredLeft);

    // Grouped knobs: two compact rows, never overlapping labels or meters.
    plate(g,{22,302,1156,360},12);
    for (const float x : {39.0f, 1161.0f})
        for (const float y : {319.0f, 645.0f}) drawScrew(g,x,y);
    g.setColour(juce::Colour(0xff0e1412));
    g.fillRoundedRectangle(34,315,1132,166,8);
    g.fillRoundedRectangle(34,485,1132,165,8);
    g.setColour(edge.withAlpha(.45f));
    g.drawHorizontalLine(483,50,1150);
    for (int c=1;c<4;++c) {
        const float x=34.0f + 283.0f*c;
        g.setColour(edge.withAlpha(.48f));
        g.drawVerticalLine(static_cast<int>(x),331,462);
        g.drawVerticalLine(static_cast<int>(x),500,632);
    }
    for (int i=0;i<8;++i) {
        const int row=i/4, col=i%4;
        const float cellX=34.0f+col*283.0f;
        const float rowY=row==0?315.0f:485.0f;
        font(g,16,true); g.setColour(pale);
        text(g,labels[i],cellX+16,rowY+125,175,22);
        font(g,12,true); g.setColour(mint);
        text(g,subtitles[i],cellX+9,rowY+148,192,15);
    }

    // Bottom control deck: separate mastering utility controls.
    plate(g,{0,673,1200,87},0);
    drawScrew(g,18.0f,714.0f);
    drawScrew(g,1181.0f,714.0f);
    g.setColour(juce::Colour(0xff66766b).withAlpha(.45f));
    g.drawHorizontalLine(673,0,1200);
    g.setColour(mint); g.fillRoundedRectangle(0,673,1200,1.2f,0.6f);
    font(g,13,true); g.setColour(pale);
    text(g,"//  MASTER BUS",43,697,208,21,juce::Justification::centredLeft);
    font(g,10); g.setColour(muted);
    text(g,"TONE IS EVERYTHING",66,723,177,15,juce::Justification::centredLeft);
    g.setColour(edge); g.drawVerticalLine(265,694,745);
    g.drawVerticalLine(552,694,745);
    font(g,12,true); g.setColour(pale);
    text(g,"OUTPUT",572,708,76,18,juce::Justification::centredLeft);
    g.setColour(edge); g.drawVerticalLine(851,694,745);
    g.setColour(pale);
    text(g,"CEILING",861,708,77,18,juce::Justification::centredLeft);
    g.setColour(edge); g.drawVerticalLine(1120,694,745);
    font(g,13,true); g.setColour(muted);
    text(g,"M-01",1133,706,53,20);
}

void SkellyMasterAudioProcessorEditor::paint(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW,
                                                static_cast<float>(getHeight()) / designH));

    if (staticBackdrop.isNull()) {
        // Supersampled static vector artwork, generated on the GUI thread only.
        staticBackdrop = juce::Image(juce::Image::ARGB, designW * 2, designH * 2, true);
        juce::Graphics cached(staticBackdrop);
        cached.addTransform(juce::AffineTransform::scale(2.0f));
        paintStatic(cached);
    }
    g.drawImage(staticBackdrop, 0, 0, designW, designH,
                0, 0, designW*2, designH*2);

    drawMeter(g,{275,112,325,159},meterValues[0],peakHoldValues[0],"OUTPUT  /  L");
    drawMeter(g,{612,112,325,159},meterValues[1],peakHoldValues[1],"OUTPUT  /  R");
    drawReduction(g);

    // Read the actual attached state, so DAW automation updates the status.
    const bool bypassed = bypass.getToggleState();
    font(g,10,true);
    g.setColour(bypassed ? juce::Colour(0xffffbb7d) : neon);
    text(g,bypassed ? "BYPASSED" : "ACTIVE",
         186,246,65,19,juce::Justification::centredLeft);
}

void SkellyMasterAudioProcessorEditor::resized() {
    const float sx = static_cast<float>(getWidth())/designW;
    const float sy = static_cast<float>(getHeight())/designH;
    const auto place = [sx,sy](juce::Component& c, int x, int y, int w, int h) {
        c.setBounds(juce::roundToInt(x*sx),juce::roundToInt(y*sy),
                    juce::roundToInt(w*sx),juce::roundToInt(h*sy));
    };
    // JUCE text-entry boxes are sized in pixels, unlike the painted vector
    // labels: resize them explicitly to prevent overlap at 65% of old default.
    const int entryW = juce::jlimit(50, 122, juce::roundToInt(95.0f*sx));
    const int entryH = juce::jlimit(18, 32, juce::roundToInt(27.0f*sy));
    for (int i=0;i<knobCount;++i)
        knobs[static_cast<size_t>(i)].setTextBoxStyle(
            juce::Slider::TextBoxRight, false, entryW, entryH);
    for (int i=0;i<8;++i) {
        const int row=i/4, col=i%4;
        const int cellX=34+col*283;
        const int rowY=row==0?315:485;
        place(knobs[static_cast<size_t>(i)],cellX+15,rowY+5,247,118);
    }
    place(bypass,289,701,115,37);
    place(reset,418,701,115,37);
    place(knobs[8],647,685,199,68);
    place(knobs[9],929,685,181,68);
}
