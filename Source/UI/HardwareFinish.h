#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
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
const juce::Colour iron(0xff10120e);
const juce::Colour black(0xff050605);

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
    const int count = juce::jlimit(70, 3400,
          static_cast<int>(area.getWidth() * area.getHeight() / 195.0f * intensity));
    for (int pass=0; pass<2; ++pass) {
        g.setColour(pass == 0 ? juce::Colour(0xffb5a07b).withAlpha(0.15f)
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
        g.setColour(juce::Colour(0xffd6c4a0).withAlpha(i%3==0?0.22f:0.075f));
        g.drawLine(x,y,x+len,y-0.6f, i%7==0?1.1f:0.6f);
    }
}

// Small metallic cross-head fastener. Deep countersink and four machined
// recess arms remain truly centred. No fake painted dots at panel corners.
inline void screw(juce::Graphics& g,float x,float y,float scale=1.0f) {
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(scale,scale,x,y));
    g.setColour(juce::Colours::black.withAlpha(0.76f));
    g.fillEllipse(x-5.5f,y-5.0f,11.0f,11.0f);
    juce::ColourGradient socket(juce::Colour(0xff0a0b0a),x-5,y-5,
                                 juce::Colour(0xff756f62),x+5,y+5,false);
    g.setGradientFill(socket);
    g.fillEllipse(x-4.85f,y-4.85f,9.7f,9.7f);
    juce::ColourGradient steel(juce::Colour(0xfffbf7e9),x-3.7f,y-4.1f,
                                juce::Colour(0xff454842),x+3.8f,y+4.3f,false);
    steel.addColour(0.31,juce::Colour(0xffbec2ba));
    steel.addColour(0.68,juce::Colour(0xff777b75));
    g.setGradientFill(steel);
    g.fillEllipse(x-3.78f,y-3.78f,7.56f,7.56f);
    // A crisply cut X, with near-black recessed floors and a thin edge
    // reflecting the fixed top-left light. Hardware-size, not a giant rivet.
    g.setColour(juce::Colour(0xff171c1a));
    g.drawLine(x-2.05f,y-2.05f,x+2.05f,y+2.05f,1.35f);
    g.drawLine(x-2.05f,y+2.05f,x+2.05f,y-2.05f,1.35f);
    g.setColour(juce::Colour(0xfff3f2e4).withAlpha(0.72f));
    g.drawLine(x-2.17f,y-2.72f,x+1.70f,y+1.15f,0.43f);
    g.drawLine(x-2.18f,y+1.54f,x+1.25f,y-1.89f,0.43f);
}

// Recessed glossy hardware display housing. Draw before its live text or
// LEDs, so reflections never obscure meter values or simulate activity.
inline void glassWindow(juce::Graphics& g, juce::Rectangle<float> r,
                        float radius=6.0f) {
    g.setColour(juce::Colours::black.withAlpha(0.83f));
    g.fillRoundedRectangle(r.translated(0,1.9f),radius);
    juce::ColourGradient bezel(juce::Colour(0xffb89862),r.getX(),r.getY(),
                                juce::Colour(0xff332617),r.getRight(),r.getBottom(),false);
    bezel.addColour(0.52,juce::Colour(0xff5b472c));
    g.setGradientFill(bezel);
    g.fillRoundedRectangle(r,radius);
    const auto well=r.reduced(2.2f);
    g.setColour(juce::Colour(0xff020503));
    g.fillRoundedRectangle(well,juce::jmax(1.0f,radius-1.5f));
    juce::ColourGradient glass(juce::Colour(0xff1c251e),well.getX(),well.getY(),
                                juce::Colour(0xff020302),well.getRight(),well.getBottom(),false);
    glass.addColour(0.38,juce::Colour(0xff090c09));
    g.setGradientFill(glass);
    g.fillRoundedRectangle(well.reduced(1.1f),juce::jmax(1.0f,radius-2.5f));
    g.setColour(juce::Colours::white.withAlpha(0.13f));
    g.drawLine(well.getX()+5.0f,well.getY()+2.1f,
               well.getRight()-5.0f,well.getY()+2.1f,0.8f);
    g.setColour(juce::Colours::black.withAlpha(0.49f));
    g.drawLine(well.getX()+4.0f,well.getBottom()-2.0f,
               well.getRight()-4.0f,well.getBottom()-2.0f,1.0f);
}

