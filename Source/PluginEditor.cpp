#include "PluginEditor.h"
#include <cmath>

namespace {
const juce::Colour green(0xff74ff7a), greenSoft(0xff36c965), greenDim(0xff1c6036);
const juce::Colour bg(0xff080d0c), rail(0xff19221f), white(0xffe3f1e7);
constexpr float pi = juce::MathConstants<float>::pi;
constexpr const char* labels[10] = {
    "INPUT", "DRIVE", "GLUE", "DYNAMICS", "LOWS", "HIGHS", "SAUCE", "TRANSIENTS", "OUTPUT", "CEILING"
};
constexpr const char* ids[10] = {
    "input", "drive", "glue", "dynamics", "lows", "highs", "sauce", "transients", "output", "ceiling"
};
const juce::String detailLabel[10] = {
    "GAIN STAGE", "HARMONIC DRIVE", "BUS COMPRESSION", "LEVEL CONTROL",
    "115 Hz SHELF", "8.2 kHz SHELF", "ANALOG ENHANCE", "ATTACK SHAPE", "POST GAIN", "PEAK LIMIT"
};
float normPeak(float p) { return juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(juce::jmax(p, 0.00001f)) + 48.0f) / 48.0f); }
void type(juce::Graphics& g, float size, bool heavy = false) {
    g.setFont(juce::Font(juce::FontOptions(size).withStyle(heavy ? "Bold" : "Regular")));
}
}

SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::SkellyLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, white);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff141c18));
    setColour(juce::Slider::textBoxHighlightColourId, greenDim);
    setColour(juce::Slider::thumbColourId, green);
    setColour(juce::TextButton::textColourOffId, white);
    setColour(juce::TextButton::textColourOnId, green);
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff151d19));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff214d30));
}

