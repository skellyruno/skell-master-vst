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

// SkellCore selects skins, SKELL Master renders coherent physical materials.
juce::Colour hardwareAccent(skellcore::ui::SkullSkin skin) {
    using Skin=skellcore::ui::SkullSkin;
    switch(skin) {
        case Skin::Neon: return juce::Colour(0xff9aff5c);
        case Skin::Ivory: return juce::Colour(0xffe6ce9e);
        case Skin::Obsidian: return juce::Colour(0xffb1c7d0);
        default: return juce::Colour(0xffe1bd7c);
    }
}
juce::Colour hardwareText(skellcore::ui::SkullSkin skin) {
    using Skin=skellcore::ui::SkullSkin;
    switch(skin) {
        case Skin::Neon: return juce::Colour(0xfff0fff0);
        case Skin::Ivory: return juce::Colour(0xfffcf3df);
        case Skin::Obsidian: return juce::Colour(0xffe9f2f4);
        default: return juce::Colour(0xfff6ebd6);
    }
}
juce::Colour hardwareEnamel(skellcore::ui::SkullSkin skin) {
    using Skin=skellcore::ui::SkullSkin;
    switch(skin) {
        case Skin::Neon: return juce::Colour(0xff4baf50);
        case Skin::Ivory: return juce::Colour(0xff977950);
        case Skin::Obsidian: return juce::Colour(0xff46616a);
        default: return juce::Colour(0xff31744b);
    }
}

constexpr const char* labels[11] = {
    "INPUT", "DRIVE", "GLUE", "DYNAMICS", "LOWS", "HIGHS",
    "SAUCE", "TRANSIENTS", "OUTPUT", "CEILING", "CLIP"
};
constexpr const char* ids[11] = {
    "input", "drive", "glue", "dynamics", "lows", "highs",
    "sauce", "transients", "output", "ceiling", "clipper"
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
    activeSkin=selected;
    const auto accent=hardwareAccent(selected), lettering=hardwareText(selected);
    setColour(juce::Slider::textBoxTextColourId,lettering);
    setColour(juce::Slider::textBoxOutlineColourId,accent);
    setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff060806));
    setColour(juce::Slider::textBoxHighlightColourId,accent.withAlpha(0.65f));
    setColour(juce::Slider::thumbColourId,accent);
    setColour(juce::Label::textColourId,lettering);
    setColour(juce::TextButton::textColourOffId,lettering);
    setColour(juce::TextButton::textColourOnId,lettering);
    setColour(juce::TextButton::buttonColourId,juce::Colour(0xff1b1e19));
    setColour(juce::TextButton::buttonOnColourId,accent.darker(0.70f));
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawLabel(
    juce::Graphics& g, juce::Label& label) {
    const auto r=label.getLocalBounds().toFloat().reduced(0.65f);
    // Readout is a recessed glass lens with a manufactured metal rim.
    hw::glassWindow(g,r,4.5f);
    if(r.getWidth()>75.0f) {
        hw::screw(g,r.getX()+8.0f,r.getCentreY(),0.59f);
        hw::screw(g,r.getRight()-8.0f,r.getCentreY(),0.59f);
    }
    if (!label.isBeingEdited()) {
        g.setFont(getLabelFont(label));
        g.setColour(findColour(juce::Label::textColourId));
        const int inset=r.getWidth()>75.0f?13:4;
        g.drawFittedText(label.getText(), label.getLocalBounds().reduced(inset,2),
                         label.getJustificationType(), 1);
    }
}

