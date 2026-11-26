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
// Original SKELLY meter-card insignia. Kept only in the two readouts;
// the master header has its own separate machined emblem.
inline void drawStarSkull(juce::Graphics& g, juce::Rectangle<float> bounds,
                         float opacity = 1.0f,
                         juce::Colour pigment = juce::Colour(0xff59f76d))
{
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(bounds.getWidth() / 128.0f,
                                                bounds.getHeight() / 104.0f)
                       .translated(bounds.getX(), bounds.getY()));

    const auto neon = pigment.withMultipliedAlpha(opacity);
    const auto glow = pigment.darker(0.78f).withMultipliedAlpha(opacity);
    const auto cutout = juce::Colour(0xff141109).withMultipliedAlpha(opacity);
    const auto star = [](float x, float y, float outer, int points = 5) {
        juce::Path p;
        p.addStar({x, y}, points, outer * 0.40f, outer,
                  -juce::MathConstants<float>::halfPi);
        return p;
    };

    // Distinctive scattered stars and crossing streaks, kept behind the head.
    juce::Path streaks;
    streaks.startNewSubPath(8.0f, 48.0f);
    streaks.lineTo(45.0f, 72.0f);
    streaks.startNewSubPath(119.0f, 40.0f);
    streaks.lineTo(86.0f, 72.0f);
    g.setColour(glow);
    g.strokePath(streaks, juce::PathStrokeType(7.0f,
                 juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(neon.withAlpha(opacity * 0.80f));
    g.fillPath(star(14, 48, 13));
    g.fillPath(star(112, 42, 15));
    g.fillPath(star(66, 97, 10));
    g.setColour(glow);
    g.fillPath(star(27, 73, 5));
    g.fillPath(star(102, 77, 6));

    // Broad rounded cranium, cheek bones and tapered jaw.
    juce::Path head;
    head.startNewSubPath(29, 48);
    head.cubicTo(20, 23, 38, 8, 62, 8);
    head.cubicTo(91, 7, 105, 25, 98, 52);
    head.cubicTo(96, 60, 92, 63, 88, 66);
    head.lineTo(86, 81);
    head.cubicTo(74, 95, 52, 95, 41, 81);
    head.lineTo(40, 67);
    head.cubicTo(33, 62, 30, 57, 29, 48);
    head.closeSubPath();

    juce::ColourGradient headLight(pigment.brighter(0.27f).withMultipliedAlpha(opacity),
                                   47, 15,
                                   pigment.darker(0.37f).withMultipliedAlpha(opacity),
                                   85, 85, false);
    headLight.addColour(0.56, neon);
    g.setGradientFill(headLight);
    g.fillPath(head);
    g.setColour(pigment.brighter(0.38f).withMultipliedAlpha(opacity * 0.40f));
    g.strokePath(head, juce::PathStrokeType(0.75f));

    // Black five-point eye sockets and triangular nose.
    g.setColour(cutout);
    g.fillPath(star(49, 47, 12));
    g.fillPath(star(79, 47, 12));
    juce::Path nose;
    nose.addTriangle(64, 58, 57, 72, 71, 72);
    g.fillPath(nose);

    // Expressive grin carved from the base, followed by teeth.
    juce::Path smile;
    smile.startNewSubPath(46, 77);
    smile.quadraticTo(64, 90, 82, 77);
    smile.quadraticTo(77, 93, 64, 94);
    smile.quadraticTo(52, 91, 46, 77);
    smile.closeSubPath();
    g.fillPath(smile);
    g.setColour(neon.withAlpha(opacity * 0.9f));
    for (int n = 0; n < 5; ++n)
        g.drawLine(53.0f + n * 5.5f, 81.5f, 53.0f + n * 5.5f, 87.5f, 1.4f);

    // Fixed-seed stipple, rather than random repaint-time flicker.
    // Avoid the eye/nose region so the mark stays recognizable at small sizes.
    for (int i = 0; i < 42; ++i) {
        const int rx = (i * 37 + 19) % 109;
        const int ry = (i * 53 + 23) % 96;
        const float x = static_cast<float>(rx + 9);
        const float y = static_cast<float>(ry + 4);
        if (x > 38 && x < 92 && y > 32 && y < 91)
            continue;
        g.setColour(i % 3 == 0 ? neon.withAlpha(opacity * 0.38f)
                               : glow.withAlpha(opacity * 0.43f));
        const float d = 0.9f + static_cast<float>(i % 4) * 0.55f;
        g.fillEllipse(x, y, d, d);
    }

    // Worn paint chips in the forehead, subtle rather than noisy.
    g.setColour(cutout.withAlpha(opacity * 0.32f));
    g.fillEllipse(39, 23, 4.5f, 1.7f);
    g.fillEllipse(74, 19, 2.7f, 2.4f);
    g.fillEllipse(88, 31, 3.8f, 1.9f);
}

} // namespace skelly::ui