void SkellyMasterAudioProcessorEditor::SkellyLookAndFeel::drawRotarySlider(
    juce::Graphics& g, int x, int y, int width, int height, float pos,
    float start, float end, juce::Slider& slider) {
    const bool bigMachine = slider.getComponentID() == "machine";
    const float size = std::min(static_cast<float>(width), static_cast<float>(height)) * 0.90f;
    const float r = size * 0.5f;
    const float cx = static_cast<float>(x) + width * 0.5f;
    const float cy = static_cast<float>(y) + height * 0.5f;
    const float startAngle = start + 0.10f, endAngle = end - 0.10f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Recessed, soft-neon outer torus with engraved tick marks.
    g.setColour(juce::Colour(0xff060908));
    g.fillEllipse(cx-r-5, cy-r-5, (r+5)*2, (r+5)*2);
    g.setColour(juce::Colour(0xff36453d));
    g.drawEllipse(cx-r-4, cy-r-4, (r+4)*2, (r+4)*2, 1.0f);
    for (int i = 0; i <= 36; ++i) {
        const float a = startAngle + (endAngle - startAngle) * i / 36.0f;
        const float cs = std::sin(a), sn = -std::cos(a);
        const float inner = r * (i % 3 ? 0.89f : 0.83f), outer = r * 0.98f;
        g.setColour(i <= pos * 36.0f ? greenSoft.withAlpha(0.75f) : juce::Colour(0xff45574c));
        g.drawLine(cx + cs*inner, cy + sn*inner, cx+cs*outer, cy+sn*outer,
                   i % 3 == 0 ? 1.3f : 0.75f);
    }
    juce::Path groove;
    groove.addCentredArc(cx, cy, r*0.76f, r*0.76f, 0, startAngle, endAngle, true);
    g.setColour(juce::Colour(0xff1f2925));
    g.strokePath(groove, juce::PathStrokeType(bigMachine ? 9.0f : 5.0f));
    juce::Path active;
    const bool bipolar = slider.getMinimum() < 0 && slider.getMaximum() > 0
                           && slider.getComponentID() == "tone";
    const float zeroPos = bipolar ? static_cast<float>(slider.valueToProportionOfLength(0)) : 0.0f;
    const float anchorAngle = startAngle + zeroPos * (endAngle - startAngle);
    active.addCentredArc(cx, cy, r*0.76f, r*0.76f, 0,
                         std::min(anchorAngle,angle), std::max(anchorAngle,angle), true);
    g.setColour(green.withAlpha(0.12f));
    g.strokePath(active, juce::PathStrokeType(bigMachine ? 13.0f : 8.0f,
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(green);
    g.strokePath(active, juce::PathStrokeType(bigMachine ? 3.2f : 2.3f,
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float faceRadius = r * (bigMachine ? 0.68f : 0.73f);
    g.setColour(juce::Colour(0xff080e0d));
    g.fillEllipse(cx-faceRadius-2, cy-faceRadius-2, (faceRadius+2)*2, (faceRadius+2)*2);
    juce::ColourGradient metal(juce::Colour(0xff50605b), cx-r*0.5f, cy-r*0.62f,
                              juce::Colour(0xff101b18), cx+r*0.58f, cy+r*0.7f, false);
    metal.addColour(0.46, juce::Colour(bigMachine ? 0xff293a34 : 0xff394b47));
    metal.addColour(0.75, juce::Colour(0xff0e1613));
    g.setGradientFill(metal);
    g.fillEllipse(cx-faceRadius, cy-faceRadius, faceRadius*2, faceRadius*2);
    g.setColour(juce::Colour(0xff6d8177).withAlpha(0.45f));
    g.drawEllipse(cx-faceRadius, cy-faceRadius, faceRadius*2, faceRadius*2, 1.15f);
    // Specular light and concentric lathe marks, subtle at default size.
    for (int i = 1; i <= 4; ++i) {
        const float rr = faceRadius * (0.22f + i * 0.16f);
        g.setColour(juce::Colours::white.withAlpha(0.012f + i * 0.003f));
        g.drawEllipse(cx-rr, cy-rr, rr*2, rr*2, 0.55f);
    }
    const float tipRadius = faceRadius * 0.79f;
    const float pointerSin = std::sin(angle), pointerCos = -std::cos(angle);
    g.setColour(green.withAlpha(0.20f));
    g.drawLine(cx,cy,cx+pointerSin*tipRadius,cy+pointerCos*tipRadius,5.0f);
    g.setColour(green);
    g.drawLine(cx+pointerSin*faceRadius*0.35f,cy+pointerCos*faceRadius*0.35f,
               cx+pointerSin*tipRadius,cy+pointerCos*tipRadius,2.2f);
    g.setColour(juce::Colour(0xff06170c));
    g.fillEllipse(cx-5,cy-5,10,10);
    g.setColour(greenSoft);
    g.fillEllipse(cx-2,cy-2,4,4);
}

SkellyMasterAudioProcessorEditor::SkellyMasterAudioProcessorEditor(SkellyMasterAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p) {
    setLookAndFeel(&lookAndFeel);
    for (int i = 0; i < knobCount; ++i) {
        auto& s = knobs[static_cast<size_t>(i)];
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, i >= 8 ? 76 : 92, 21);
        s.setComponentID(i < 4 ? "machine" : "tone");
        s.setRotaryParameters(pi*1.26f, pi*2.74f, true);
        s.setMouseDragSensitivity(180);
        s.setDoubleClickReturnValue(true, i == 9 ? -1.0 : 0.0);
        s.setVelocityBasedMode(false);
        s.setTextValueSuffix(i == 0 || i == 4 || i == 5 || i == 8 || i == 9 ? " dB" : " %");
        s.setTooltip(juce::String(labels[i]) + " • " + detailLabel[i] + " • Double-click to reset");
        addAndMakeVisible(s);
        attachments[static_cast<size_t>(i)] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, ids[i], s);
    }
    bypass.setClickingTogglesState(true);
    bypass.setConnectedEdges(0);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, "bypass", bypass);
    addAndMakeVisible(bypass);
    reset.onClick = [this] { resetToDefaults(); };
    addAndMakeVisible(reset);
    setResizable(true, true);
    setResizeLimits(840, 540, 1680, 1080);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(designW) / designH);
    setSize(896, 576);
    startTimerHz(30);
}

SkellyMasterAudioProcessorEditor::~SkellyMasterAudioProcessorEditor() {
    stopTimer();
    // Detach controls before destruction; attachments are destroyed first.
    for (auto& a : attachments) a.reset();
    bypassAttachment.reset();
    setLookAndFeel(nullptr);
}

void SkellyMasterAudioProcessorEditor::resetToDefaults() {
    for (const auto* id : ids) {
        if (auto* param = processor.parameters.getParameter(id)) {
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->getDefaultValue());
            param->endChangeGesture();
        }
    }
    if (auto* param = processor.parameters.getParameter("bypass")) {
        param->beginChangeGesture();
        param->setValueNotifyingHost(param->getDefaultValue());
        param->endChangeGesture();
    }
}

