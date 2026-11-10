#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Skelly Master M-01 / aged hardware finish.
// Original procedural artwork, no bitmap mockup. All texture is deterministic;
// called on the GUI thread while building the editor's cached static faceplate.
namespace skelly::hardware {
const juce::Colour brass(0xffb98a50);
const juce::Colour gold(0xfff2d49b);
const juce::Colour parchment(0xffefdcad);
const juce::Colour lacquer(0xff103b29);
const juce::Colour emerald(0xff23734c);
const juce::Colour phosphor(0xffbef65e);
const juce::Colour iron(0xff161710);
const juce::Colour black(0xff080907);

inline uint32_t noise(uint32_t x) noexcept {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15;
    x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}

// Wear is grouped into bright worn-through brass / dark chipped paint.
// Strictly clipped so texture cannot escape a plate into a meter or label.
inline void patina(juce::Graphics& g, juce::Rectangle<float> area, uint32_t seed,
                   float intensity = 1.0f) {
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(area.toNearestInt());
    const int count = juce::jlimit(70, 3000,
          static_cast<int>(area.getWidth() * area.getHeight() / 225.0f * intensity));
    for (int pass=0; pass<2; ++pass) {
        g.setColour(pass == 0 ? juce::Colour(0xffcaa46b).withAlpha(0.12f)
                              : juce::Colour(0xff020302).withAlpha(0.35f));
        for (int i=0; i<count; ++i) {
            const auto h=noise(seed + static_cast<uint32_t>(i*731 + pass*9973));
            const float x=area.getX()+1.0f+(h % 16381u)/16381.0f*(area.getWidth()-2.0f);
            const float y=area.getY()+1.0f+((h>>12) % 12289u)/12289.0f*(area.getHeight()-2.0f);
            const float size=0.65f+static_cast<float>((h>>25)&7u)*0.17f;
            g.fillRect(x,y,size*(i%13==0?2.8f:1.0f),size*0.55f);
        }
    }
    // Soft uneven dark patina blooms break up overly flat digital gradients.
    for (int i=0;i<count/85;++i) {
        const auto h=noise(seed + 0x9381u + static_cast<uint32_t>(i*137));
        const float x=area.getX()+static_cast<float>(h%10007u)/10007.0f*area.getWidth();
        const float y=area.getY()+static_cast<float>((h>>13)%9941u)/9941.0f*area.getHeight();
        const float width=3.0f+static_cast<float>((h>>21)&31u)*0.75f;
        g.setColour(i%2==0?juce::Colours::black.withAlpha(0.12f):
                        juce::Colour(0xffab8550).withAlpha(0.055f));
        g.fillEllipse(x,y,width,width*0.32f);
    }
    // Directional scuffs and small exposed-metal scratches.
    for (int i=0;i<count/15;++i) {
        const auto h=noise(seed+0x43c3u+static_cast<uint32_t>(i*73));
        const float x=area.getX()+static_cast<float>(h%10007u)/10007.0f*area.getWidth();
        const float y=area.getY()+static_cast<float>((h>>13)%9973u)/9973.0f*area.getHeight();
        const float len=2.0f+static_cast<float>((h>>22)&15u)*1.9f;
        g.setColour(juce::Colour(0xfff8dfaf).withAlpha(i%3==0?0.18f:0.065f));
        g.drawLine(x,y,x+len,y-0.6f, i%7==0?1.1f:0.6f);
    }
}

inline void screw(juce::Graphics& g, float x, float y, float scale=1.0f) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(scale,scale,x,y));
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.fillEllipse(x-7.7f,y-7.0f,15.4f,15.4f);
    juce::ColourGradient rim(juce::Colour(0xfff4d99d),x-5.3f,y-5.3f,
                             juce::Colour(0xff4c321d),x+6.0f,y+6.0f,false);
    rim.addColour(0.46,juce::Colour(0xff90704a));
    g.setGradientFill(rim);
    g.fillEllipse(x-5.7f,y-5.7f,11.4f,11.4f);
    g.setColour(juce::Colour(0xff20180e));
    g.drawEllipse(x-4.5f,y-4.5f,9.0f,9.0f,0.8f);
    g.drawLine(x-2.5f,y+2.2f,x+2.5f,y-2.2f,1.8f);
    g.setColour(juce::Colour(0xffe9c47c).withAlpha(0.55f));
    g.drawLine(x-2.2f,y+1.2f,x+1.9f,y-2.6f,0.65f);
}

