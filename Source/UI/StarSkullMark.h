#pragma once
#include <JuceHeader.h>

// SKELL Master machined skull medallion. One clean skull, no orbit stars,
// no skull-key motif, and no decorative text baked into the art.
namespace skelly::ui {
inline void drawMachinedSkull(juce::Graphics& g, juce::Rectangle<float> bounds,
                             float opacity=1.0f,
                             juce::Colour pigment=juce::Colour(0xffd7bb86)) {
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(bounds.getWidth()/128.0f,
                                                bounds.getHeight()/104.0f)
                       .translated(bounds.getX(),bounds.getY()));
    const auto metal=pigment.withMultipliedAlpha(opacity);
    const auto shade=pigment.darker(0.74f).withMultipliedAlpha(opacity);
    const auto cut=juce::Colour(0xff16130d).withMultipliedAlpha(opacity);

    // Raised medallion shell with an inset black enamel field.
    g.setColour(juce::Colours::black.withAlpha(opacity*0.66f));
    g.fillEllipse(9.0f,2.5f,110.0f,99.0f);
    juce::ColourGradient rim(metal.brighter(0.22f),25.0f,8.0f,
                             shade.darker(0.26f),106.0f,94.0f,false);
    rim.addColour(0.4,metal.darker(0.25f));
    g.setGradientFill(rim);
    g.fillEllipse(11.0f,2.0f,106.0f,98.0f);
    g.setColour(juce::Colour(0xff16150f));
    g.fillEllipse(15.5f,6.5f,97.0f,89.0f);
    g.setColour(metal.withAlpha(opacity*0.56f));
    g.drawEllipse(16.4f,7.4f,95.2f,87.2f,1.15f);

    // Anodised cast skull: heavy, credible bone geometry instead of
    // cartoon eyes, stars, streaks or a printed logo.
    juce::Path bone;
    bone.startNewSubPath(31.0f,46.0f);
    bone.cubicTo(25.0f,24.0f,37.0f,12.0f,62.0f,11.0f);
    bone.cubicTo(89.0f,10.0f,104.0f,27.0f,97.0f,47.0f);
    bone.cubicTo(97.0f,59.0f,90.0f,63.0f,86.0f,67.0f);
    bone.lineTo(84.0f,84.0f);
    bone.cubicTo(75.0f,93.0f,53.0f,94.0f,44.0f,84.0f);
    bone.lineTo(42.0f,67.0f);
    bone.cubicTo(34.0f,62.0f,30.0f,56.0f,31.0f,46.0f);
    bone.closeSubPath();
    juce::ColourGradient face(metal.brighter(0.36f),47.0f,21.0f,
                              shade,87.0f,91.0f,false);
    face.addColour(0.52,metal);
    g.setGradientFill(face);
    g.fillPath(bone);
    g.setColour(shade.withAlpha(opacity*0.74f));
    g.strokePath(bone,juce::PathStrokeType(1.4f));

    // Shadowed sockets, sculpted brow and tapered nose.
    g.setColour(cut);
    g.fillEllipse(40.0f,45.0f,20.0f,17.0f);
    g.fillEllipse(68.0f,45.0f,20.0f,17.0f);
    juce::Path nose;
    nose.addTriangle(64.0f,61.0f,57.0f,73.0f,71.0f,73.0f);
    g.fillPath(nose);
    g.setColour(metal.brighter(0.27f).withAlpha(opacity*0.75f));
    g.drawLine(40.5f,43.8f,57.5f,42.6f,1.9f);
    g.drawLine(70.5f,42.6f,87.5f,43.8f,1.9f);
    g.setColour(shade);
    g.drawLine(44.0f,78.8f,84.0f,78.8f,2.3f);
    for(int i=0;i<5;++i) {
        const float x=49.5f+7.2f*i;
        g.setColour(cut.withAlpha(opacity*0.72f));
        g.drawLine(x,79.0f,x,87.0f,1.35f);
    }
    g.setColour(metal.brighter(0.30f).withAlpha(opacity*0.45f));
    g.drawLine(40.0f,34.0f,47.0f,24.0f,1.0f);
    g.drawLine(81.0f,25.0f,89.0f,35.0f,1.0f);
}
} // namespace skelly::ui