void SkellyMasterAudioProcessorEditor::timerCallback() {
    const std::array<float,2> fresh { processor.consumePeakLeft(), processor.consumePeakRight() };
    for (int c = 0; c < 2; ++c)
        meterValues[static_cast<size_t>(c)] = juce::jmax(fresh[static_cast<size_t>(c)],
                                                         meterValues[static_cast<size_t>(c)] * 0.90f);
    displayedReduction = juce::jmax(processor.consumeMaxReduction(), displayedReduction * 0.87f);
    repaint();
}

void SkellyMasterAudioProcessorEditor::drawScrew(juce::Graphics& g, float x, float y) const {
    g.setColour(juce::Colour(0xff030605)); g.fillEllipse(x-6,y-6,12,12);
    g.setColour(juce::Colour(0xff4a5a52)); g.drawEllipse(x-5,y-5,10,10,1.2f);
    g.setColour(juce::Colour(0xff6a8374)); g.drawLine(x-2.3f,y+2.3f,x+2.3f,y-2.3f,1.2f);
}

void SkellyMasterAudioProcessorEditor::drawSkullMark(juce::Graphics& g, juce::Rectangle<float> box) const {
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(box.getWidth()/100.0f,box.getHeight()/100.0f)
          .translated(box.getX(),box.getY()));
    juce::Path skull;
    skull.startNewSubPath(22,46);
    skull.cubicTo(14,13,33,7,50,7);
    skull.cubicTo(77,7,87,24,79,49);
    skull.lineTo(70,63); skull.lineTo(69,79);
    skull.quadraticTo(50,91,31,79);
    skull.lineTo(30,62); skull.closeSubPath();
    g.setColour(green.withAlpha(0.90f)); g.fillPath(skull);
    juce::Path eyes;
    eyes.addTriangle(24,43,43,48,35,63);
    eyes.addTriangle(76,43,57,48,65,63);
    g.setColour(bg); g.fillPath(eyes);
    juce::Path nose; nose.addTriangle(50,57,43,71,57,71); g.fillPath(nose);
    for (int i=0;i<4;i++) g.fillRoundedRectangle(39+i*6.5f,76,4.3f,10,1.0f);
}

void SkellyMasterAudioProcessorEditor::drawMeter(juce::Graphics& g, juce::Rectangle<float> r,
                                                  float value, const juce::String& name, float number) const {
    g.setColour(juce::Colour(0xff050907)); g.fillRoundedRectangle(r.expanded(3),8);
    g.setColour(juce::Colour(0xff48544c)); g.drawRoundedRectangle(r.expanded(3),8,1.5f);
    juce::ColourGradient glass(juce::Colour(0xff1c3027),r.getCentreX(),r.getY(),
                              juce::Colour(0xff0b160f),r.getCentreX(),r.getBottom(),false);
    glass.addColour(0.5,juce::Colour(0xff15261a));
    g.setGradientFill(glass); g.fillRoundedRectangle(r,5);
    g.setColour(greenDim.withAlpha(0.30f));
    g.drawRoundedRectangle(r.reduced(6),3,1);
    type(g,12,true); g.setColour(white.withAlpha(.85f));
    g.drawText(name,r.getX()+12,r.getY()+10,r.getWidth()-24,16,juce::Justification::centred);

    const float pivotX = r.getCentreX(), pivotY = r.getY()+145.0f;
    const float radius = 103.0f;
    for(int i=0;i<=12;i++) {
        const float t=static_cast<float>(i)/12.0f;
        const float a=pi*(0.91f-0.82f*t);
        const float x0=pivotX+std::cos(a)*radius, y0=pivotY-std::sin(a)*radius;
        const float x1=pivotX+std::cos(a)*(radius-(i%3?6.5f:12.0f));
        const float y1=pivotY-std::sin(a)*(radius-(i%3?6.5f:12.0f));
        g.setColour(i>=10 ? juce::Colour(0xffb1a958) : greenSoft.withAlpha(0.75f));
        g.drawLine(x0,y0,x1,y1,i%3?0.9f:1.6f);
    }
    const float angle = pi * (0.91f - 0.82f * normPeak(value));
    g.setColour(green.withAlpha(0.12f));
    g.drawLine(pivotX,pivotY,pivotX+std::cos(angle)*radius*0.91f,
               pivotY-std::sin(angle)*radius*0.91f,5.5f);
    g.setColour(green);
    g.drawLine(pivotX,pivotY,pivotX+std::cos(angle)*radius*0.91f,
               pivotY-std::sin(angle)*radius*0.91f,1.8f);
    g.setColour(juce::Colour(0xff53685a)); g.fillEllipse(pivotX-6,pivotY-6,12,12);
    g.setColour(green); g.fillEllipse(pivotX-2,pivotY-2,4,4);
    type(g,10,true); g.setColour(white.withAlpha(.75f));
    g.drawText("-48",r.getX()+19,r.getY()+105,30,14,juce::Justification::centredLeft);
    g.drawText("-24",r.getCentreX()-14,r.getY()+43,30,14,juce::Justification::centred);
    g.drawText("0",r.getRight()-42,r.getY()+105,28,14,juce::Justification::centredRight);
    type(g,12,true); g.setColour(green);
    g.drawText(juce::String(number,1)+" dBFS",r.getX()+6,r.getBottom()-20,r.getWidth()-12,16,
               juce::Justification::centred);
    g.setColour(juce::Colours::white.withAlpha(0.02f));
    g.fillRoundedRectangle(r.getX()+5,r.getY()+3,r.getWidth()-10,18,4);
}

