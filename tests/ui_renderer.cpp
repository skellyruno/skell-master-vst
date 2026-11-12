// Actual Skelly Master JUCE editor rendered by pinned shared SkellCore.
// Development-only tool: not linked into or shipped with the VST3.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <SkellCore/HeadlessUIRenderer.h>
#include <JuceHeader.h>
#include <array>
#include <cstdio>
#include <vector>

namespace {
constexpr int expectedControls = 16; // 11 knobs and 5 mechanical text buttons
void setValue(SkellyMasterAudioProcessor& p, const char* id, float plainValue) {
    if (auto* param = p.parameters.getParameter(id))
        param->setValueNotifyingHost(param->convertTo0to1(plainValue));
}
void resetValues(SkellyMasterAudioProcessor& p) {
    static constexpr std::array<const char*, 12> ids {{
       "input", "drive", "glue", "dynamics", "lows", "highs",
       "sauce", "transients", "output", "ceiling", "clipper", "clipper", "bypass"
    }};
    for (const auto* id : ids)
        if (auto* param = p.parameters.getParameter(id))
            param->setValueNotifyingHost(param->getDefaultValue());
}
// APVTS SliderAttachment updates may be queued for the JUCE message loop.
// The screenshot tool has no live DAW loop, so explicitly synchronize the
// real visible Slider/TextButton controls from the real processor parameters.
// This is visual test setup, not a mock UI or alternate rendering code.
void synchronizeControls(SkellyMasterAudioProcessorEditor& editor,
                         SkellyMasterAudioProcessor& p) {
    static constexpr std::array<const char*, 11> ids {{
        "input", "drive", "glue", "dynamics", "lows", "highs",
        "sauce", "transients", "output", "ceiling"
    }};
    int index = 0;
    for (int i=0;i<editor.getNumChildComponents();++i) {
        auto* child = editor.getChildComponent(i);
        if (auto* slider = dynamic_cast<juce::Slider*>(child)) {
            if (index < static_cast<int>(ids.size())) {
                if (auto* raw = p.parameters.getRawParameterValue(ids[static_cast<size_t>(index)]))
                    slider->setValue(raw->load(std::memory_order_relaxed), juce::dontSendNotification);
                ++index;
            }
        } else if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            if (button->getButtonText() == "BYPASS") {
                if (auto* raw = p.parameters.getRawParameterValue("bypass"))
                    button->setToggleState(raw->load(std::memory_order_relaxed) >= 0.5f,
                                           juce::dontSendNotification);
            }
        }
    }
}
juce::StringArray inspect(SkellyMasterAudioProcessorEditor& editor) {
    juce::StringArray issues;
    std::vector<juce::Rectangle<int>> rectangles;
    int count = 0;
    for (int i = 0; i < editor.getNumChildComponents(); ++i) {
        const auto* component = editor.getChildComponent(i);
        if (!component || !component->isVisible()) continue;
        if (dynamic_cast<const juce::Slider*>(component) == nullptr
          && dynamic_cast<const juce::TextButton*>(component) == nullptr)
            continue;
        ++count;
        const auto bounds = component->getBounds();
        if (!editor.getLocalBounds().contains(bounds))
            issues.add("Control extends outside viewport");
        for (const auto& r : rectangles)
            if (r.intersects(bounds))
                issues.add("Interactive controls overlap");
        rectangles.push_back(bounds);
    }
    if (count != expectedControls)
        issues.add("Expected 11 rotary controls and five buttons, found " + juce::String(count));
    return issues;
}
} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceGui;
    const juce::String requested = argc > 2 && juce::String(argv[1]) == "--output"
        ? juce::String::fromUTF8(argv[2]) : juce::String("ui-screenshots");
    const juce::File output = juce::File::getCurrentWorkingDirectory().getChildFile(requested);
    SkellyMasterAudioProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    int failures = 0;
    {
        SkellyMasterAudioProcessorEditor editor(processor);
        skellcore::ui::headless::Options options;
        options.project = "Skelly Master Hardware Edition - real JUCE processor/editor";
        options.minimumColours = 12;
        // One existing hardware theme/page, plus bypass and automated values.
        options.scenes = {
            {"minimum_620", 620, 393, 0, 0, false},
            {"default_702", 702, 445, 0, 0, false},
            {"medium_960", 960, 608, 0, 0, false},
            {"reference_1200", 1200, 760, 0, 0, false},
            {"maximum_1800", 1800, 1140, 0, 0, false},
            {"bypass_minimum", 620, 393, 0, 1, false},
            {"bypass_default", 702, 445, 0, 1, false},
            {"drive_selected", 702, 445, 0, 2, false},
            {"eq_selected", 702, 445, 0, 3, false},
            {"mastering_active", 702, 445, 0, 4, false},
            {"theme_neon", 702, 445, 0, 5, false},
            {"theme_ivory", 702, 445, 0, 6, false},
            {"theme_obsidian", 702, 445, 0, 7, false}
        };
        options.configure = [&](const skellcore::ui::headless::Scene& scene) {
            // The exact same real left-skull callback used in the plugin is
            // exercised to capture and validate each shared SkellCore skin.
            const auto target=scene.mode==5 ? skellcore::ui::SkullSkin::Neon
                : scene.mode==6 ? skellcore::ui::SkullSkin::Ivory
                : scene.mode==7 ? skellcore::ui::SkullSkin::Obsidian
                : skellcore::ui::SkullSkin::Ossuary;
            for(int step=0;step<4 && editor.selectedSkin()!=target;++step) {
                for(int child=0;child<editor.getNumChildComponents();++child) {
                    auto* c=editor.getChildComponent(child);
                    if(c && c->getComponentID()=="skellcore-theme-skull") {
                        if(auto* b=dynamic_cast<juce::Button*>(c))
                            if(b->onClick) b->onClick();
                        break;
                    }
                }
            }
            resetValues(processor);
            switch (scene.mode) {
                case 1:
                    setValue(processor, "bypass", 1.0f);
                    break;
                case 2:
                    setValue(processor, "drive", 75.0f);
                    setValue(processor, "glue", 30.0f);
                    break;
                case 3:
                    setValue(processor, "lows", 4.5f);
                    setValue(processor, "highs", -2.5f);
                    setValue(processor, "sauce", 42.0f);
                    break;
                case 4:
                    setValue(processor, "input", 5.0f);
                    setValue(processor, "drive", 34.0f);
                    setValue(processor, "glue", 53.0f);
                    setValue(processor, "dynamics", 25.0f);
                    setValue(processor, "lows", 2.0f);
                    setValue(processor, "highs", 3.0f);
                    setValue(processor, "sauce", 34.0f);
                    setValue(processor, "transients", 55.0f);
                    setValue(processor, "output", -3.0f);
                    setValue(processor, "ceiling", -0.5f);
                    break;
                default:
                    break;
            }
            synchronizeControls(editor, processor);
        };
        options.inspect = [&] { return inspect(editor); };
        options.mustDiffer = {
            {"default_702", "bypass_default"},
            {"default_702", "drive_selected"},
            {"default_702", "eq_selected"},
            {"default_702", "mastering_active"},
            {"default_702", "theme_neon"},
            {"default_702", "theme_ivory"},
            {"default_702", "theme_obsidian"}
        };
        const auto report = skellcore::ui::headless::render(editor, output, options);
        for (const auto& error : report.errors) {
            std::fprintf(stderr, "SkellCore screenshot error: %s\n", error.toRawUTF8());
            ++failures;
        }
        if (report.captured != static_cast<int>(options.scenes.size())) {
            std::fprintf(stderr, "Expected %zu scenes, rendered %d\n",
                         options.scenes.size(), report.captured);
            ++failures;
        }

        // Contact sheet composed only of actual captured PNG files.
        const std::array<juce::String, 6> selected {{
            "minimum_620", "default_702", "reference_1200",
            "bypass_default", "drive_selected", "theme_neon"
        }};
        constexpr int cellW = 600, cellH = 400;
        juce::Image sheet(juce::Image::RGB, cellW * 3, cellH * 2, true);
        {
            juce::Graphics g(sheet);
            g.fillAll(juce::Colour(0xff111814));
            for (int index=0; index<static_cast<int>(selected.size()); ++index) {
                const auto imageFile = output.getChildFile(selected[static_cast<size_t>(index)] + ".png");
                const auto image = juce::ImageFileFormat::loadFrom(imageFile);
                if (!image.isValid()) {
                    ++failures;
                    continue;
                }
                const int x = (index % 3)*cellW;
                const int y = (index / 3)*cellH;
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
                g.drawText(selected[static_cast<size_t>(index)].replaceCharacter('_', ' '),
                           x + 10, y + 4, cellW - 20, 24, juce::Justification::centredLeft);
                g.drawImageWithin(image,x+10,y+36,cellW-20,cellH-48,
                                  juce::RectanglePlacement::centred);
            }
        }
        if (!skellcore::ui::headless::writePng(sheet,output.getChildFile("contact-sheet.png")))
            ++failures;
        std::printf("SkellCore real-editor capture: %d / %zu scenes, errors %d\n",
                    report.captured,options.scenes.size(),failures);
    }
    processor.releaseResources();
    return failures == 0 ? 0 : 1;
}
