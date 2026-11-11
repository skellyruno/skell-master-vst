#include "PluginEditor.h"
#include "UI/StarSkullMark.h"
#include "UI/HardwareFinish.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr float pi = juce::MathConstants<float>::pi;
namespace hw = skelly::hardware;
const juce::Colour neon(0xffcaff5d);
const juce::Colour mint(0xff82d48b);
const juce::Colour edge = hw::brass;
const juce::Colour ink = hw::black;
const juce::Colour panel = hw::iron;
const juce::Colour pale = hw::parchment;
const juce::Colour muted(0xffbda776);
// Warm near-white for all live numeric readouts; status LEDs remain green.
const juce::Colour readoutWhite(0xfffaf9f6);

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
    // Georgia is the supplied vintage serif face on Windows.
    // Linux/Xvfb has DejaVu Serif, so the shared renderer can check an
    // equivalent physical-engraved style rather than silently using sans.
   #if JUCE_LINUX
    constexpr const char* family = "DejaVu Serif";
   #else
    constexpr const char* family = "Georgia";
   #endif
    g.setFont(juce::Font(juce::FontOptions(size)
                        .withName(family)
                        .withStyle(bold ? "Bold" : "Regular")));
}
void text(juce::Graphics& g, const juce::String& s, float x, float y,
          float w, float h, juce::Justification j = juce::Justification::centred) {
    g.drawFittedText(s, juce::Rectangle<int>(juce::roundToInt(x),
                                          juce::roundToInt(y),
                                          juce::roundToInt(w),
                                          juce::roundToInt(h)), j, 1);
}
void plate(juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f) {
    hw::plate(g,r,false,static_cast<uint32_t>(r.getX()*37+r.getY()*97),radius,0.5f);
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
    setColour(juce::Slider::textBoxOutlineColourId, edge);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff0c0c08));
    setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0xff30613d));
    setColour(juce::Slider::thumbColourId, hw::gold);
    setColour(juce::TextButton::textColourOffId, pale);
    setColour(juce::TextButton::textColourOnId, hw::gold);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::buttonOnColourId, hw::lacquer);
}
void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::setSkin(
        skellcore::ui::SkullSkin selected) {
    const auto p=skellcore::ui::skullPalette(selected);
    setColour(juce::Slider::textBoxTextColourId,p.bone);
    setColour(juce::Slider::textBoxOutlineColourId,p.line);
    setColour(juce::Slider::textBoxBackgroundColourId,p.background);
    setColour(juce::Slider::thumbColourId,
              selected==skellcore::ui::SkullSkin::Ossuary ? hw::gold : p.accent);
    setColour(juce::Label::textColourId,p.bone);
    setColour(juce::TextButton::textColourOffId,p.bone);
    setColour(juce::TextButton::textColourOnId,p.bone);
    setColour(juce::TextButton::buttonColourId,p.panel);
    setColour(juce::TextButton::buttonOnColourId,p.panelHi);
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawLabel(
    juce::Graphics& g, juce::Label& label) {
    const auto r=label.getLocalBounds().toFloat().reduced(0.65f);
    g.setColour(juce::Colour(0xff030302));
    g.fillRoundedRectangle(r.translated(0,1.4f),4.5f);
    juce::ColourGradient metal(juce::Colour(0xff5b4328),r.getX(),r.getY(),
                               juce::Colour(0xff1b150c),r.getX(),r.getBottom(),false);
    metal.addColour(0.20,juce::Colour(0xff9c7641));
    g.setGradientFill(metal);
    g.fillRoundedRectangle(r,4.5f);
    g.setColour(juce::Colour(0xff0d0c08));
    g.fillRoundedRectangle(r.reduced(2.5f),3.0f);
    g.setColour(hw::gold.withAlpha(0.48f));
    g.drawRoundedRectangle(r.reduced(0.8f),4.0f,0.8f);
    g.setColour(hw::brass.withAlpha(0.33f));
    g.drawLine(r.getX()+5,r.getBottom()-3,r.getRight()-5,r.getBottom()-3,0.8f);
    if (r.getWidth() > 75.0f) {
        for (float cx : {r.getX()+5.5f,r.getRight()-5.5f}) {
            g.setColour(juce::Colour(0xffa98d5e));
            g.fillEllipse(cx-1.8f,r.getCentreY()-1.8f,3.6f,3.6f);
            g.setColour(juce::Colour(0xff2a170c));
            g.drawLine(cx-1.1f,r.getCentreY()+0.9f,cx+1.0f,r.getCentreY()-1.1f,0.75f);
        }
    }
    if (!label.isBeingEdited()) {
        g.setFont(getLabelFont(label));
        g.setColour(findColour(juce::Label::textColourId));
        const int inset=r.getWidth()>75.0f?10:4;
        g.drawFittedText(label.getText(), label.getLocalBounds().reduced(inset,2),
                         label.getJustificationType(), 1);
    }
}

juce::Font SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::getLabelFont(juce::Label& label) {
    const auto size = juce::jlimit(10.0f, 24.0f,
                    static_cast<float>(label.getHeight()) * 0.85f);
   #if JUCE_LINUX
    constexpr const char* family = "DejaVu Serif";
   #else
    constexpr const char* family = "Georgia";
   #endif
    return juce::Font(juce::FontOptions(size).withName(family).withStyle("Bold"));
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawButtonBackground(
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) {
    const auto r=b.getLocalBounds().toFloat().reduced(1.0f);
    const bool active=b.getToggleState();
    g.setColour(juce::Colour(0xff030403));
    g.fillRoundedRectangle(r.translated(0,2),9.0f);
    juce::ColourGradient bezel(juce::Colour(0xffd1b27b),r.getX(),r.getY(),
                               juce::Colour(0xff44331c),r.getX(),r.getBottom(),false);
    bezel.addColour(0.42,juce::Colour(0xff917347));
    g.setGradientFill(bezel);
    g.fillRoundedRectangle(r,9.0f);
    const auto inner=r.reduced(3.2f);
    juce::ColourGradient face(down?juce::Colour(0xff0c1f12):juce::Colour(0xff343023),
                              inner.getX(),inner.getY(),
                              active?juce::Colour(0xff15462b):juce::Colour(0xff100f0b),
                              inner.getX(),inner.getBottom(),false);
    face.addColour(0.30,active?juce::Colour(0xff265637):juce::Colour(0xff28251c));
    g.setGradientFill(face);
    g.fillRoundedRectangle(inner,6.8f);
    g.setColour(active?hw::emerald.brighter(0.3f):hw::brass.withAlpha(0.70f));
    g.drawRoundedRectangle(inner,6.8f,1.1f);
    g.setColour(juce::Colours::white.withAlpha(hover?0.22f:0.10f));
    g.drawLine(inner.getX()+8,inner.getY()+2,inner.getRight()-8,inner.getY()+2,0.9f);
    if (r.getWidth()>70) {
        hw::screw(g,r.getX()+8.0f,r.getCentreY(),0.44f);
        hw::screw(g,r.getRight()-8.0f,r.getCentreY(),0.44f);
    }
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawButtonText(
    juce::Graphics& g, juce::TextButton& b, bool, bool) {
    font(g,juce::jlimit(10.0f,17.0f,static_cast<float>(b.getHeight())*0.42f),true);
    g.setColour(findColour(juce::TextButton::textColourOffId));
    g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(12,3),
                     juce::Justification::centred,1);
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float position, float start, float end, juce::Slider& slider) {
    if (width < 16 || height < 16) return;
    const float cx=x+width*0.5f, cy=y+height*0.5f;
    const float radius=std::min(static_cast<float>(width),
                                static_cast<float>(height))*0.445f;
    const float startAngle=start+0.04f, endAngle=end-0.04f;
    float p=std::isfinite(position) ? juce::jlimit(0.0f,1.0f,position) : 0.5f;
    // OUTPUT has an asymmetric dB range, but its neutral 0 dB must
    // visibly point straight up. This only changes marker geometry;
    // the original parameter range and host automation remain intact.
    if (slider.getComponentID()=="utility-output") {
        const float v=static_cast<float>(slider.getValue());
        p=v<=0.0f ? 0.5f*juce::jlimit(0.0f,1.0f,(v+18.0f)/18.0f)
                   : 0.5f+0.5f*juce::jlimit(0.0f,1.0f,v/12.0f);
    }
    const float angle=startAngle+p*(endAngle-startAngle);
    const auto accent=getColour(juce::Slider::thumbColourId);
    // Fixed engraved graduation ring: never rotate a photographic ring
    // or asymmetrical texture over these ticks.
    for(int tick=0;tick<=40;++tick) {
        const float a=startAngle+(endAngle-startAngle)*tick/40.0f;
        const float ss=std::sin(a),cs=-std::cos(a);
        const float d0=radius*(tick%5==0?0.91f:0.98f);
        const float d1=radius*(tick%5==0?1.14f:1.10f);
        g.setColour(accent.withAlpha(tick%5==0?0.91f:0.53f));
        g.drawLine(cx+ss*d0,cy+cs*d0,cx+ss*d1,cy+cs*d1,
                   tick%5==0?1.25f:0.80f);
    }
    // Concentric, perfectly centred machined collar and knob. Stationary
    // reflections keep the body from wobbling during dragging.
    const float seat=radius*0.88f;
    g.setColour(juce::Colour(0xff020302));
    g.fillEllipse(cx-seat-2.0f,cy-seat-1.0f,seat*2+4.0f,seat*2+4.0f);
    g.setColour(hw::brass.withAlpha(0.78f));
    g.drawEllipse(cx-seat,cy-seat,seat*2,seat*2,1.4f);
    const float cap=radius*0.785f;
    juce::ColourGradient metal(juce::Colour(0xff81715b),cx-cap,cy-cap,
                               juce::Colour(0xff090b09),cx+cap,cy+cap,false);
    metal.addColour(0.20,juce::Colour(0xff34332e));
    metal.addColour(0.60,juce::Colour(0xff1c1e1b));
    g.setGradientFill(metal);
    g.fillEllipse(cx-cap,cy-cap,cap*2,cap*2);
    g.setColour(juce::Colours::black.withAlpha(0.68f));
    g.drawEllipse(cx-cap+1.8f,cy-cap+1.8f,(cap-1.8f)*2,(cap-1.8f)*2,1.2f);
    for(int i=0;i<60;++i) {
        const float a=2.0f*pi*static_cast<float>(i)/60.0f;
        const float ss=std::sin(a),cs=std::cos(a);
        g.setColour((i%4==0?hw::gold:juce::Colours::black)
                    .withAlpha(i%4==0?0.21f:0.33f));
        g.drawLine(cx+ss*cap*0.92f,cy+cs*cap*0.92f,
                   cx+ss*cap*0.985f,cy+cs*cap*0.985f,0.8f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.105f));
    g.drawEllipse(cx-cap*0.72f,cy-cap*0.72f,cap*1.44f,cap*1.44f,0.8f);
    g.setColour(accent.withAlpha(0.55f));
    g.drawEllipse(cx-cap+0.8f,cy-cap+0.8f,(cap-0.8f)*2,(cap-0.8f)*2,0.8f);
    // The ONLY rotating feature is this symmetrical ivory/brass indicator.
    const float ss=std::sin(angle),cs=-std::cos(angle);
    const float inner=cap*0.28f,outer=cap*0.84f;
    g.setColour(juce::Colours::black.withAlpha(0.80f));
    g.drawLine(cx+ss*inner+1.3f,cy+cs*inner+1.4f,
               cx+ss*outer+1.3f,cy+cs*outer+1.4f,7.4f);
    g.setColour(hw::brass);
    g.drawLine(cx+ss*inner,cy+cs*inner,cx+ss*outer,cy+cs*outer,5.4f);
    g.setColour(accent);
    g.drawLine(cx+ss*inner,cy+cs*inner,cx+ss*outer,cy+cs*outer,3.35f);
    g.setColour(juce::Colour(0xff10100c));
    g.fillEllipse(cx-2.5f,cy-2.5f,5.0f,5.0f);
}

SkellyMasterAudioProcessorEditor::SkellyMasterAudioProcessorEditor(SkellyMasterAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p) {
    skin=loadSkin();
    lookAndFeel.setSkin(skin);
    setLookAndFeel(&lookAndFeel);
    skullHitTarget.onClick=[this] { switchSkin(); };
    addAndMakeVisible(skullHitTarget);

    for (int i = 0; i < knobCount; ++i) {
        auto& s = knobs[static_cast<size_t>(i)];
        s.setComponentID(i==8 ? "utility-output" : i==9 ? "utility-ceiling" : "primary");
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, i >= 8 ? 85 : 87, 27);
        s.setRotaryParameters(pi * 1.25f, pi * 2.75f, true);
        s.setMouseDragSensitivity(190);
        s.setVelocityBasedMode(false);
        s.setDoubleClickReturnValue(true, 0.0);
        s.setNumDecimalPlacesToDisplay(0);
        const bool db = i == 0 || i == 4 || i == 5 || i == 8 || i == 9;
        s.setTextValueSuffix(i==9 ? "" : db ? " dB" : " %");
        const juce::String sub = i < 8 ? subtitles[i] : (i == 8 ? "OUTPUT TRIM" : "PEAK CEILING");
        s.setTooltip(i==9
            ? "CEILING ADJUST: 0 = -1 dBFS limiting; -3 = -3 dBFS, +3 = -0.1 dBFS. Drag, type, or double-click to reset."
            : juce::String(labels[i]) + " - " + sub +
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

skellcore::ui::SkullSkin SkellyMasterAudioProcessorEditor::loadSkin() {
    const auto file=juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("SkellAudio").getChildFile("skellcore-skull-skin-v1.txt");
    return file.existsAsFile()
        ? skellcore::ui::skinFromIndex(file.loadFileAsString().trim().getIntValue())
        : skellcore::ui::SkullSkin::Ossuary;
}

void SkellyMasterAudioProcessorEditor::saveSkin(skellcore::ui::SkullSkin selected) {
    const auto file=juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("SkellAudio").getChildFile("skellcore-skull-skin-v1.txt");
    if(file.getParentDirectory().createDirectory().wasOk())
        file.replaceWithText(juce::String(static_cast<int>(selected))+"\n");
}

void SkellyMasterAudioProcessorEditor::switchSkin() {
    // Purely UI/message-thread, never DSP. No new audio parameter or preset.
    skin=skellcore::ui::nextSkin(skin);
    lookAndFeel.setSkin(skin);
    saveSkin(skin);
    staticBackdrop=juce::Image(); // forces a correctly recoloured cached image
    repaint();
}

void SkellyMasterAudioProcessorEditor::recolourStaticBackdrop(
        juce::Image& result) const {
    if(skin==skellcore::ui::SkullSkin::Ossuary || !result.isValid()) return;
    // SkellCore canonical palettes. Preserve physical texture luminance,
    // but remap brass and green enamel into each skin. Runs only when the
    // cached backdrop is regenerated after a theme change or resizing.
    const auto p=skellcore::ui::skullPalette(skin);
    juce::Image::BitmapData pixels(result,juce::Image::BitmapData::readWrite);
    for(int y=0;y<pixels.height;++y)
        for(int x=0;x<pixels.width;++x) {
            const auto c=pixels.getPixelColour(x,y);
            if(c.getAlpha()==0) continue;
            const float r=c.getFloatRed(),gg=c.getFloatGreen(),
                        b=c.getFloatBlue();
            const float lum=0.23f*r+0.67f*gg+0.10f*b;
            const bool warm=r>gg*1.05f && gg>b*1.07f && r>0.22f;
            const bool green=gg>r*1.22f && gg>b*1.13f && gg>0.10f;
            juce::Colour base=p.panel;
            float brightness=1.0f;
            if(warm) {
                base=p.bone;
                brightness=0.72f+0.40f*lum;
            } else if(green) {
                base=p.accent;
                brightness=0.48f+0.65f*lum;
            } else if(skin==skellcore::ui::SkullSkin::Ivory) {
                base=p.panel;
                // Reverse lightness for legible dark engraved iron edges.
                brightness=juce::jlimit(0.20f,1.0f,1.05f-0.75f*lum);
            } else {
                base=p.panel;
                brightness=0.36f+1.20f*lum;
            }
            pixels.setPixelColour(x,y,juce::Colour::fromFloatRGBA(
                juce::jlimit(0.0f,1.0f,base.getFloatRed()*brightness),
                juce::jlimit(0.0f,1.0f,base.getFloatGreen()*brightness),
                juce::jlimit(0.0f,1.0f,base.getFloatBlue()*brightness),
                c.getFloatAlpha()));
        }
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
    const bool bypassed = bypass.getToggleState();

    // Meter graphics are relatively expensive at a compact size. While the
    // DAW is stopped, the timer should not repaint the whole bridge 30x/s.
    const bool metersChanged = [&] {
        if (std::abs(displayedReduction - lastPaintedReduction) >= 0.025f
            || (lastPaintedReduction > 0.015f && displayedReduction <= 0.015f))
            return true;
        for (size_t i=0;i<meterValues.size();++i)
            if (std::abs(meterValues[i]-lastPaintedMeterValues[i]) >= 0.0001f
                || std::abs(peakHoldValues[i]-lastPaintedPeakHoldValues[i]) >= 0.0001f)
                return true;
        return false;
    }();

    const float sx = static_cast<float>(getWidth()) / designW;
    const float sy = static_cast<float>(getHeight()) / designH;
    const auto dirty = [this, sx, sy](int x, int y, int w, int h) {
        repaint(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                juce::roundToInt(w * sx) + 2, juce::roundToInt(h * sy) + 2);
    };
    if (metersChanged) {
        // Accumulate sub-threshold movement between paints rather than
        // freezing GR text or the VU cursor at a stale low value.
        lastPaintedMeterValues = meterValues;
        lastPaintedPeakHoldValues = peakHoldValues;
        lastPaintedReduction = displayedReduction;
        dirty(270, 105, 901, 177);
    }
    if (bypassed != lastDisplayedBypass) {
        lastDisplayedBypass = bypassed;
        dirty(174, 243, 85, 24);
    }
}

void SkellyMasterAudioProcessorEditor::drawScrew(juce::Graphics& g,float x,float y) const {
    hw::screw(g,x,y,0.86f);
}

void SkellyMasterAudioProcessorEditor::drawMeter(
    juce::Graphics& g, juce::Rectangle<float> r, float value,
    float heldValue, const juce::String& name) const {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(
        r.getWidth()/310.0f,r.getHeight()/156.0f).translated(r.getX(),r.getY()));
    hw::plate(g,{0,0,310,156},false,0x52b4u,9.0f,0.24f);
    for(float xx : {11.5f,298.5f})
        for(float yy : {12.0f,144.0f}) hw::screw(g,xx,yy,0.72f);
    g.setColour(juce::Colour(0xff040404));
    g.fillRoundedRectangle(16,23,278,120,29.0f);
    font(g,14,true);g.setColour(hw::parchment);
    text(g,name,40,5,230,18);

    // Needle-free meter face: static lens, scale and skull cached at 2x.
    // Unlike the previous JPEG, this has NO baked needle or ghost shadow.
    // Real signal/peak-hold needles are the only moving pieces.
    static const juce::Image cleanFace=[] {
        juce::Image image(juce::Image::ARGB,620,312,true);
        juce::Graphics f(image);
        f.addTransform(juce::AffineTransform::scale(2.0f));
        juce::ColourGradient paper(juce::Colour(0xfff9f9f4),155,27,
                                   juce::Colour(0xffc7c5be),155,143,false);
        paper.addColour(0.42,juce::Colour(0xfff4f3ec));
        paper.addColour(0.81,juce::Colour(0xffe4e1d9));
        f.setGradientFill(paper);
        f.fillRoundedRectangle(19.0f,26.0f,272.0f,113.0f,27.0f);
        // Subtle deterministic paper patina, independent of meter refresh.
        for(int i=0;i<500;++i) {
            const auto seed=hw::noise(static_cast<uint32_t>(i*2677+0x789bu));
            const float px=24.0f+(seed%12037u)/12037.0f*261.0f;
            const float py=29.0f+((seed>>12)%12011u)/12011.0f*105.0f;
            if(px>21.0f && px<288.0f) {
                f.setColour(juce::Colour(0xff6c6256).withAlpha(0.04f));
                f.fillEllipse(px,py,0.4f,0.4f);
            }
        }
        constexpr float cx=155.0f,cy=141.0f,rad=111.0f;
        const auto angle=[](float p) { return (-157.0f+134.0f*p)*pi/180.0f; };
        juce::Path scaleArc;
        for(int i=0;i<=100;++i) {
            const float a=angle(i/100.0f);
            const float px=cx+std::cos(a)*rad, py=cy+std::sin(a)*rad;
            if(i==0) scaleArc.startNewSubPath(px,py);
            else scaleArc.lineTo(px,py);
        }
        f.setColour(juce::Colour(0xff3c362d));
        f.strokePath(scaleArc,juce::PathStrokeType(1.45f));
        for(int tick=0;tick<=30;++tick) {
            const float a=angle(tick/30.0f);
            const float ux=std::cos(a),uy=std::sin(a);
            const float outer=rad+1.5f;
            const float inner=rad-(tick%5==0?11.5f:5.6f);
            f.setColour(juce::Colour(0xff41382d));
            f.drawLine(cx+ux*inner,cy+uy*inner,
                       cx+ux*outer,cy+uy*outer,
                       tick%5==0?1.25f:0.65f);
        }
        // Fixed red OVERLOAD scale is a printed *zone*. The moving red
        // indicator is rendered separately from live audio below.
        juce::Path danger;
        for(int i=0;i<=36;++i) {
            const float a=angle(0.83f+i*(0.17f/36.0f));
            const float px=cx+std::cos(a)*rad,py=cy+std::sin(a)*rad;
            if(i==0)danger.startNewSubPath(px,py);
            else danger.lineTo(px,py);
        }
        f.setColour(juce::Colour(0xffb1281a));
        f.strokePath(danger,juce::PathStrokeType(3.4f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        font(f,11,true);
        f.setColour(juce::Colour(0xff28241f));
        text(f,"-48",25,94,36,14);
        text(f,"-36",45,71,41,15);
        text(f,"-24",100,48,40,15);
        text(f,"-12",171,48,40,15);
        text(f,"-6",225,72,31,15);
        text(f,"0",266,93,19,15);
        skelly::ui::drawStarSkull(f,{142,80,26,23},0.75f,
                                  juce::Colour(0xff392e22));
        font(f,8,true);f.setColour(juce::Colour(0xff342b1f));
        text(f,"SKELLY",126,105,58,11);
        f.setColour(juce::Colour(0xff29251e).withAlpha(0.55f));
        f.drawRoundedRectangle(juce::Rectangle<float>(19,26,272,113),27.0f,1.8f);
        return image;
    }();
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(cleanFace,19.0f,24.0f,272.0f,119.0f,
                38,48,544,238);
    juce::ColourGradient reflection(juce::Colours::white.withAlpha(0.15f),
                                     155.0f,27.0f,
                                     juce::Colours::white.withAlpha(0.0f),
                                     155.0f,73.0f,false);
    g.setGradientFill(reflection);
    g.fillRoundedRectangle(24.0f,28.0f,262.0f,42.0f,18.0f);

    constexpr float pivotX=155.0f,pivotY=141.0f,radius=111.0f;
    const float normalized=normalizedPeak(value);
    const float a=(-157.0f+134.0f*normalized)*pi/180.0f;
    const float ux=std::cos(a),uy=std::sin(a),nx=-uy,ny=ux;
    const float tipX=pivotX+ux*radius*0.965f;
    const float tipY=pivotY+uy*radius*0.965f;
    juce::Graphics::ScopedSaveState needleClip(g);
    g.reduceClipRegion(juce::Rectangle<int>(20,30,271,106));
    // Red arc *illumination* and peak marker follow audio as it approaches 0.
    if(normalized>0.83f) {
        juce::Path activeDanger;
        const int steps=juce::jmax(2,juce::roundToInt((normalized-0.83f)*96.0f));
        for(int i=0;i<=steps;++i) {
            const float t=0.83f+(normalized-0.83f)*i/static_cast<float>(steps);
            const float aa=(-157.0f+134.0f*t)*pi/180.0f;
            const float px=pivotX+std::cos(aa)*(radius+1.5f);
            const float py=pivotY+std::sin(aa)*(radius+1.5f);
            if(i==0)activeDanger.startNewSubPath(px,py);
            else activeDanger.lineTo(px,py);
        }
        g.setColour(juce::Colour(0xffff493a).withAlpha(0.92f));
        g.strokePath(activeDanger,juce::PathStrokeType(3.0f));
    }
    // A single tapered, centered mechanical needle that reacts to the
    // actual output channel. No photo pointer remains underneath it.
    juce::Path needle;
    needle.startNewSubPath(pivotX+nx*2.4f,pivotY+ny*2.4f);
    needle.lineTo(tipX+nx*0.28f,tipY+ny*0.28f);
    needle.lineTo(tipX-nx*0.28f,tipY-ny*0.28f);
    needle.lineTo(pivotX-nx*2.4f,pivotY-ny*2.4f);
    needle.closeSubPath();
    g.setColour(juce::Colours::black.withAlpha(0.42f));
    g.fillPath(needle,juce::AffineTransform::translation(1.2f,1.5f));
    g.setColour(normalized>0.83f ? juce::Colour(0xff9d2218)
                                  : juce::Colour(0xff342618));
    g.fillPath(needle);
    g.setColour(normalized>0.83f ? juce::Colour(0xffff5743)
                                  : hw::brass);
    g.drawLine(pivotX+nx*.32f,pivotY+ny*.32f,
               tipX+nx*.18f,tipY+ny*.18f,0.65f);
    if(heldValue>0.0001f) {
        const float ah=(-157.0f+134.0f*normalizedPeak(heldValue))*pi/180.0f;
        const float hx=pivotX+std::cos(ah)*radius;
        const float hy=pivotY+std::sin(ah)*radius;
        g.setColour(normalizedPeak(heldValue)>0.83f
            ? juce::Colour(0xffff5134) : hw::gold);
        g.fillEllipse(hx-2.3f,hy-2.3f,4.6f,4.6f);
    }

    // Pivot and live INTEGER dBFS readout hide the needle origin.
    g.setColour(juce::Colours::black.withAlpha(0.74f));
    g.fillRoundedRectangle(101,126,108,24,6.0f);
    juce::ColourGradient bronze(hw::brass,103,126,
                                juce::Colour(0xff42321d),207,149,false);
    g.setGradientFill(bronze);
    g.fillRoundedRectangle(106,127,98,22,5.0f);
    g.setColour(juce::Colour(0xff17160f));
    g.fillRoundedRectangle(109,129,92,18,4.0f);
    font(g,12,true);g.setColour(readoutWhite);
    text(g,juce::String(juce::roundToInt(peakDb(value)))+" dBFS",
         108,128,94,19);
    hw::screw(g,107,137,0.28f);
    hw::screw(g,203,137,0.28f);
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawLine(43,29,264,29,0.9f);
}

void SkellyMasterAudioProcessorEditor::drawReduction(juce::Graphics& g) const {
    hw::plate(g,{953,108,216,170},false,0x300ad5u,9.0f,0.34f);
    for (float xx : {965.0f,1157.0f})
        for (float yy : {120.0f,265.0f}) hw::screw(g,xx,yy,0.63f);
    font(g,12,true);g.setColour(hw::parchment);
    text(g,"GAIN REDUCTION",969,125,185,17);
    g.setColour(juce::Colour(0xff030702));
    g.fillRoundedRectangle(976,152,173,56,8.0f);
    g.setColour(hw::brass.withAlpha(0.5f));
    g.drawRoundedRectangle(juce::Rectangle<float>(976,152,173,56),8.0f,1.0f);
    font(g,37,true);g.setColour(hw::phosphor);
    text(g,juce::String(juce::roundToInt(displayedReduction)),985,158,130,47);
    font(g,12,true);g.setColour(hw::gold);
    text(g,"dB",1117,178,27,14);
    const float level=juce::jlimit(0.0f,16.0f,displayedReduction*(16.0f/18.0f));
    for(int i=0;i<16;++i) {
        g.setColour(i<level?(i>12?hw::gold:hw::phosphor):
                              juce::Colour(0xff766a54));
        g.fillRoundedRectangle(976.0f+i*10.4f,229,6.9f,17,2.1f);
    }
}

void SkellyMasterAudioProcessorEditor::paintStatic(juce::Graphics& g) {
    g.setColour(juce::Colour(0xff060705));
    g.fillRect(0,0,designW,designH);
    // Iron chassis and worn brass corners. Expensive texture is cached,
    // never recalculated during realtime DSP or normal meter refresh.
    hw::plate(g,{3,3,1194,754},false,0x310570u,14.0f,1.15f);
    g.setColour(hw::brass.withAlpha(0.26f));
    g.drawRoundedRectangle(juce::Rectangle<float>(8,8,1184,744),12.0f,1.0f);

    // Nameplate and skulls are separate transparent photographic resources,
    // drawn once into the existing editor faceplate cache, never per meter tick.
    hw::plate(g,{405,7,391,73},true,0x001ab3u,10.0f,0.9f);
   #if SKELLY_MASTER_HEADER_ASSETS
    static const juce::Image photoHeader = juce::ImageFileFormat::loadFrom(
        BinaryData::header_nameplate_green_blank_png,
        static_cast<size_t>(BinaryData::header_nameplate_green_blank_pngSize));
    static const juce::Image photoSkullLeft = juce::ImageFileFormat::loadFrom(
        BinaryData::logo_skull_header_left_png,
        static_cast<size_t>(BinaryData::logo_skull_header_left_pngSize));
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    if (photoHeader.isValid())
        g.drawImage(photoHeader,405.0f,7.0f,391.0f,73.0f,
                    0,0,photoHeader.getWidth(),photoHeader.getHeight());
    else
   #endif
    {
        for(float x : {417.0f,784.0f})
            for(float y : {21.0f,66.0f}) hw::screw(g,x,y,0.83f);
    }
   #if SKELLY_MASTER_HEADER_ASSETS
    if (photoSkullLeft.isValid())
        g.drawImage(photoSkullLeft,57.0f,10.0f,82.0f,72.0f,
                    0,0,photoSkullLeft.getWidth(),photoSkullLeft.getHeight());
    else
   #endif
        skelly::ui::drawStarSkull(g,{57,10,82,72},0.98f,hw::gold);
    font(g,31,true);g.setColour(hw::gold);
    text(g,"SKELLY",145,15,185,38,juce::Justification::centredLeft);
    font(g,16,true);g.setColour(hw::parchment);
    text(g,"AUDIO LABS",149,55,172,23,juce::Justification::centredLeft);
    hw::divider(g,342,17,342,78);
    font(g,36,true);g.setColour(hw::gold);
    text(g,"SKELLY MASTER",428,11,347,43);
    font(g,14,true);g.setColour(hw::parchment);
    text(g,"STEREO MASTERING SUITE  /  M-01",422,53,359,22);
    hw::divider(g,861,18,861,76);
    font(g,16,true);g.setColour(hw::gold);
    text(g,"PRECISION / POWER",885,27,191,28);
    // The skull on the right was removed; left skull alone changes skin.

    // Engraved meter bridge with a separate stamped status panel.
    hw::plate(g,{37,94,1126,200},false,0x33c8a7u,12.0f,0.72f);
    hw::plate(g,{48,109,213,176},false,0x641b4fu,9.0f,0.52f);
    for(float x : {60.0f,249.0f})
        for(float y : {121.0f,273.0f}) hw::screw(g,x,y,0.67f);
    font(g,18,true);g.setColour(hw::gold);
    text(g,"SKELLY MASTER",65,125,182,26);
    font(g,12,true);g.setColour(hw::parchment);
    text(g,"PRECISION MASTER BUS / M-01",61,154,189,20);
    hw::divider(g,83,182,225,182);
    font(g,15,true);g.setColour(hw::parchment);
    text(g,"4X OVERSAMPLING",65,191,188,19);
    text(g,"STEREO-LINKED DSP",65,213,188,19);
    text(g,"LOOKAHEAD LIMITING",65,235,188,19);
    g.setColour(juce::Colour(0xff123b1d));
    g.fillEllipse(82,259,13,13);
    g.setColour(hw::phosphor);
    g.fillEllipse(85.4f,262.4f,6.2f,6.2f);
    font(g,11,true);g.setColour(hw::phosphor);
    text(g,"ENGINE STATUS",98,253,104,20,juce::Justification::centredLeft);

    // Modular lower rack: same genuine APVTS controls as previous build.
    hw::plate(g,{37,298,1126,356},false,0x8b113au,12.0f,1.0f);
    for(float x : {51.0f,1149.0f})
        for(float y : {310.0f,641.0f}) hw::screw(g,x,y,0.75f);
    hw::divider(g,64,480,1138,480);
    for(int col=1;col<4;++col) {
        const float x=34.0f+col*283.0f;
        hw::divider(g,x,321,x,463);
        hw::divider(g,x,492,x,637);
    }
    for(int i=0;i<8;++i) {
        const int row=i/4,col=i%4;
        const float cellX=34.0f+283.0f*col;
        const float rowY=row?485.0f:315.0f;
        font(g,19,true);g.setColour(hw::gold);
        text(g,labels[i],cellX+13,rowY+123,178,24);
        font(g,14,true);g.setColour(hw::parchment);
        text(g,subtitles[i],cellX+9,rowY+148,194,16);
    }

    // Emerald (not red) heavy lacquer utility deck.
    hw::plate(g,{13,673,1174,80},true,0x731ac4u,9.0f,0.99f);
    for(float x : {29.0f,1172.0f})
        for(float y : {685.0f,741.0f}) hw::screw(g,x,y,0.78f);
    hw::divider(g,267,685,267,744);
    hw::divider(g,553,685,553,744);
    hw::divider(g,850,685,850,744);
    hw::divider(g,1121,685,1121,744);
    font(g,16,true);g.setColour(hw::gold);
    text(g,"// MASTER BUS",48,696,205,25,juce::Justification::centredLeft);
    font(g,14,true);g.setColour(hw::parchment);
    text(g,"TONE IS EVERYTHING",66,724,184,18,juce::Justification::centredLeft);
    font(g,15,true);g.setColour(hw::gold);
    text(g,"OUTPUT",566,710,84,21,juce::Justification::centredLeft);
    text(g,"CEILING",852,710,88,21,juce::Justification::centredLeft);
    font(g,16,true);g.setColour(hw::gold);
    text(g,"M-01",1125,705,41,24);

    // Clean uninterrupted rack edges: removable side handles intentionally
    // omitted at the user's request. Keep the two corner mounting screws.
    for(float x : {25.0f,1176.0f})
       hw::screw(g,x,24.0f,0.75f);
}

void SkellyMasterAudioProcessorEditor::paint(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / designW,
                                                static_cast<float>(getHeight()) / designH));

    // Scale decorative cache to the actual display resolution, not a fixed
    // 2400x1520 allocation per instance. Quantize to 128px steps to avoid
    // repeated reallocations while the user drags a resize corner.
    // The compact default now uses about 77% less backdrop memory while
    // preserving >1.5 source pixels per displayed pixel.
    const int desiredW = juce::roundToInt(static_cast<float>(getWidth()) * 1.5f);
    const int rasterW = juce::jlimit(960, 2400, ((desiredW + 127) / 128) * 128);
    const int rasterH = juce::roundToInt(static_cast<float>(rasterW) * designH / designW);
    if (staticBackdrop.isNull() || staticBackdrop.getWidth() != rasterW
        || staticBackdrop.getHeight() != rasterH) {
        juce::Image updated(juce::Image::ARGB, rasterW, rasterH, true);
        {
            juce::Graphics cached(updated);
            cached.addTransform(juce::AffineTransform::scale(
                static_cast<float>(rasterW) / designW,
                static_cast<float>(rasterH) / designH));
            paintStatic(cached);
        }
        recolourStaticBackdrop(updated); // one-time UI-thread remap per skin/size
        staticBackdrop = std::move(updated);
    }
    g.drawImage(staticBackdrop, 0, 0, designW, designH,
                0, 0, rasterW, rasterH);

    drawMeter(g,{275,112,325,159},meterValues[0],peakHoldValues[0],"OUTPUT  /  L");
    drawMeter(g,{612,112,325,159},meterValues[1],peakHoldValues[1],"OUTPUT  /  R");
    drawReduction(g);

    // Read the actual attached state, so DAW automation updates the status.
    const bool bypassed = bypass.getToggleState();
    font(g,12,true);
    g.setColour(bypassed ? hw::gold : neon);
    text(g,bypassed ? "BYPASSED" : "ACTIVE",
         202,246,49,19,juce::Justification::centredLeft);
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
    const int entryW = juce::jlimit(50, 160, juce::roundToInt(95.0f*sx));
    const int entryH = juce::jlimit(18, 46, juce::roundToInt(27.0f*sy));
    if (entryW != lastTextBoxWidth || entryH != lastTextBoxHeight) {
        for (auto& slider : knobs)
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, entryW, entryH);
        lastTextBoxWidth = entryW;
        lastTextBoxHeight = entryH;
    }
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
    place(skullHitTarget,57,10,82,72);
}