void SkellyMasterAudioProcessorEditor::paint(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState scaleGuard(g);
    g.addTransform(juce::AffineTransform::scale(static_cast<float>(getWidth())/designW,
                                               static_cast<float>(getHeight())/designH));
    juce::ColourGradient background(juce::Colour(0xff17201c),0,0,bg,0,designH,false);
    background.addColour(.38,juce::Colour(0xff0c1210));
    g.setGradientFill(background); g.fillRect(0,0,designW,designH);
    for(int x=0;x<designW;x+=9) {
        g.setColour(juce::Colours::white.withAlpha(.008f));
        g.drawVerticalLine(x,0,static_cast<float>(designH));
    }
    // Top anodized faceplate.
    g.setColour(juce::Colour(0xff131b18)); g.fillRect(0,0,designW,68);
    g.setColour(juce::Colour(0xff35433a)); g.drawHorizontalLine(68,0,designW);
    g.setColour(greenSoft); g.fillRect(0,67,designW,2);
    drawSkullMark(g,{35,10,49,49});
    type(g,29,true); g.setColour(white);
    g.drawText("SKELLY",90,9,220,37,juce::Justification::centredLeft);
    type(g,11,true); g.setColour(greenSoft);
    g.drawText("AUDIO LABS",92,43,140,17,juce::Justification::centredLeft);
    type(g,32,true); g.setColour(white);
    g.drawText("MASTER",420,5,282,47,juce::Justification::centred);
    type(g,10,true); g.setColour(greenSoft);
    g.drawText("STEREO MASTERING SUITE  /  M-01",384,49,355,12,juce::Justification::centred);
    type(g,11,true); g.setColour(white.withAlpha(.7f));
    g.drawText("PRECISION • POWER • CHARACTER",816,28,263,18,juce::Justification::centredRight);

    // Raised rack-mounted meter bridge.
    g.setColour(juce::Colour(0xff121a16)); g.fillRoundedRectangle(28,84,1064,185,9);
    g.setColour(juce::Colour(0xff36473a)); g.drawRoundedRectangle(28,84,1064,185,9,1.4f);
    for (float sy : {100.0f,255.0f}) { drawScrew(g,45,sy); drawScrew(g,1075,sy); }
    type(g,13,true); g.setColour(green);
    g.drawText("S K E L L Y",56,112,165,24,juce::Justification::centred);
    type(g,10,true); g.setColour(white.withAlpha(.68f));
    g.drawText("4× OVERSAMPLED",57,144,165,17,juce::Justification::centred);
    g.drawText("LOOKAHEAD LIMIT",57,166,165,17,juce::Justification::centred);
    g.drawText("STEREO LINKED",57,188,165,17,juce::Justification::centred);
    g.setColour(green); g.fillEllipse(125,222,9,9);
    g.setColour(green.withAlpha(.16f)); g.fillEllipse(121,218,17,17);
    type(g,10,true); g.setColour(greenSoft);
    g.drawText("ENGINE ACTIVE",144,216,105,18,juce::Justification::centredLeft);
    drawMeter(g,{268,99,258,157},meterValues[0],"OUTPUT  /  L",
              juce::jmax(-80.0f,juce::Decibels::gainToDecibels(meterValues[0],-80.0f)));
    drawMeter(g,{546,99,258,157},meterValues[1],"OUTPUT  /  R",
              juce::jmax(-80.0f,juce::Decibels::gainToDecibels(meterValues[1],-80.0f)));
    g.setColour(juce::Colour(0xff0a100c)); g.fillRoundedRectangle(828,100,226,155,7);
    g.setColour(juce::Colour(0xff3a5540)); g.drawRoundedRectangle(828,100,226,155,7,1.1f);
    type(g,12,true); g.setColour(white); g.drawText("GAIN REDUCTION",844,112,192,20,juce::Justification::centred);
    type(g,27,true); g.setColour(green);
    g.drawText(juce::String(displayedReduction,1),844,142,192,47,juce::Justification::centred);
    type(g,11,true); g.setColour(white.withAlpha(.6f)); g.drawText("dB  /  PEAK GR",844,190,192,16,juce::Justification::centred);
    for (int i=0;i<15;i++) {
        g.setColour(i < displayedReduction * (15.0f/18.0f) ?
            (i > 10 ? juce::Colour(0xffe7d66b) : green) : juce::Colour(0xff233329));
        g.fillRoundedRectangle(851+i*12.1f,222,8.6f,15,2);
    }

    // Subtle framing rails between the two banks of reference-style controls.
    g.setColour(juce::Colour(0xff43584a)); g.drawHorizontalLine(282,29,1091);
    g.setColour(greenDim); g.fillRoundedRectangle(27,277,1066,2,1);
    g.setColour(juce::Colour(0xff121915)); g.fillRoundedRectangle(28,294,1064,332,10);
    g.setColour(juce::Colour(0xff2b3b2e)); g.drawRoundedRectangle(28,294,1064,332,10,1.1f);
    g.setColour(juce::Colour(0xff0b110e)); g.fillRoundedRectangle(38,301,1044,155,7);
    g.fillRoundedRectangle(38,462,1044,156,7);
    const float centers[4] {158.0f,427.0f,696.0f,965.0f};
    for (int i=0;i<8;i++) {
        const int row=i/4, col=i%4;
        const float cx=centers[col];
        const float labelY=row?586.0f:424.0f;
        type(g,15,true); g.setColour(white);
        g.drawText(labels[i],cx-105,labelY,210,25,juce::Justification::centred);
        type(g,9,true); g.setColour(greenSoft.withAlpha(.8f));
        g.drawText(detailLabel[i],cx-115,labelY+22,230,15,juce::Justification::centred);
        if(col<3) {
            g.setColour(juce::Colour(0xff27352c));
            g.drawVerticalLine(static_cast<int>((centers[col]+centers[col+1])/2),
                               row?481.0f:313.0f,row?602.0f:445.0f);
        }
    }

    // Footer: compact service controls and rack markings.
    g.setColour(juce::Colour(0xff161f1a)); g.fillRect(0,634,designW,86);
    g.setColour(juce::Colour(0xff405345)); g.drawHorizontalLine(634,0,designW);
    drawScrew(g,25,672); drawScrew(g,1092,672);
    type(g,11,true); g.setColour(greenSoft);
    g.drawText("MASTER BUS",50,657,135,19,juce::Justification::centredLeft);
    type(g,10); g.setColour(white.withAlpha(.55f));
    g.drawText("TONE IS EVERYTHING",50,679,215,18,juce::Justification::centredLeft);
    type(g,11,true); g.setColour(white);
    g.drawText("OUTPUT",475,660,85,15,juce::Justification::centredLeft);
    g.drawText("CEILING",680,660,86,15,juce::Justification::centredLeft);
    type(g,10); g.setColour(greenSoft);
    g.drawText("HQ 4× / LOOKAHEAD",890,660,178,16,juce::Justification::centredRight);
}

void SkellyMasterAudioProcessorEditor::resized() {
    const float sx=static_cast<float>(getWidth())/designW;
    const float sy=static_cast<float>(getHeight())/designH;
    const auto place = [sx,sy](juce::Component& c,int x,int y,int w,int h) {
        c.setBounds(juce::roundToInt(x*sx),juce::roundToInt(y*sy),
                    juce::roundToInt(w*sx),juce::roundToInt(h*sy));
    };
    const int cx[4] {158,427,696,965};
    for (int i=0;i<8;i++) {
        const int row=i/4;
        // Text box is part of the Slider, separate silk-screen labels in paint().
        place(knobs[static_cast<size_t>(i)],cx[i%4]-72,row ? 469 : 307,144,116);
    }
    place(knobs[8],550,645,118,70);
    place(knobs[9],762,645,118,70);
    place(bypass,292,658,106,35);
    place(reset,383,658,76,35);
}