juce::Font SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::getLabelFont(juce::Label& label) {
    const auto size = juce::jlimit(10.0f, 22.0f,
                    static_cast<float>(label.getHeight()) * 0.80f);
    // Use crisp, un-slashed numerals: Consolas/DejaVu Sans Mono render
    // their zero like an Ø, which is easy to mistake for another glyph
    // in a compact rack display. JUCE centres values in fixed-size plates.
   #if JUCE_LINUX
    constexpr const char* family = "DejaVu Sans";
   #else
    constexpr const char* family = "Segoe UI";
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
    g.setColour(active?hardwareAccent(activeSkin):hardwareAccent(activeSkin).withAlpha(0.54f));
    g.drawRoundedRectangle(inner,6.8f,1.1f);
    g.setColour(juce::Colours::white.withAlpha(hover?0.22f:0.10f));
    g.drawLine(inner.getX()+8,inner.getY()+2,inner.getRight()-8,inner.getY()+2,0.9f);
    // Small pushbuttons use a clean milled bezel, not unreadable micro-rivets.
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawButtonText(
    juce::Graphics& g, juce::TextButton& b, bool, bool) {
    const float size=juce::jlimit(9.0f,17.0f,
                                  static_cast<float>(b.getHeight())*0.43f);
    font(g,size,true);
    g.setColour(findColour(juce::TextButton::textColourOffId));
    // Compact 620px editor leaves only ~30px for the left mini-buttons.
    // A fixed 12px inset previously crushed their text into a tiny sliver.
    const int sidePad=b.getWidth()<45?2:b.getWidth()<95?6:12;
    g.drawFittedText(b.getButtonText(),b.getLocalBounds().reduced(sidePad,3),
                     juce::Justification::centred,1);
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height,
    float position, float start, float end, juce::Slider& slider) {
    if (width < 16 || height < 16) return;
    const float cx=x+width*0.5f, cy=y+height*0.5f;
    const float r=std::min(static_cast<float>(width),
                           static_cast<float>(height))*0.431f;
    const float a0=start+0.04f, a1=end-0.04f;
    float p=std::isfinite(position)?juce::jlimit(0.0f,1.0f,position):0.5f;
    // The asymmetric output range has a centred physical zero.
    if (slider.getComponentID()=="utility-output") {
        const float v=static_cast<float>(slider.getValue());
        p=v<=0.0f?0.5f*juce::jlimit(0.0f,1.0f,(v+18.0f)/18.0f)
                  :0.5f+0.5f*juce::jlimit(0.0f,1.0f,v/12.0f);
    }
    const float angle=a0+p*(a1-a0);
    const auto metalAccent=findColour(juce::Slider::thumbColourId);
    const auto brass=juce::Colour(0xffa98551);
    const auto bright=juce::Colour(0xffe4c68e);
    const auto shadow=juce::Colour(0xff080908);
    const float frame=r*0.90f;
    const float grip=r*0.835f;
    const float face=r*0.702f;

    // Fixed engraved degree scale, with dark cut grooves and light catches.
    // Unlike a flattened photograph, neither the ring nor its lighting rotates.
    for(int tick=0;tick<=32;++tick) {
        const float a=a0+(a1-a0)*static_cast<float>(tick)/32.0f;
        const float sn=std::sin(a),cs=-std::cos(a);
        const bool major=(tick%4)==0;
        const float inner=r*(major?0.936f:0.992f);
        const float outer=r*(major?1.15f:1.087f);
        g.setColour(shadow.withAlpha(0.78f));
        g.drawLine(cx+sn*inner+0.7f,cy+cs*inner+0.9f,
                   cx+sn*outer+0.7f,cy+cs*outer+0.9f,major?2.0f:1.25f);
        g.setColour(metalAccent.withAlpha(major?0.87f:0.46f));
        g.drawLine(cx+sn*inner,cy+cs*inner,cx+sn*outer,cy+cs*outer,
                   major?1.27f:0.76f);
    }

    // Real stacked physical layers: recessed chassis socket, machined brass
    // retaining collar, protruding cylindrical sidewall and a raised top.
    // All centres and light sources are fixed; the pointer alone rotates.
    const float lift=juce::jmax(1.3f,r*0.12f);
    g.setColour(shadow.withAlpha(0.50f));
    g.fillEllipse(cx-frame-4.4f,cy-frame+5.2f,2*(frame+4.4f),2*(frame+4.4f));
    g.setColour(juce::Colour(0xff020302));
    g.fillEllipse(cx-frame-2.3f,cy-frame-2.3f,2*(frame+2.3f),2*(frame+2.3f));

    // Stationary concave retaining ring: lit from top left, dark underneath.
    juce::ColourGradient machinedRim(juce::Colour(0xffc4c4b6),
                                     cx-frame*0.82f,cy-frame*0.95f,
                                     juce::Colour(0xff242725),
                                     cx+frame*0.85f,cy+frame*0.95f,false);
    machinedRim.addColour(0.22,juce::Colour(0xff8c938b));
    machinedRim.addColour(0.57,juce::Colour(0xff454c46));
    g.setGradientFill(machinedRim);
    g.fillEllipse(cx-frame,cy-frame,2*frame,2*frame);
    g.setColour(juce::Colour(0xff0a0a08));
    g.fillEllipse(cx-frame+2.6f,cy-frame+2.6f,
                  2*(frame-2.6f),2*(frame-2.6f));
    g.setColour(bright.withAlpha(0.40f));
    g.drawEllipse(cx-frame+0.9f,cy-frame+0.9f,
                  2*(frame-0.9f),2*(frame-0.9f),0.85f);

    // Offset lower silhouette is the visible thickness of the grip barrel,
    // not a rotating texture or an independent off-axis knob centre.
    g.setColour(shadow.withAlpha(0.92f));
    g.fillEllipse(cx-grip-1.8f,cy-grip+lift+1.0f,
                  2*(grip+1.8f),2*(grip+1.8f));
    juce::ColourGradient sidewall(juce::Colour(0xffbbbdb2),
                                  cx-grip*0.85f,cy-grip*0.8f,
                                  juce::Colour(0xff111513),
                                  cx+grip,cy+grip,false);
    sidewall.addColour(0.29,juce::Colour(0xff5f675f));
    sidewall.addColour(0.70,juce::Colour(0xff262d29));
    g.setGradientFill(sidewall);
    g.fillEllipse(cx-grip,cy-grip,2*grip,2*grip);
    g.setColour(bright.withAlpha(0.29f));
    g.drawEllipse(cx-grip+0.8f,cy-grip+0.8f,
                  2*(grip-0.8f),2*(grip-0.8f),0.85f);

    // Machining ridges have shadowed grooves and tiny illuminated flanks.
    // Their count scales with circumference to prevent a fuzzy gear effect.
    const int ridges=juce::jlimit(52,78,juce::roundToInt(2.0f*pi*grip/3.3f));
    for(int tooth=0;tooth<ridges;++tooth) {
        const float a=2.0f*pi*static_cast<float>(tooth)/static_cast<float>(ridges);
        const float sn=std::sin(a),cs=std::cos(a);
        const float d0=grip*0.845f,d1=grip*0.982f;
        const float exposure=juce::jmax(0.0f,-0.64f*sn-0.77f*cs);
        g.setColour(juce::Colour(0xff050605).withAlpha(0.73f));
        g.drawLine(cx+sn*d0,cy+cs*d0,cx+sn*d1,cy+cs*d1,1.20f);
        const float next=a+0.019f;
        g.setColour(juce::Colour(0xffd6ddd4).withAlpha(0.12f+0.51f*exposure));
        g.drawLine(cx+std::sin(next)*d0,cy+std::cos(next)*d0,
                   cx+std::sin(next)*d1,cy+std::cos(next)*d1,0.72f);
    }

    // Deep contact shadow and a stepped metal land separate cap from grip.
    g.setColour(shadow.withAlpha(0.90f));
    g.fillEllipse(cx-face-2.4f,cy-face-1.8f,
                  2*(face+2.4f),2*(face+2.4f));
    juce::ColourGradient capBezel(juce::Colour(0xffd2d3c8),
                                  cx-face,cy-face,
                                  juce::Colour(0xff191e1c),
                                  cx+face,cy+face,false);
    capBezel.addColour(0.36,juce::Colour(0xff727c75));
    g.setGradientFill(capBezel);
    g.fillEllipse(cx-face-0.7f,cy-face-0.7f,
                  2*(face+0.7f),2*(face+0.7f));

    // Matte anodised top, one fixed lighting direction and fine lathe cuts.
    // No new scratches or noise: depth comes from constructed geometry.
    juce::ColourGradient crown(juce::Colour(0xff9a9e96),
                                cx-face*0.91f,cy-face*0.93f,
                                juce::Colour(0xff171d1a),
                                cx+face*0.89f,cy+face*0.94f,false);
    crown.addColour(0.22,juce::Colour(0xff535c55));
    crown.addColour(0.60,juce::Colour(0xff2c342f));
    g.setGradientFill(crown);
    g.fillEllipse(cx-face+1.0f,cy-face+1.0f,
                  2*(face-1.0f),2*(face-1.0f));
    // A broad stationary grazing highlight gives the solid knob cap weight.
    juce::ColourGradient capLustre(
        juce::Colours::white.withAlpha(0.105f),
        cx-face*0.60f,cy-face*0.64f,
        juce::Colours::white.withAlpha(0.0f),
        cx+face*0.48f,cy+face*0.62f,false);
    g.setGradientFill(capLustre);
    g.fillEllipse(cx-face+2.0f,cy-face+2.0f,
                  2*(face-2.0f),2*(face-2.0f));
    for(int ring=0;ring<2;++ring) {
        const float rr=face*(0.47f+ring*0.29f);
        g.setColour((ring%2==0?juce::Colours::black:juce::Colours::white)
                    .withAlpha(ring%2==0?0.052f:0.021f));
        g.drawEllipse(cx-rr,cy-rr,rr*2,rr*2,0.48f);
    }
    // Small continuous machined arcs read as directional light, not decals.
    const auto arcAt=[&](float radius,float begin,float finish,
                         float lineWidth,juce::Colour colour) {
        juce::Path arc;
        arc.addCentredArc(cx,cy,radius,radius,0.0f,begin,finish,true);
        g.setColour(colour);
        g.strokePath(arc,juce::PathStrokeType(lineWidth));
    };
    arcAt(face-1.55f,-1.45f,-0.22f,1.15f,
          juce::Colour(0xffe4d6b8).withAlpha(0.34f));
    arcAt(face-2.2f,0.67f,2.54f,1.3f,
          juce::Colours::black.withAlpha(0.43f));
    arcAt(grip-0.9f,-1.47f,-0.17f,0.70f,
          juce::Colour(0xffeed5aa).withAlpha(0.27f));

    // Only the physical ivory pointer moves. Body/knurling/shadows stay
    // centred and the pointer follows the DAW-automated parameter exactly.
    const float sn=std::sin(angle),cs=-std::cos(angle);
    const float d0=face*0.25f,d1=face*0.84f;
    const float nx=-cs,ny=sn;
    juce::Path tip;
    tip.startNewSubPath(cx+sn*d0+nx*4.6f,cy+cs*d0+ny*4.6f);
    tip.lineTo(cx+sn*d1+nx*2.8f,cy+cs*d1+ny*2.8f);
    tip.lineTo(cx+sn*(d1+2.2f),cy+cs*(d1+2.2f));
    tip.lineTo(cx+sn*d1-nx*2.8f,cy+cs*d1-ny*2.8f);
    tip.lineTo(cx+sn*d0-nx*4.6f,cy+cs*d0-ny*4.6f);
    tip.closeSubPath();
    g.setColour(shadow.withAlpha(0.82f));
    g.fillPath(tip,juce::AffineTransform::translation(1.8f,2.1f));
    // Pointer material rotates, but studio lighting does not.
    juce::ColourGradient pointer(juce::Colour(0xfffff5d9),
                                 cx-face*0.46f,cy-face*0.49f,
                                 juce::Colour(0xff817f74),
                                 cx+face*0.63f,cy+face*0.66f,false);
    pointer.addColour(0.48,juce::Colour(0xffcbc9b5));
    g.setGradientFill(pointer);
    g.fillPath(tip);
    g.setColour(juce::Colours::white.withAlpha(0.47f));
    g.drawLine(cx+sn*d0+nx*1.3f,cy+cs*d0+ny*1.3f,
               cx+sn*d1+nx*1.3f,cy+cs*d1+ny*1.3f,0.65f);
    // Small concentric rivet/fastening point, not a lopsided dark decal.
    g.setColour(juce::Colour(0xff090a08));
    g.fillEllipse(cx-2.6f,cy-2.6f,5.2f,5.2f);
    g.setColour(brass.withAlpha(0.67f));
    g.drawEllipse(cx-2.25f,cy-2.25f,4.5f,4.5f,0.8f);
}

SkellyMasterAudioProcessorEditor::SkellyMasterAudioProcessorEditor(SkellyMasterAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p) {
    skin=loadSkin();
    lookAndFeel.setSkin(skin);
    setLookAndFeel(&lookAndFeel);
    skullHitTarget.setComponentID("skellcore-theme-skull");
    skullHitTarget.onClick=[this] { switchSkin(); };
    addAndMakeVisible(skullHitTarget);

    for (int i = 0; i < knobCount; ++i) {
        auto& s = knobs[static_cast<size_t>(i)];
        s.setComponentID(i==8 ? "utility-output" : i==9 ? "utility-ceiling" : i==10 ? "utility-clipper" : "primary");
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, i >= 8 ? 85 : 87, 27);
        s.setRotaryParameters(pi * 1.25f, pi * 2.75f, true);
        s.setMouseDragSensitivity(190);
        s.setVelocityBasedMode(false);
        s.setDoubleClickReturnValue(true, 0.0);
        s.setNumDecimalPlacesToDisplay(0);
        const bool db = i == 0 || i == 4 || i == 5 || i == 8 || i == 9;
        s.setTextValueSuffix(i==9 ? "" : db ? " dB" : " %");
        const juce::String sub = i < 8 ? subtitles[i] : (i == 8 ? "OUTPUT TRIM" : i == 9 ? "PEAK CEILING" : "SOFT CLIP AMOUNT");
         s.setTooltip(i==9
            ? "CEILING ADJUST: 0 = -1 dBFS limiting; -3 = -3 dBFS, +3 = -0.1 dBFS. Drag, type, or double-click to reset."
            : i==10 ? "CLIP: 0% = transparent lookahead limiting; higher settings blend oversampled soft clipping before the limiter."
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

    // These are genuine monitoring controls; none changes an exported mix.
    monitorButtons[0].setTooltip("PEAK: show stereo sample-peak hold in dBFS.");
    monitorButtons[1].setTooltip("TRUE: show 4x reconstructed output peaks in dBTP.");
    monitorButtons[2].setTooltip("CLEAR: reset displayed sample-peak and true-peak holds; audio stays untouched.");
    monitorButtons[0].setClickingTogglesState(true);
    monitorButtons[1].setClickingTogglesState(true);
    monitorButtons[0].setRadioGroupId(2301);
    monitorButtons[1].setRadioGroupId(2301);
    monitorButtons[1].setToggleState(true,juce::dontSendNotification);
    monitorButtons[0].onClick=[this] { monitorTruePeak=false; repaint(); };
    monitorButtons[1].onClick=[this] { monitorTruePeak=true; repaint(); };
    monitorButtons[2].onClick=[this] { clearMeterHolds(); };
    for(auto& button : monitorButtons)
        addAndMakeVisible(button);

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
    // Only brass and enamel are tinted. Dark iron, texture highlights and
    // glass remain physical, instead of flattening into a solid skin color.
    const auto metal=hardwareAccent(skin), enamel=hardwareEnamel(skin);
    juce::Image::BitmapData pixels(result,juce::Image::BitmapData::readWrite);
    for(int y=0;y<pixels.height;++y)
        for(int x=0;x<pixels.width;++x) {
            const auto c=pixels.getPixelColour(x,y);
            if(c.getAlpha()==0) continue;
            const float red=c.getFloatRed(),green=c.getFloatGreen(),
                        blue=c.getFloatBlue();
            const bool brass=red>green*1.055f && green>blue*1.08f
                             && red>0.18f;
            const bool greenEnamel=green>red*1.17f && green>blue*1.10f
                                   && green>0.095f;
            if(!brass && !greenEnamel) continue;
            const float sourceLuma=0.24f*red+0.66f*green+0.10f*blue;
            const float shading=juce::jlimit(0.20f,1.0f,sourceLuma*1.58f);
            const auto target=(brass?metal:enamel)
                                 .withMultipliedBrightness(shading);
            pixels.setPixelColour(x,y,c.interpolatedWith(target,0.68f));
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

void SkellyMasterAudioProcessorEditor::clearMeterHolds() {
    meterValues.fill(0.0f);
    peakHoldValues.fill(0.0f);
    peakHoldTicks.fill(0);
    displayedTruePeak=0.0f;
    lastPaintedTruePeak=0.0f;
    repaint();
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
    displayedTruePeak = juce::jmax(processor.consumeTruePeak(), displayedTruePeak * 0.90f);
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
    // The live glass display changes only on meaningful metering movement.
    if(std::abs(displayedTruePeak-lastPaintedTruePeak) >= 0.002f) {
        lastPaintedTruePeak=displayedTruePeak;
        dirty(64,245,185,35);
    }
    if (bypassed != lastDisplayedBypass) {
        lastDisplayedBypass = bypassed;
        dirty(173, 251, 83, 22);
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
        for(float yy : {12.0f,144.0f}) hw::screw(g,xx,yy,0.89f);
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
    {
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
    hw::screw(g,107,137,0.53f);
    hw::screw(g,203,137,0.53f);
    // Glass curvature sits IN FRONT of the mechanical needle and scale.
    // It never draws a second permanent needle or fake audio indication.
    juce::ColourGradient lensLight(
        juce::Colours::white.withAlpha(0.13f),34.0f,30.0f,
        juce::Colours::white.withAlpha(0.0f),115.0f,77.0f,false);
    g.setGradientFill(lensLight);
    g.fillRoundedRectangle(27.0f,31.0f,246.0f,26.0f,13.0f);
    g.setColour(juce::Colours::white.withAlpha(0.20f));
    g.drawLine(42.0f,29.5f,268.0f,29.5f,0.8f);
    g.setColour(juce::Colours::black.withAlpha(0.44f));
    g.drawRoundedRectangle(juce::Rectangle<float>(18.3f,23.2f,273.4f,119.7f),
                           27.0f,2.3f);
}

void SkellyMasterAudioProcessorEditor::drawReduction(juce::Graphics& g) const {
    // Hardware GR module: the electronic display and LED ladder are
    // physically DARK at idle. No ghost 0, dB text, or grey inactive bars.
    // The label and recessed unlit glass stay visible as physical hardware.
    hw::plate(g,{953,108,216,170},false,0x300ad5u,9.0f,0.34f);
    for(float xx : {965.0f,1157.0f})
        for(float yy : {120.0f,265.0f}) hw::screw(g,xx,yy,0.85f);
    font(g,12,true);g.setColour(hw::parchment);
    text(g,"GAIN REDUCTION",969,125,185,17);

    // Deep lens with a believable glass cover; light stays off at idle.
    hw::glassWindow(g,{975,152,175,57},9.0f);

    const bool active=std::isfinite(displayedReduction)
                   && displayedReduction>=0.5f;
    // Recessed light-pipe holder stays dark when no compressor/limiter GR.
    hw::glassWindow(g,{972,225,176,24},3.1f);
    if (!active) return;

    font(g,37,true);
    g.setColour(hw::phosphor);
    text(g,juce::String(juce::roundToInt(displayedReduction)),985,158,130,47);
    font(g,12,true);
    g.setColour(hw::gold);
    text(g,"dB",1117,178,27,14);
    const int lit=juce::jlimit(1,16,juce::roundToInt(
        displayedReduction*(16.0f/18.0f)));
    for(int i=0;i<lit;++i) {
        const float xx=977.0f+i*10.4f;
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(xx+0.7f,230.2f,7.3f,15.3f,1.9f);
        g.setColour(i>12?juce::Colour(0xffefb653):hw::phosphor);
        g.fillRoundedRectangle(xx,229.1f,6.9f,16.0f,2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.drawLine(xx+1.1f,231.0f,xx+1.1f,242.0f,0.55f);
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
    // Mastering hardware uses the new machined medallion consistently.
    // One selectable left logo, no old star-eyes and no right skull.
    skelly::ui::drawMachinedSkull(g,{57,10,82,72},0.98f,hw::gold);
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
    // BYPASS and RESET are real interactive controls in the header.
    // No baked labels or permanent graphics beneath their hitboxes.
    // The skull on the right was removed; left skull alone changes skin.

    // Engraved meter bridge with a separate stamped status panel.
    hw::plate(g,{37,94,1126,200},false,0x33c8a7u,12.0f,0.72f);
    hw::plate(g,{48,109,213,176},false,0x641b4fu,9.0f,0.52f);
    for(float x : {60.0f,249.0f})
        for(float y : {121.0f,273.0f}) hw::screw(g,x,y,0.88f);
    font(g,18,true);g.setColour(hw::gold);
    text(g,"SKELLY MASTER",65,125,182,26);
    font(g,12,true);g.setColour(hw::parchment);
    text(g,"PRECISION MASTER BUS / M-01",61,154,189,20);
    font(g,11,true);g.setColour(hw::parchment);
    text(g,"4x OS  /  STEREO LINK  /  LIMIT",60,175,190,15);
    hw::divider(g,63,191,247,191);
    font(g,13,true);g.setColour(hw::gold);
    text(g,"OUTPUT MONITOR",65,193,178,18);
    // The dedicated monitor is passive: no audio processing changes.
    // Leave a visible clearance around the lower panel mounting screws.
    hw::glassWindow(g,{70,247,169,30},5.0f);

    // Modular lower rack: same genuine APVTS controls as previous build.
    hw::plate(g,{37,298,1126,356},false,0x8b113au,12.0f,1.0f);
    for(float x : {51.0f,1149.0f})
        for(float y : {310.0f,641.0f}) hw::screw(g,x,y,0.90f);
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
        for(float y : {685.0f,741.0f}) hw::screw(g,x,y,0.90f);
    hw::divider(g,267,685,267,744);
    hw::divider(g,553,685,553,744);
    hw::divider(g,850,685,850,744);
    hw::divider(g,1121,685,1121,744);
    font(g,16,true);g.setColour(hw::gold);
    text(g,"// MASTER BUS",48,696,205,25,juce::Justification::centredLeft);
    font(g,14,true);g.setColour(hw::parchment);
    text(g,"TONE IS EVERYTHING",66,724,184,18,juce::Justification::centredLeft);
    font(g,14,true);g.setColour(hw::gold);
    text(g,"CLIP",294,710,85,21,juce::Justification::centredLeft);
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

    // Readout is separate from artwork and continuously uses actual DSP data.
    // -80 dB is a visual noise-floor marker rather than fabricated activity.
    const float observed=monitorTruePeak ? displayedTruePeak
        : juce::jmax(peakHoldValues[0],peakHoldValues[1]);
    const float db=juce::Decibels::gainToDecibels(
        juce::jmax(observed,0.0001f),-80.0f);
    font(g,13,true);g.setColour(readoutWhite);
    text(g,juce::String(monitorTruePeak ? "TP " : "SP ")
            + juce::String(db,1) + (monitorTruePeak ? " dBTP" : " dBFS"),
         73,252,163,20,juce::Justification::centred);
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
    place(bypass,887,29,125,38);
    place(reset,1024,29,126,38);
    place(knobs[10],371,685,178,68);
    for(int i=0;i<3;++i)
        place(monitorButtons[static_cast<size_t>(i)],64+i*61,216,57,29);
    place(knobs[8],647,685,199,68);
    place(knobs[9],929,685,181,68);
    place(skullHitTarget,57,10,82,72);
}
