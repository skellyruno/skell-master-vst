#pragma once
#include <JuceHeader.h>

// Original vector insignia: star-eyed skull with distressed orbit stars.
// Deliberately no typography or raster assets. Drawn on the message thread.
namespace skelly::ui {
inline void drawStarSkull(juce::Graphics& g, juce::Rectangle<float> bounds,
                         float opacity = 1.0f)
{
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(bounds.getWidth() / 128.0f,
                                                bounds.getHeight() / 104.0f)
                       .translated(bounds.getX(), bounds.getY()));

    const auto neon = juce::Colour(0xff59f76d).withMultipliedAlpha(opacity);
    const auto glow = juce::Colour(0xff208f3a).withMultipliedAlpha(opacity);
    const auto cutout = juce::Colour(0xff0d1210).withMultipliedAlpha(opacity);
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

    juce::ColourGradient headLight(juce::Colour(0xff7aff88).withMultipliedAlpha(opacity),
                                   47, 15,
                                   juce::Colour(0xff2ec654).withMultipliedAlpha(opacity),
                                   85, 85, false);
    headLight.addColour(0.56, neon);
    g.setGradientFill(headLight);
    g.fillPath(head);
    g.setColour(juce::Colour(0xffb2ffba).withMultipliedAlpha(opacity * 0.40f));
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