// Heavy aged iron surround; emerald=true replaces red enamel on reference.
inline void plate(juce::Graphics& g, juce::Rectangle<float> r, bool green=false,
                  uint32_t seed=0x6172u, float radius=9.0f, float wear=1.0f) {
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRoundedRectangle(r.translated(0,3.5f),radius);
    juce::ColourGradient enamel(green?juce::Colour(0xff35764e):juce::Colour(0xff363426),
                r.getX(),r.getY(),green?juce::Colour(0xff071d14):juce::Colour(0xff11120e),
                r.getX()+r.getWidth()*0.8f,r.getBottom(),false);
    enamel.addColour(0.27,green?juce::Colour(0xff215b3c):juce::Colour(0xff26251b));
    enamel.addColour(0.56,green?juce::Colour(0xff133924):juce::Colour(0xff1b1c15));
    g.setGradientFill(enamel);
    g.fillRoundedRectangle(r,radius);
    patina(g,r.reduced(2.2f),seed,wear);
    g.setColour(juce::Colour(0xffe9c995).withAlpha(0.46f));
    g.drawRoundedRectangle(r.reduced(0.8f),radius,1.3f);
    g.setColour(juce::Colour(0xff0b0906));
    g.drawRoundedRectangle(r.reduced(3.3f),juce::jmax(0.0f,radius-2.2f),2.3f);
    g.setColour(juce::Colour(0xffb98a50).withAlpha(0.50f));
    g.drawRoundedRectangle(r.reduced(5.8f),juce::jmax(0.0f,radius-4.0f),0.8f);
    // Irregular flaked enamel near the panel borders, with warm exposed metal
    // beneath the black or green paint. Keep chips behind the inset trim.
    const int edgeMarks=juce::jlimit(8,130,static_cast<int>(r.getWidth()/11.0f));
    for (int i=0;i<edgeMarks;++i) {
        const auto h=noise(seed+static_cast<uint32_t>(i*463+0x72e3u));
        const float t=static_cast<float>(h%10001u)/10001.0f;
        const bool upper=(i%3==0);
        const float px=r.getX()+8.0f+t*(r.getWidth()-17.0f);
        const float py=upper?r.getY()+7.0f:r.getBottom()-7.0f;
        const float len=2.0f+static_cast<float>((h>>19)&31u)*0.62f;
        const float depth=0.7f+static_cast<float>((h>>25)&7u)*0.55f;
        g.setColour(i%5==0?juce::Colour(0xffe9c58b).withAlpha(0.52f):
                            juce::Colour(0xff937147).withAlpha(0.27f));
        g.fillRect(px,py,len,depth);
    }
    // Exposed brass along long chipped edges.
    for (int i=0; i<static_cast<int>(r.getWidth()/27.0f); ++i) {
        auto h=noise(seed+static_cast<uint32_t>(i*173));
        const float x=r.getX()+9.0f+i*25.0f;
        const float len=1.5f+(h%47u)*0.15f;
        g.setColour(gold.withAlpha(0.15f+static_cast<float>(h%7u)*0.024f));
        g.drawLine(x,r.getBottom()-2.2f,x+len,r.getBottom()-2.2f,0.7f);
    }
}

inline void handle(juce::Graphics& g, float x, float top, float bottom) {
    const float h=bottom-top;
    g.setColour(juce::Colour(0xff030504));
    g.fillRoundedRectangle(x-10.0f,top-3.0f,20.0f,h+6.0f,10.0f);
    juce::ColourGradient steel(juce::Colour(0xfff0e5d1),x-7,top,
                               juce::Colour(0xff433c32),x+8,top,false);
    steel.addColour(0.24,juce::Colour(0xff998e81));
    steel.addColour(0.55,juce::Colour(0xff342f29));
    g.setGradientFill(steel);
    g.fillRoundedRectangle(x-6.0f,top,12.0f,h,6.0f);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawLine(x-3.4f,top+18,x-3.4f,bottom-18,1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(x+4.0f,top+12,x+4.0f,bottom-12,1.2f);
    screw(g,x,top+7,0.63f);
    screw(g,x,bottom-7,0.63f);
}

inline void divider(juce::Graphics& g, float x1,float y1,float x2,float y2) {
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawLine(x1+1,y1+1,x2+1,y2+1,2.0f);
    g.setColour(brass.withAlpha(0.52f));
    g.drawLine(x1,y1,x2,y2,1.0f);
}

} // namespace skelly::hardware
