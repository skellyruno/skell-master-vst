#include "PluginEditor.h"
#include "UI/StarSkullMark.h"
#include "UI/HardwareFinish.h"
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
    // Georgia on Windows; JUCE selects the closest installed serif fallback
    // on the Linux SkellCore screenshot runner.
    g.setFont(juce::Font(juce::FontOptions(size)
                        .withName("Georgia")
                        .withStyle(bold ? "Bold" : "Regular")));
}
void text(juce::Graphics& g, const juce::String& s, float x, float y,
          float w, float h, juce::Justification j = juce::Justification::centred) {
    g.drawText(s, juce::Rectangle<float>(x, y, w, h), j, false);
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
        g.setColour(hw::gold);
        const int inset=r.getWidth()>75.0f?10:4;
        g.drawFittedText(label.getText(), label.getLocalBounds().reduced(inset,2),
                         label.getJustificationType(), 1);
    }
}

juce::Font SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::getLabelFont(juce::Label& label) {
    return juce::Font(juce::FontOptions(juce::jlimit(10.0f, 24.0f,
                    static_cast<float>(label.getHeight()) * 0.85f)).withStyle("Bold"));
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
    g.setColour(b.getToggleState()?hw::gold:hw::parchment);
    g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(12,3),
                     juce::Justification::centred,1);
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x,int y,int width,int height,float position,
    float start,float end,juce::Slider& slider) {
    const float cx=static_cast<float>(x)+width*.50f;
    const float cy=static_cast<float>(y)+height*.50f;
    const bool utility=slider.getComponentID()=="utility";
    const float radius=std::min(static_cast<float>(width),static_cast<float>(height))
                       *(utility?0.455f:0.462f);
    const float startAngle=start+0.05f,endAngle=end-0.05f;
    const float angle=startAngle+position*(endAngle-startAngle);

    // Embossed machine-engraved graduation ring; no fake raster knobs.
    for (int i=0;i<=40;++i) {
        const float a=startAngle+(endAngle-startAngle)*i/40.0f;
        const float si=std::sin(a),co=-std::cos(a);
        const bool major=(i%5==0);
        const float inner=radius*(major?0.93f:0.985f);
        const float outer=radius*(major?1.13f:1.09f);
        g.setColour(hw::gold.withAlpha(major?0.91f:0.48f));
        g.drawLine(cx+si*inner,cy+co*inner,cx+si*outer,cy+co*outer,major?1.3f:0.85f);
    }
    g.setColour(juce::Colour(0xff030403));
    g.fillEllipse(cx-radius*.92f,cy-radius*.91f,radius*1.84f,radius*1.84f);
    g.setColour(hw::brass.withAlpha(0.75f));
    g.drawEllipse(cx-radius*.91f,cy-radius*.91f,radius*1.82f,radius*1.82f,1.6f);

    const float outerFace=radius*.81f;
    juce::ColourGradient metal(juce::Colour(0xffc8a676),cx-outerFace,cy-outerFace,
                               juce::Colour(0xff302319),cx+outerFace,cy+outerFace,false);
    metal.addColour(0.27,juce::Colour(0xff756046));
    metal.addColour(0.65,juce::Colour(0xff16150e));
    g.setGradientFill(metal);
    g.fillEllipse(cx-outerFace,cy-outerFace,2*outerFace,2*outerFace);

    // Knurled perimeter catches specular highlights at different angles.
    for (int i=0;i<72;++i) {
        const float a=2*pi*i/72.0f;
        const float cs=std::cos(a),sn=std::sin(a);
        const float d1=outerFace*.89f,d2=outerFace*.99f;
        g.setColour(i%3==0?hw::gold.withAlpha(0.28f):
                      juce::Colours::black.withAlpha(0.38f));
        g.drawLine(cx+cs*d1,cy+sn*d1,cx+cs*d2,cy+sn*d2,0.9f);
    }
    const float face=outerFace*.84f;
    juce::ColourGradient blackSteel(juce::Colour(0xff5a4937),
                                    cx-face*.7f,cy-face*.78f,
                                    juce::Colour(0xff0c0d0b),
                                    cx+face*.72f,cy+face*.82f,false);
    blackSteel.addColour(.32,juce::Colour(0xff292820));
    blackSteel.addColour(.70,juce::Colour(0xff171610));
    g.setGradientFill(blackSteel);
    g.fillEllipse(cx-face,cy-face,face*2,face*2);
    g.setColour(hw::gold.withAlpha(0.35f));
    g.drawEllipse(cx-face+.9f,cy-face+.9f,(face-.9f)*2,(face-.9f)*2,1.0f);
    // Circular lathe markings, shaded and offset so they read as real machining.
    for (int i=0;i<7;++i) {
        const float rr=face*(0.35f+0.086f*i);
        g.setColour(i%2==0?juce::Colours::white.withAlpha(0.024f):
                            juce::Colours::black.withAlpha(0.09f));
        g.drawEllipse(cx-rr,cy-rr,rr*2,rr*2,0.55f);
    }

    // Physical ivory/brass bar pointer: bright bevel and dark cast shadow.
    const float sn=std::sin(angle),cs=-std::cos(angle);
    const float startD=face*.35f,endD=face*.94f;
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawLine(cx+sn*startD+1.7f,cy+cs*startD+1.7f,
               cx+sn*endD+1.7f,cy+cs*endD+1.7f,8.0f);
    g.setColour(hw::brass);
    g.drawLine(cx+sn*startD,cy+cs*startD,
               cx+sn*endD,cy+cs*endD,6.2f);
    g.setColour(hw::gold);
    g.drawLine(cx+sn*startD,cy+cs*startD,
               cx+sn*endD,cy+cs*endD,3.2f);
    // Warm centre inset maintains the handmade reference's proportions.
    g.setColour(juce::Colour(0xff100e09));
    g.fillEllipse(cx-3.0f,cy-3.0f,6.0f,6.0f);
    g.setColour(hw::gold.withAlpha(0.45f));
    g.drawEllipse(cx-2.3f,cy-2.3f,4.6f,4.6f,0.9f);
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
    g.addTransform(juce::AffineTransform::scale(r.getWidth()/310.0f,r.getHeight()/156.0f)
                   .translated(r.getX(),r.getY()));
    hw::plate(g,{0,0,310,156},false,0x52b4u,9.0f,0.24f);
    for (float xx : {11.5f,298.5f})
        for (float yy : {12.0f,144.0f}) hw::screw(g,xx,yy,0.72f);
    g.setColour(juce::Colour(0xff040404));
    g.fillRoundedRectangle(16,23,278,119,30.0f);
    // Warm amber illuminated mechanical meter card, like the reference.
    juce::ColourGradient paper(juce::Colour(0xffffe6ac),155,26,
                               juce::Colour(0xffa26b2d),155,140,false);
    paper.addColour(0.36,juce::Colour(0xffffd381));
    paper.addColour(0.73,juce::Colour(0xffd8a350));
    g.setGradientFill(paper);
    g.fillRoundedRectangle(19,27,272,108,27.0f);
    g.setColour(juce::Colour(0xff2b210f).withAlpha(0.72f));
    g.drawRoundedRectangle(juce::Rectangle<float>(19,27,272,108),26.0f,1.4f);
    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.drawLine(25,32,283,32,0.8f);

    font(g,14,true); g.setColour(hw::parchment);
    text(g,name,40,5,230,18);

    // Curved calibrated arc and engraved divisions, plus green peak zone.
    constexpr float pivotX=155.0f,pivotY=141.0f,radius=111.0f;
    for(int i=0;i<=30;++i) {
        const float t=static_cast<float>(i)/30.0f;
        const float a=(-157.0f+134.0f*t)*pi/180.0f;
        const float c=std::cos(a),si=std::sin(a);
        const float outer=radius,inner=radius-(i%5==0?13.5f:7.0f);
        g.setColour(i>25?juce::Colour(0xff20683d):juce::Colour(0xff3b2c19));
        g.drawLine(pivotX+c*inner,pivotY+si*inner,
                   pivotX+c*outer,pivotY+si*outer,i%5==0?1.8f:0.85f);
    }
    // Tiny authentic skull-brand ink stamp on the parchment dial.
    skelly::ui::drawStarSkull(g,{135,79,40,31},0.78f,juce::Colour(0xff392510));
    font(g,8,true);g.setColour(juce::Colour(0xff322310));
    text(g,"SKELLY",126,109,58,10);
    font(g,12,true); g.setColour(juce::Colour(0xff30200e));
    text(g,"-48",30,95,40,15);
    text(g,"-36",48,69,38,15);
    text(g,"-24",100,47,39,15);
    text(g,"-12",171,47,39,15);
    text(g,"-6",222,69,32,15);
    text(g,"0",268,95,18,15);
    // Needle is clipped under the readout plate and never leaves the meter.
    const float a=(-157.0f+134.0f*normalizedPeak(value))*pi/180.0f;
    const float tipX=pivotX+std::cos(a)*radius*.97f;
    const float tipY=pivotY+std::sin(a)*radius*.97f;
    {
        juce::Graphics::ScopedSaveState clipped(g);
        g.reduceClipRegion(juce::Rectangle<int>(20,30,271,106));
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawLine(pivotX+1.0f,pivotY+2.0f,tipX+1.0f,tipY+2.0f,4.0f);
        g.setColour(juce::Colour(0xff251c12));
        g.drawLine(pivotX,pivotY,tipX,tipY,2.0f);
        g.setColour(hw::brass);
        g.drawLine(pivotX,pivotY,tipX,tipY,0.75f);
    }
    if (heldValue>0.0001f) {
        const float ah=(-157.0f+134.0f*normalizedPeak(heldValue))*pi/180.0f;
        const float hx=pivotX+std::cos(ah)*radius;
        const float hy=pivotY+std::sin(ah)*radius;
        g.setColour(juce::Colour(0xff164f30));
        g.fillEllipse(hx-2.6f,hy-2.6f,5.2f,5.2f);
    }

    // Riveted black mechanical dBFS label masks the needle pivot.
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillRoundedRectangle(101,126,108,24,6.0f);
    juce::ColourGradient bronze(hw::brass,103,126,juce::Colour(0xff42321d),207,149,false);
    g.setGradientFill(bronze);
    g.fillRoundedRectangle(106,127,98,22,5.0f);
    g.setColour(juce::Colour(0xff17160f));
    g.fillRoundedRectangle(109,129,92,18,4.0f);
    font(g,12,true);g.setColour(hw::gold);
    text(g,juce::String(peakDb(value),1)+" dBFS",108,128,94,19);
    hw::screw(g,107,137,0.28f);
    hw::screw(g,203,137,0.28f);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
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
    text(g,juce::String(displayedReduction,1),985,158,130,47);
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

    // Top enamel branding plate (RED on reference -> emerald green here).
    hw::plate(g,{405,7,391,73},true,0x001ab3u,10.0f,0.9f);
    for(float x : {417.0f,784.0f})
        for(float y : {21.0f,66.0f}) hw::screw(g,x,y,0.83f);
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
    skelly::ui::drawStarSkull(g,{1080,12,79,69},0.96f,hw::gold);

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
    text(g,"ENGINE STATUS",99,253,150,20,juce::Justification::centredLeft);

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

    // Twin heavy industrial rack handles, specular aged metal, permanent.
    hw::handle(g,15.0f,173.0f,537.0f);
    hw::handle(g,1185.0f,173.0f,537.0f);
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
        staticBackdrop = std::move(updated);
    }
    g.drawImage(staticBackdrop, 0, 0, designW, designH,
                0, 0, rasterW, rasterH);

    drawMeter(g,{275,112,325,159},meterValues[0],peakHoldValues[0],"OUTPUT  /  L");
    drawMeter(g,{612,112,325,159},meterValues[1],peakHoldValues[1],"OUTPUT  /  R");
    drawReduction(g);

    // Read the actual attached state, so DAW automation updates the status.
    const bool bypassed = bypass.getToggleState();
    font(g,14,true);
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
}
