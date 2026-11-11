#include <JuceHeader.h>
#include <BinaryData.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void check(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
}
// CI runs without an on-screen plugin editor. The normal snapshot helper
// exits early for components that have never been attached to a desktop peer.
// Render the same JUCE paint callbacks recursively into a software image,
// including editable Labels inside Sliders and text on Buttons.
void paintDetachedTree(juce::Component& component, juce::Graphics& graphics) {
    juce::Graphics::ScopedSaveState save(graphics);
    graphics.reduceClipRegion(component.getLocalBounds());
    component.paint(graphics);
    for (int i=0;i<component.getNumChildComponents();++i) {
        auto* child=component.getChildComponent(i);
        if (child==nullptr || !child->isVisible()) continue;
        juce::Graphics::ScopedSaveState childSave(graphics);
        graphics.addTransform(juce::AffineTransform::translation(
            static_cast<float>(child->getX()),static_cast<float>(child->getY())));
        paintDetachedTree(*child, graphics);
    }
    component.paintOverChildren(graphics);
}
void saveSnapshot(juce::Image image, int width, int height) {
    const auto directory = juce::File::getCurrentWorkingDirectory()
        .getChildFile("ui-snapshots");
    check(directory.createDirectory(), "cannot create UI snapshot directory");
    auto stream = directory.getChildFile(
        "SkellyMaster-" + juce::String(width) + "x" + juce::String(height) + ".png")
        .createOutputStream();
    check(stream != nullptr && stream->openedOk(), "cannot create UI snapshot");
    juce::PNGImageFormat format;
    check(format.writeImageToStream(image, *stream), "PNG snapshot encode failed");
    stream->flush();
}
void checkRendered(juce::Image image) {
    check(!image.isNull(), "JUCE editor snapshot was null");
    int visible = 0;
    int green = 0;
    const int step = juce::jmax(3, image.getWidth()/180);
    for (int y=0; y<image.getHeight(); y+=step)
        for (int x=0; x<image.getWidth(); x+=step) {
            const auto c = image.getPixelAt(x,y);
            if (c.getBrightness() > 0.025f) ++visible;
            if (c.getGreen() > 105 && c.getGreen() > c.getRed()+18
                && c.getGreen() > c.getBlue()+12)
                ++green;
        }
    std::cout << "UI pixels: nonblack=" << visible
              << ", green=" << green << "\n";
    check(visible > 100, "editor rendered nearly blank");
    check(green > 6, "neon Skelly accents missing from UI snapshot");
    // Inspect the real white OUTPUT/L meter face, scaled to the tested size.
    int whiteFace=0;
    for (int y=static_cast<int>(image.getHeight()*0.210f);
         y<static_cast<int>(image.getHeight()*0.275f);y+=2)
        for (int x=static_cast<int>(image.getWidth()*0.315f);
             x<static_cast<int>(image.getWidth()*0.475f);x+=2) {
            const auto c=image.getPixelAt(x,y);
            if (c.getRed()>196 && c.getGreen()>196 && c.getBlue()>190
                && std::abs(static_cast<int>(c.getRed())-
                            static_cast<int>(c.getBlue()))<20) ++whiteFace;
        }
    check(whiteFace>20,"OUTPUT meter photographic face must render white");
}
} // namespace

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI juceInit;
        // Enforce that the reference-derived UI resources really decode.
        // Otherwise the vector fallback can hide a damaged asset in CI.
        const auto photoKnob=juce::ImageFileFormat::loadFrom(
            BinaryData::knob_photo_png,
            static_cast<size_t>(BinaryData::knob_photo_pngSize));
        const auto photoMeter=juce::ImageFileFormat::loadFrom(
            BinaryData::meter_card_jpg,
            static_cast<size_t>(BinaryData::meter_card_jpgSize));
        const auto iron=juce::ImageFileFormat::loadFrom(
            BinaryData::irongrain_jpg,
            static_cast<size_t>(BinaryData::irongrain_jpgSize));
        const auto green=juce::ImageFileFormat::loadFrom(
            BinaryData::greenpatina_jpg,
            static_cast<size_t>(BinaryData::greenpatina_jpgSize));
        check(photoKnob.isValid() && photoKnob.getWidth()>=80
              && photoKnob.getHeight()==photoKnob.getWidth(),
              "reference rotary PNG missing or incorrect aspect");
        check(photoKnob.getPixelAt(0,0).getAlpha() < 32,
              "rotary asset background not transparent");
        check(photoMeter.isValid() && photoMeter.getWidth()>=220,
              "reference VU meter JPEG missing or unexpectedly low resolution");
        check(iron.isValid() && green.isValid(),
              "photographic panel grain assets did not decode");
       #if SKELLY_MASTER_HEADER_ASSETS
        const auto header = juce::ImageFileFormat::loadFrom(
            BinaryData::header_nameplate_green_blank_png,
            static_cast<size_t>(BinaryData::header_nameplate_green_blank_pngSize));
        const auto skullLeft = juce::ImageFileFormat::loadFrom(
            BinaryData::logo_skull_header_left_png,
            static_cast<size_t>(BinaryData::logo_skull_header_left_pngSize));
        check(header.isValid() && skullLeft.isValid(),
              "reference header artwork did not decode");
        check(header.getWidth() >= 391 && header.getHeight() >= 73,
              "header nameplate below display resolution");
        check(skullLeft.getPixelAt(0,0).getAlpha() < 32,
              "transparent header skull assets have opaque corners");
        std::cout << "PASS: reference-kit header graphics decoded\n";
       #endif
        std::cout << "PASS: reference-derived hardware images decoded\n";
        SkellyMasterAudioProcessor processor;
        processor.prepareToPlay(48000.0, 512);
        SkellyMasterAudioProcessorEditor editor(processor);
        editor.setVisible(true); // Offscreen component has no native desktop peer.

        // Visual coverage: compact min/default, intermediate, design, large.
        for (int width : {620, 702, 960, 1200, 1800}) {
            const int height=juce::roundToInt(width * 760.0 / 1200.0);
            editor.setSize(width,height);
            const auto screen=editor.getLocalBounds();
            int sliderCount=0, buttonCount=0;
            std::vector<juce::Rectangle<int>> interactive;
            for (int i=0;i<editor.getNumChildComponents();++i) {
                const auto* component=editor.getChildComponent(i);
                if (!component->isVisible()) continue;
                const auto box=component->getBounds();
                check(screen.contains(box), "UI control outside editor bounds");
                if (dynamic_cast<const juce::Slider*>(component) != nullptr) {
                    ++sliderCount; interactive.push_back(box);
                } else if (dynamic_cast<const juce::TextButton*>(component) != nullptr) {
                    ++buttonCount; interactive.push_back(box);
                }
            }
            check(sliderCount==10 && buttonCount==2, "missing mastering controls");
            for (size_t i=0;i<interactive.size();++i)
                for (size_t j=i+1;j<interactive.size();++j)
                    check(!interactive[i].intersects(interactive[j]),
                          "two top-level interactive controls overlap");

            juce::Image rendered(juce::Image::ARGB,width,height,true);
            {
                juce::Graphics graphics(rendered);
                paintDetachedTree(editor,graphics);
                // Finish and flush the software drawing context before
                // inspecting or PNG-encoding the image pixels.
            }
            // Save before asserting: a visually broken renderer must still
            // publish an image for root-cause inspection in CI.
            if (width==620 || width==702 || width==1200)
                saveSnapshot(rendered,width,height);
            checkRendered(rendered);
            std::cout << "PASS: JUCE UI render, bounds, contrast " << width
                      << "x" << height << "\n";
        }

        juce::Slider* input=nullptr;
        juce::TextButton* bypass=nullptr;
        juce::TextButton* reset=nullptr;
        for (int i=0;i<editor.getNumChildComponents();++i) {
            auto* component=editor.getChildComponent(i);
            if (auto* slider=dynamic_cast<juce::Slider*>(component)) {
                if (slider->getTooltip().startsWith("INPUT -")) input=slider;
            }
            if (auto* button=dynamic_cast<juce::TextButton*>(component)) {
                if (button->getButtonText()=="BYPASS") bypass=button;
                if (button->getButtonText()=="RESET") reset=button;
            }
        }
        check(input && bypass && reset, "cannot locate UI controls");
        input->setValue(4.0, juce::sendNotificationSync);
        check(std::abs(processor.parameters.getRawParameterValue("input")->load()-4.0f)<.12f,
              "input slider automation attachment failed");
        bypass->setToggleState(true, juce::sendNotificationSync);
        check(processor.parameters.getRawParameterValue("bypass")->load()>=.5f,
              "bypass button automation attachment failed");
        check(static_cast<bool>(reset->onClick), "reset callback missing");
        reset->onClick();
        check(std::abs(processor.parameters.getRawParameterValue("input")->load())<.12f,
              "reset did not restore input default");
        check(processor.parameters.getRawParameterValue("bypass")->load()<.5f,
              "reset did not restore bypass default");
        std::cout << "PASS: input slider, bypass, reset and automation bindings\n";

        // Host-like audio checks exercise the actual JUCE wrapper + 4x resampler.
        for (const int sampleRate : {44100, 48000, 96000}) {
            processor.prepareToPlay(static_cast<double>(sampleRate),512);
            const auto latency=processor.getLatencySamples();
            check(latency>0 && latency<sampleRate/30,
                  "reported plugin latency outside reasonable range");
            for (int blockSize : {1, 32, 256, 511, 512}) {
                juce::AudioBuffer<float> buffer(2, blockSize);
                juce::MidiBuffer midi;
                for (int block=0;block<30;++block) {
                    for (int i=0;i<blockSize;++i) {
                        const double phase=(block*blockSize+i)*2.0*
                            juce::MathConstants<double>::pi*330.0/sampleRate;
                        const float x=static_cast<float>(0.85*std::sin(phase));
                        buffer.setSample(0,i,block==3 && i==0 ? 1.5f : x);
                        buffer.setSample(1,i,block==4 && i==0 ? -1.4f : x*.79f);
                    }
                    processor.processBlock(buffer,midi);
                    constexpr float margin=0.0002f;
                    const float ceiling=static_cast<float>(std::pow(10.0,-1.0/20.0));
                    for (int channel=0;channel<2;++channel)
                        for (int i=0;i<blockSize;++i) {
                            const float out=buffer.getSample(channel,i);
                            check(std::isfinite(out), "host processor generated NaN/Inf");
                            check(std::abs(out)<=ceiling+margin,
                                  "host processor violated sample peak ceiling");
                        }
                }
            }
            // Stress input sanitization at the actual JUCE oversampler boundary.
            // An invalid host frame must not poison the persistent filter
            // state, and the following normal programme must recover.
            processor.reset();
            juce::AudioBuffer<float> badBuffer(2, 257);
            juce::MidiBuffer badMidi;
            const auto bad = std::numeric_limits<float>::quiet_NaN();
            const auto inf = std::numeric_limits<float>::infinity();
            double recoveredEnergy = 0.0;
            constexpr float sampleCeiling = 0.891250938f; // -1 dBFS
            for (int block = 0; block < 48; ++block) {
                for (int i = 0; i < badBuffer.getNumSamples(); ++i) {
                    const double phase = (block * badBuffer.getNumSamples() + i)
                        * juce::MathConstants<double>::twoPi * 440.0 / sampleRate;
                    const float input = static_cast<float>(0.2 * std::sin(phase));
                    badBuffer.setSample(0, i, input);
                    badBuffer.setSample(1, i, input * 0.8f);
                }
                if (block == 3) {
                    badBuffer.setSample(0, 0, bad);
                    badBuffer.setSample(1, 3, inf);
                    badBuffer.setSample(0, 11, -inf);
                    badBuffer.setSample(0, 15, 1.e38f);
                }
                processor.processBlock(badBuffer, badMidi);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < badBuffer.getNumSamples(); ++i) {
                        const float output = badBuffer.getSample(channel, i);
                        check(std::isfinite(output),
                              "oversampler filter poisoned by invalid host audio");
                        check(std::abs(output) <= sampleCeiling + .0003f,
                              "sample-peak safety failed after invalid host audio");
                        if (block >= 42)
                            recoveredEnergy += static_cast<double>(output) * output;
                    }
            }
            check(recoveredEnergy > 2.0,
                  "host output did not recover after invalid samples");
            std::cout << "PASS: host wrapper " << sampleRate
                      << " Hz, variable blocks, invalid input recovery, ceiling, latency="
                      << latency << " samples\n";
        }
        processor.releaseResources();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
