#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool result, const char* message) {
    if (!result) throw std::runtime_error(message);
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
}
} // namespace

int main() {
    try {
        juce::ScopedJuceInitialiser_GUI juceInit;
        SkellyMasterAudioProcessor processor;
        processor.prepareToPlay(48000.0, 512);
        SkellyMasterAudioProcessorEditor editor(processor);

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
            juce::Graphics graphics(rendered);
            editor.paintEntireComponent(graphics,true);
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
            std::cout << "PASS: host wrapper " << sampleRate
                      << " Hz, variable block sizes, ceiling, latency="
                      << latency << " samples\n";
        }
        processor.releaseResources();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