// Heavy aged iron surround; emerald=true replaces red enamel on reference.
inline void plate(juce::Graphics& g, juce::Rectangle<float> r, bool green=false,
                  uint32_t seed=0x6172u, float radius=9.0f, float wear=1.0f,
                  bool previousIronBackground=false) {
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRoundedRectangle(r.translated(0,3.5f),radius);
    // User-selected aged bronze charcoal: the three large Ossuary chassis
    // panels use warm grey-brown paint, with retained scuffed exposed metal.
    // Other themes, enamel plates, controls, knobs and text stay unchanged.
    const auto ironPaint = [&](juce::uint32 dark, juce::uint32 earlier) {
        return juce::Colour(previousIronBackground ? earlier : dark);
    };
    juce::ColourGradient enamel(green?juce::Colour(0xff16432e)
                                    :ironPaint(0xff11130f,0xff887968),
                r.getX(),r.getY(),green?juce::Colour(0xff030f0a)
                                     :ironPaint(0xff040604,0xff5c5146),
                r.getX()+r.getWidth()*0.8f,r.getBottom(),false);
    enamel.addColour(0.27,green?juce::Colour(0xff0e3020)
                               :ironPaint(0xff0e100c,0xff796b5e));
    enamel.addColour(0.56,green?juce::Colour(0xff071d12)
                               :ironPaint(0xff090b08,0xff6d5f52));
    g.setGradientFill(enamel);
    g.fillRoundedRectangle(r,radius);
    // Authentic source-image material grain overlays only large STATIC panels.
    // Live meter faces (310 px) remain free of tiled-image work at 30 Hz.
    // Both photographed tiles are compiled into JUCE BinaryData, decoded once
    // on the UI thread, and cached with the static brushed-metal artwork.
    if (r.getWidth() > 350.0f && r.getHeight() > 60.0f) {
        static const juce::Image originalIronGrain =
            juce::ImageFileFormat::loadFrom(BinaryData::irongrain_jpg,
                         static_cast<size_t>(BinaryData::irongrain_jpgSize));
        static const juce::Image originalGreenPatina =
            juce::ImageFileFormat::loadFrom(BinaryData::greenpatina_jpg,
                         static_cast<size_t>(BinaryData::greenpatina_jpgSize));
        const auto& grain = green ? originalGreenPatina : originalIronGrain;
        if (grain.isValid()) {
            juce::Graphics::ScopedSaveState textureScope(g);
            g.setTiledImageFill(grain, static_cast<int>(r.getX()+4),
                                static_cast<int>(r.getY()+4), green ? 0.39f : 0.36f);
            g.fillRoundedRectangle(r.reduced(6.0f),juce::jmax(0.0f,radius-4.0f));
        }
    }
    // The raised frame sits above a recessed enamel well. Broad, fixed
    // directional light and a tight bottom-right contact shadow create metal
    // depth without introducing any more scratches or random dirt.
    // Increase the paint abrasion only on the major dark chassis plates;
    // work is deterministic and cached in the editor's static backdrop.
    patina(g,r.reduced(2.2f),seed,wear*(previousIronBackground?1.55f:1.08f));
    if (previousIronBackground) {
        juce::Graphics::ScopedSaveState wearScope(g);
        const auto well=r.reduced(8.0f);
        g.reduceClipRegion(well.toNearestInt());
        // Short irregular exposed-brass scuffs, concentrated along the upper
        // and lower edges instead of compromising the live control labels.
        for (int i=0;i<160;++i) {
            const auto hash=noise(seed+0x23a3u+static_cast<uint32_t>(i*1151));
            const float rx=static_cast<float>(hash%10321u)/10321.0f;
            const float ry=static_cast<float>((hash>>14)%8191u)/8191.0f;
            const float xx=well.getX()+rx*well.getWidth();
            const bool top=(i%3)!=0;
            const float yy=top?well.getY()+ry*18.0f
                              :well.getBottom()-ry*19.0f;
            const float span=2.8f+static_cast<float>((hash>>23)&15u)*1.0f;
            g.setColour((i%4==0?juce::Colour(0xffdec79c)
                                :juce::Colour(0xffa98c64))
                            .withAlpha(i%7==0?0.33f:0.17f));
            g.drawLine(xx,yy,xx+span,yy-0.45f,(i%6==0)?1.25f:0.7f);
        }
    }
    {
        juce::Graphics::ScopedSaveState lightScope(g);
        const auto well=r.reduced(7.4f);
        g.reduceClipRegion(well.toNearestInt());
        juce::ColourGradient grazingLight(
            juce::Colours::white.withAlpha(green?0.085f:0.055f),
            well.getX(),well.getY(),
            juce::Colours::white.withAlpha(0.0f),
            well.getX()+well.getWidth()*0.55f,
            well.getY()+well.getHeight()*0.68f,false);
        g.setGradientFill(grazingLight);
        g.fillRect(well);
    }
    g.setColour(juce::Colours::black.withAlpha(0.56f));
    g.drawRoundedRectangle(r.reduced(6.9f),
                           juce::jmax(0.0f,radius-5.0f),1.45f);
    g.setColour(juce::Colour(0xffe1c99b).withAlpha(green?0.15f:0.10f));
    g.drawLine(r.getX()+radius+8.0f,r.getY()+6.9f,
               r.getRight()-radius-8.0f,r.getY()+6.9f,0.82f);
    // Gloss remains inside the painted plate. The black rack body and the
    // emerald enamel are not merged into the same green surface.
    if (green) {
        g.setColour(juce::Colour(0xffd9edc6).withAlpha(0.16f));
        g.drawLine(r.getX()+17.0f,r.getY()+7.0f,
                   r.getRight()-17.0f,r.getY()+7.0f,1.25f);
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawLine(r.getX()+15.0f,r.getBottom()-6.0f,
                   r.getRight()-15.0f,r.getBottom()-6.0f,1.3f);
    }
    g.setColour(juce::Colour(0xffe9c995).withAlpha(0.46f));
    g.drawRoundedRectangle(r.reduced(0.8f),radius,1.3f);
    g.setColour(juce::Colour(0xff0b0906));
    g.drawRoundedRectangle(r.reduced(3.3f),juce::jmax(0.0f,radius-2.2f),2.3f);
    g.setColour(juce::Colour(0xffb98a50).withAlpha(0.50f));
    g.drawRoundedRectangle(r.reduced(5.8f),juce::jmax(0.0f,radius-4.0f),0.8f);

    // Machined 3-D lip: warm overhead light, sharp falloff along the right,
    // black shadow underneath and worn-through metal at the rim. These are
    // real line geometries on the panel, not a flattened interface bitmap.
    // In the cached chassis it adds depth at every editor size; on the
    // small meter plates the cost is just a handful of paths.
    g.setColour(juce::Colour(0xffffe3ae).withAlpha(green?0.24f:0.19f));
    g.drawLine(r.getX()+radius+6.0f,r.getY()+4.7f,
               r.getRight()-radius-6.0f,r.getY()+4.7f,0.82f);
    g.setColour(juce::Colours::black.withAlpha(0.58f));
    g.drawLine(r.getX()+radius+5.0f,r.getBottom()-4.1f,
               r.getRight()-radius-5.0f,r.getBottom()-4.1f,1.2f);
    g.drawLine(r.getRight()-4.2f,r.getY()+radius,
               r.getRight()-4.2f,r.getBottom()-radius,1.1f);
    g.setColour(juce::Colour(0xffe3c38d).withAlpha(0.13f));
    g.drawLine(r.getX()+4.4f,r.getY()+radius,
               r.getX()+4.4f,r.getBottom()-radius,0.8f);
    // Fine directional grinding marks along the bare mounting lip.
    const int lipMarks=juce::jlimit(4,60,static_cast<int>(r.getWidth()/21.0f));
    for(int i=0;i<lipMarks;++i) {
        const auto h=noise(seed+0x173bu+static_cast<uint32_t>(i*937u));
        const float xx=r.getX()+radius+4.0f+
            static_cast<float>(h%32749u)/32749.0f*
            juce::jmax(1.0f,r.getWidth()-radius*2.0f-8.0f);
        const float length=1.4f+static_cast<float>((h>>20)&7u)*0.55f;
        g.setColour(i%5==0?gold.withAlpha(0.21f):
                             juce::Colours::black.withAlpha(0.24f));
        g.drawLine(xx,r.getY()+5.3f,xx+length,r.getY()+5.3f,0.65f);
    }
    // Irregular flaked enamel near the panel borders, with warm exposed metal
    // beneath the black or green paint. Keep chips behind the inset trim.
    const int edgeMarks=juce::jlimit(10,125,static_cast<int>(r.getWidth()/11.0f));
    for (int i=0;i<edgeMarks;++i) {
        const auto h=noise(seed+static_cast<uint32_t>(i*463+0x72e3u));
        const float t=static_cast<float>(h%10001u)/10001.0f;
        const bool upper=(i%3==0);
        const float px=r.getX()+8.0f+t*(r.getWidth()-17.0f);
        const float jitter=static_cast<float>((h>>17)&7u)*0.65f;
        const float py=upper?r.getY()+5.0f+jitter:r.getBottom()-5.0f-jitter;
        const float len=1.0f+static_cast<float>((h>>21)&31u)*0.31f;
        const float delta=static_cast<float>((h>>26)&7u)*0.18f;
        g.setColour(i%7==0?juce::Colour(0xffe9c58b).withAlpha(0.42f):
                            juce::Colour(0xff937147).withAlpha(0.18f));
        g.drawLine(px,py,px+len,py+(upper?delta:-delta),i%5==0?1.1f:0.6f);
    }
    // Cluster small chipped-paint marks toward the rim/corners rather than
    // repeating a uniform scratch pattern across the control labels.
    if (r.getWidth() > 350.0f && r.getHeight() > 70.0f) {
        for (int i=0;i<74;++i) {
            const auto n=noise(seed+0x6cc6u+static_cast<uint32_t>(i*293u));
            const float u=static_cast<float>(n%8191u)/8191.0f;
            const float v=static_cast<float>((n>>13)%4093u)/4093.0f;
            const float along=(i%2==0?u*u:1.0f-(1.0f-u)*(1.0f-u));
            const float x=r.getX()+9.0f+along*(r.getWidth()-18.0f);
            const bool top=i%3==0;
            const float y=(top?r.getY()+7.0f:r.getBottom()-7.0f)
                          +(top?1.0f:-1.0f)*v*3.0f;
            const float w=1.3f+static_cast<float>((n>>26)&7u)*0.8f;
            g.setColour(gold.withAlpha(i%7==0?0.46f:0.17f));
            g.fillEllipse(x,y,w,0.55f+(w*0.20f));
        }
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
    // Deep bowed chrome pull bars, styled after the photographic rack handles.
    // All highlights remain fixed in the cached static editor faceplate.
    const float h=bottom-top;
    g.setColour(juce::Colour(0xff020202));
    g.fillRoundedRectangle(x-14.0f,top-5.0f,28.0f,h+10.0f,13.0f);
    juce::ColourGradient recess(juce::Colour(0xffa08155),x-13,top,
                               juce::Colour(0xff171713),x+13,top,false);
    recess.addColour(0.24,juce::Colour(0xff34302b));
    recess.addColour(0.75,juce::Colour(0xff1b1c18));
    g.setGradientFill(recess);
    g.fillRoundedRectangle(x-12.0f,top-3.0f,24.0f,h+6.0f,11.0f);
    g.setColour(gold.withAlpha(0.32f));
    g.drawRoundedRectangle(juce::Rectangle<float>(x-12,top-3,24,h+6),11.0f,0.85f);

    // Wide chromed grip: bright edges and slightly darker central belly.
    juce::ColourGradient steel(juce::Colour(0xffe3d6bf),x-9.5f,top,
                               juce::Colour(0xff171913),x+10.0f,top,false);
    steel.addColour(0.16,juce::Colour(0xfff4ede2));
    steel.addColour(0.34,juce::Colour(0xff706f65));
    steel.addColour(0.61,juce::Colour(0xffc2b9aa));
    steel.addColour(0.84,juce::Colour(0xff453b33));
    g.setGradientFill(steel);
    g.fillRoundedRectangle(x-9.0f,top+2.0f,18.0f,h-4.0f,9.0f);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawLine(x-5.9f,top+23.0f,x-5.9f,bottom-23.0f,1.8f);
    g.setColour(juce::Colour(0xff241d18).withAlpha(0.72f));
    g.drawLine(x+5.5f,top+16.0f,x+5.5f,bottom-16.0f,1.35f);
    g.setColour(gold.withAlpha(0.30f));
    g.drawLine(x+7.3f,top+26.0f,x+7.3f,bottom-26.0f,0.7f);
    screw(g,x,top+8.0f,0.72f);
    screw(g,x,bottom-8.0f,0.72f);
}

inline void divider(juce::Graphics& g, float x1,float y1,float x2,float y2) {
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawLine(x1+1,y1+1,x2+1,y2+1,2.0f);
    g.setColour(brass.withAlpha(0.52f));
    g.drawLine(x1,y1,x2,y2,1.0f);
}

} // namespace skelly::hardware
