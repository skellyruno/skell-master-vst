#include <JuceHeader.h>
#include <BinaryData.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
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
        check(processor.getNumPrograms()==1,
              "processor must expose its single legacy Default program");
        check(processor.getProgramName(0)=="Default",
              "processor legacy program zero must be named Default");
        check(processor.getProgramName(1).isEmpty(),
              "processor out-of-range legacy program name must be empty");
        processor.prepareToPlay(48000.0, 512);
        SkellyMasterAudioProcessorEditor editor(processor);
        // Snapshot the startup identity BEFORE testing knob positions.
        // The hardware-pointer screenshot test later intentionally moves
        // INPUT to +18dB, which is supposed to mark an unnamed custom state.
        {
            juce::ComboBox* initialMenu=nullptr;
            for(int i=0;i<editor.getNumChildComponents();++i)
                if(auto* combo=dynamic_cast<juce::ComboBox*>(editor.getChildComponent(i)))
                    if(combo->getComponentID()=="skell-master-preset")
                        initialMenu=combo;
            check(initialMenu!=nullptr
                && initialMenu->getSelectedId()==0
                && initialMenu->getText().isEmpty()
                && initialMenu->getItemId(0)==1
                && initialMenu->getItemText(0)=="DEFAULT"
                && !static_cast<bool>(initialMenu->getProperties().getWithDefault(
                    "skellMasterPresetModified",false)),
                "neutral startup must display DEFAULT while retaining saved ID0");
        }
        // Both DEFAULT identities must pass the source unchanged after the
        // reported latency, including an impulse at the first sample. Checking
        // only against internal bypass could hide identical filtering in both.
        for(int defaultId : {0,1}) {
            auto neutral=std::make_unique<SkellyMasterAudioProcessor>();
            auto dry=std::make_unique<SkellyMasterAudioProcessor>();
            neutral->setFactoryPresetId(defaultId);
            dry->setFactoryPresetId(defaultId);
            neutral->prepareToPlay(44100,256);
            dry->prepareToPlay(44100,256);
            if(auto* bp=dry->parameters.getParameter("bypass"))
                bp->setValueNotifyingHost(1.0f);
            juce::AudioBuffer<float> neutralAudio(2,256),dryAudio(2,256);
            juce::MidiBuffer midi;
            constexpr int blockSize=256, blocks=4;
            juce::AudioBuffer<float> source(2,blockSize*blocks);
            const int latency=neutral->getLatencySamples();
            check(latency>0 && latency<blockSize,"unexpected DEFAULT latency at 44.1 kHz");
            bool exactDry=true;
            for(int block=0;block<blocks;++block) {
                for(int k=0;k<256;++k) {
                    const int sample=block*blockSize+k;
                    const double t=sample/44100.0;
                    const float left=static_cast<float>(
                        .88*std::sin(2.0*juce::MathConstants<double>::pi*437.0*t)
                        +.27*std::sin(2.0*juce::MathConstants<double>::pi*1790.0*t)
                        +(sample==0 ? .95 : 0.0));
                    const float right=static_cast<float>(
                        .82*std::sin(2.0*juce::MathConstants<double>::pi*493.0*t)
                        -.22*std::sin(2.0*juce::MathConstants<double>::pi*7200.0*t)
                        -(sample==0 ? .87 : 0.0));
                    source.setSample(0,sample,left);
                    source.setSample(1,sample,right);
                    neutralAudio.setSample(0,k,left);
                    neutralAudio.setSample(1,k,right);
                    dryAudio.setSample(0,k,left);
                    dryAudio.setSample(1,k,right);
                }
                neutral->processBlock(neutralAudio,midi);
                dry->processBlock(dryAudio,midi);
                for(int k=0;k<256;++k)
                    for(int channel=0;channel<2;++channel) {
                        const int aligned=block*blockSize+k-latency;
                        const float expected=aligned>=0 ? source.getSample(channel,aligned) : 0.0f;
                        exactDry &= neutralAudio.getSample(channel,k)==expected
                                    && dryAudio.getSample(channel,k)==expected;
                    }
            }
            check(exactDry,"DEFAULT and its explicit bypass must equal the latency-aligned input");
            std::cout<<"PASS: sample-exact DEFAULT ID"<<defaultId
                     <<" and bypass at "<<latency<<" samples of latency\n";
        }

        // Every *fresh* control starts at displayed zero. Old projects still
        // restore their saved user settings rather than resetting on open.
        for (const auto* id : {"input","drive","glue","dynamics","lows",
                               "highs","sauce","transients","output","ceiling","clipper","deuceMatch"}) {
            auto* param=processor.parameters.getParameter(id);
            check(param!=nullptr,"missing mastering parameter");
            if(juce::String(id)=="clipper")
                check(param->getName(64)=="Clip",
                      "CLIP must be the host-facing name without changing its parameter ID");
            check(std::abs(param->convertFrom0to1(param->getDefaultValue()))<0.0001f,
                  "mastering control default must be zero");
        }
        auto* widthParam=processor.parameters.getParameter("width");
        check(widthParam && std::abs(widthParam->convertFrom0to1(widthParam->getDefaultValue())-100.0f)<0.0001f,
              "new Width parameter must be neutral at 100 percent");
        check(processor.usesDepthWidth(),"fresh instances retain legacy depth mode");
        check(processor.usesAnalogSauce(),"fresh instances use V13 analog Sauce");
        check(processor.usesEfficientDepth() && processor.usesAdaptiveCompression(),
              "fresh sessions enable V15 efficient depth and optional strong compression");
        {
            check(processor.getRequestedOversampling()==4 && processor.getActiveOversampling()==4,
                  "4x must remain the original default");
            auto quality=std::make_unique<SkellyMasterAudioProcessor>();
            quality->prepareToPlay(44100,257);
            const int originalLatency=quality->getLatencySamples();
            quality->requestOversampling(8);
            check(quality->getRequestedOversampling()==8 && quality->getActiveOversampling()==4
                  && quality->getLatencySamples()==originalLatency,
                  "quality request must not rebuild an active audio path");
            juce::MemoryBlock savedQuality;
            quality->getStateInformation(savedQuality);
            auto restoredQuality=std::make_unique<SkellyMasterAudioProcessor>();
            restoredQuality->setStateInformation(savedQuality.getData(),static_cast<int>(savedQuality.getSize()));
            check(restoredQuality->getRequestedOversampling()==8,"quality must persist");
            restoredQuality->prepareToPlay(44100,257);
            check(restoredQuality->getActiveOversampling()==8,"8x must be active on host prepare");
            restoredQuality->releaseResources();
            quality->releaseResources();
            quality->requestOversampling(2);
            quality->prepareToPlay(44100,257);
            check(quality->getActiveOversampling()==2,"2x must be active on host prepare");
            quality->requestOversampling(3);
            check(quality->getRequestedOversampling()==2,"invalid quality request must be rejected");
            quality->releaseResources();
        }
        auto* spatialParam=processor.parameters.getParameter("spatialWidth");
        check(spatialParam && std::abs(spatialParam->convertFrom0to1(spatialParam->getDefaultValue()))<.0001f,"V12 spatial Width defaults to zero");
        {
            auto old=processor.parameters.copyState();
            old.setProperty("skellMasterStateVersion",10,nullptr);
            old.setProperty("skellMasterOversampling",8,nullptr);
            old.setProperty("skellMasterDepthWidth",1,nullptr);
            for(int i=0;i<old.getNumChildren();++i) {
                auto child=old.getChild(i);
                if(child.getProperty("id").toString()=="width") child.setProperty("value",145.f,nullptr);
            }
            auto xml=old.createXml(); juce::MemoryBlock blob;
            juce::AudioProcessor::copyXmlToBinary(*xml,blob);
            SkellyMasterAudioProcessor legacy; legacy.setStateInformation(blob.getData(),static_cast<int>(blob.getSize()));
            check(!legacy.usesDepthWidth(),"V10 state must retain its old Width law");
            check(!legacy.usesAnalogSauce(),"old state must retain original Sauce");
            check(!legacy.usesEfficientDepth() && !legacy.usesAdaptiveCompression(),
                  "legacy state must preserve both old DSP responses");
            check(legacy.getRequestedOversampling()==4,"legacy sessions must remain 4x");
            check(legacy.parameters.getRawParameterValue("spatialWidth")->load()==0.f,"old states must not activate spatial Width");
            check(std::abs(legacy.parameters.getRawParameterValue("width")->load()-145.f)<.01f,"V10 Width value survives migration");
            juce::MemoryBlock saved; legacy.getStateInformation(saved);
            SkellyMasterAudioProcessor roundtrip; roundtrip.setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
            check(!roundtrip.usesDepthWidth(),"V10 Width law survives V13 resave");
            check(!roundtrip.usesAnalogSauce(),"legacy Sauce survives V13 resave");
            check(!roundtrip.usesEfficientDepth() && !roundtrip.usesAdaptiveCompression(),
                  "V15 resave must not silently upgrade old DSP");
        }
        check(std::abs(skelly::detail::ceilingOffsetToDb(0.0)+1.0)<1e-9,
              "zero ceiling adjustment must preserve the -1 dBTP target before safety margin");
        juce::Button* skinButton=nullptr;
        for (int childIndex=0;childIndex<editor.getNumChildComponents();++childIndex) {
            auto* child=editor.getChildComponent(childIndex);
            if(child && child->getComponentID()=="skellcore-theme-skull")
                skinButton=dynamic_cast<juce::Button*>(child);
        }
        check(skinButton && static_cast<bool>(skinButton->onClick),
              "left skull must have functional SkellCore skin callback");
        const auto originalSkin=editor.selectedSkin();
        skinButton->onClick();
        check(editor.selectedSkin()==skellcore::ui::nextSkin(originalSkin),
              "skull click advances shared SkellCore theme");
        for(int remaining=0;remaining<3;++remaining) skinButton->onClick();
        check(editor.selectedSkin()==originalSkin,
              "four skull clicks return to the original palette");
        editor.setVisible(true); // Offscreen component has no native desktop peer.

        // Visual coverage: compact min/default, intermediate, design, large.
        for (int width : {620, 702, 960, 1200, 1800}) {
            const int height=juce::roundToInt(width * 760.0 / 1200.0);
            editor.setSize(width,height);
            const auto screen=editor.getLocalBounds();
            int sliderCount=0, buttonCount=0, presetCount=0;
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
                } else if (dynamic_cast<const juce::ComboBox*>(component) != nullptr) {
                    ++presetCount; interactive.push_back(box);
                }
            }
            check(sliderCount==11 && buttonCount==6 && presetCount==1,
                  "missing mastering controls, monitor buttons or preset dropdown");
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
            if (width==1200) {
                // A new mastering instance must show completely unlit
                // gain-reduction digits and LEDs: the chassis frame itself
                // remains visible, but no fake GR is reported at idle.
                int illuminatedPixels=0;
                for(int yy=165;yy<202;yy+=2)
                    for(int xx=984;xx<1130;xx+=2) {
                        const auto c=rendered.getPixelAt(xx,yy);
                        if(c.getGreen()>130 && c.getGreen()>c.getRed()+28
                            && c.getGreen()>c.getBlue()+20) ++illuminatedPixels;
                    }
                for(int yy=229;yy<245;yy+=2)
                    for(int xx=978;xx<1145;xx+=2) {
                        const auto c=rendered.getPixelAt(xx,yy);
                        if(c.getGreen()>130 && c.getGreen()>c.getRed()+28
                            && c.getGreen()>c.getBlue()+20) ++illuminatedPixels;
                    }
                check(illuminatedPixels==0,
                    "idle gain-reduction display/LEDs must remain completely dark");
            }
            std::cout << "PASS: JUCE UI render, bounds, contrast " << width
                      << "x" << height << "\n";
        }

        juce::Slider* input=nullptr;
        juce::Slider* clipper=nullptr;
        juce::ComboBox* preset=nullptr;
        juce::TextButton* peak=nullptr;
        juce::TextButton* truePeak=nullptr;
        juce::TextButton* clear=nullptr;
        juce::TextButton* bypass=nullptr;
        juce::TextButton* reset=nullptr;
        for (int i=0;i<editor.getNumChildComponents();++i) {
            auto* component=editor.getChildComponent(i);
            if (auto* slider=dynamic_cast<juce::Slider*>(component)) {
                if (slider->getTooltip().startsWith("INPUT -")) input=slider;
                if (slider->getComponentID()=="utility-clipper") clipper=slider;
            }
            if (auto* combo=dynamic_cast<juce::ComboBox*>(component))
                if(combo->getComponentID()=="skell-master-preset") preset=combo;
            if (auto* button=dynamic_cast<juce::TextButton*>(component)) {
                if (button->getButtonText()=="BYPASS") bypass=button;
                if (button->getButtonText()=="RESET") reset=button;
                if (button->getButtonText()=="PEAK") peak=button;
                if (button->getButtonText()=="TRUE") truePeak=button;
                if (button->getButtonText()=="CLEAR") clear=button;
            }
        }
        check(input && bypass && reset && clipper && peak && truePeak && clear && preset,
              "cannot locate actual hardware monitor controls");
        // A mouse click on ANY part of the visible preset bar must reach
        // the ComboBox (not its overlaid child text label). Repeat at
        // the minimum and medium resize sizes, including the arrow edge.
        for(const int width : {620,702,1200}) {
            editor.setSize(width,juce::roundToInt(width*760.0/1200.0));
            const auto bounds=preset->getBounds();
            const int middleY=bounds.getCentreY();
            for(const int xx : {bounds.getX()+3,bounds.getCentreX(),
                                bounds.getRight()-5})
                check(editor.getComponentAt(xx,middleY)==preset,
                      "preset dropdown text or arrow intercepts native mouse clicks");
            for(auto* monitor : {peak,truePeak,clear}) {
                const auto local=monitor->getBounds().getCentre();
                check(editor.getComponentAt(local)==monitor,
                      "small-size PEAK/TRUE/CLEAR button hit area is obstructed");
            }
        }
        // JUCE's combo popup should not have its item states mutated by
        // the 30Hz timer while the user clicks a menu entry.
        editor.setSize(1200,760);

        // A true hardware rotary has a motionless bezel, barrel, lighting
        // and machined scale. The ivory pointer extends to 0.94 * face radius
        // (face = 0.755 * outer radius). The 3.35px shadow, its (0.7,1.0)
        // offset and raster antialiasing reach beyond 0.80 at this size.
        // Test the fixed retaining-ring annulus outside that shadow
        // safely outside that moving geometry, then separately verify that
        // the pointer itself changes. Never suppress actual hardware motion.
        editor.setSize(1200,760);
        const auto rotaryBounds=input->getLookAndFeel().getSliderLayout(*input).sliderBounds;
        const float knobCx=static_cast<float>(input->getX()+rotaryBounds.getX())
            +rotaryBounds.getWidth()*0.5f;
        const float knobCy=static_cast<float>(input->getY()+rotaryBounds.getY())
            +rotaryBounds.getHeight()*0.5f;
        const float knobRadius=juce::jmin(static_cast<float>(rotaryBounds.getWidth()),
                                         static_cast<float>(rotaryBounds.getHeight()))*0.431f;
        const auto renderPosition=[&](double value) {
            input->setValue(value,juce::sendNotificationSync);
            juce::Image screenshot(juce::Image::ARGB,1200,760,true);
            {
                juce::Graphics g(screenshot);
                paintDetachedTree(editor,g);
            }
            return screenshot;
        };
        const auto lowKnob=renderPosition(-18.0);
        const auto zeroKnob=renderPosition(0.0);
        const auto highKnob=renderPosition(18.0);
        for(const auto& entry : std::array<std::pair<const char*,juce::Image>,3> {{
                {"knob-low.png",lowKnob},{"knob-zero.png",zeroKnob},{"knob-high.png",highKnob}}}) {
            auto output=juce::File::getCurrentWorkingDirectory().getChildFile("ui-snapshots")
                .getChildFile(entry.first).createOutputStream();
            check(output && output->openedOk(),"cannot save rotary regression evidence");
            check(juce::PNGImageFormat().writeImageToStream(entry.second,*output),
                  "cannot encode rotary regression evidence");
        }
        int movingPointerPixels=0, fixedRingPixels=0;
        for(int y=juce::roundToInt(knobCy-knobRadius);
                y<juce::roundToInt(knobCy+knobRadius);++y)
            for(int x=juce::roundToInt(knobCx-knobRadius);
                    x<juce::roundToInt(knobCx+knobRadius);++x) {
                const float dx=x+0.5f-knobCx,dy=y+0.5f-knobCy;
                const float radial=std::sqrt(dx*dx+dy*dy)/knobRadius;
                const auto a=lowKnob.getPixelAt(x,y).getARGB();
                const auto b=zeroKnob.getPixelAt(x,y).getARGB();
                const auto c=highKnob.getPixelAt(x,y).getARGB();
                if(radial>=0.86f && radial<=0.90f) {
                    ++fixedRingPixels;
                    if(a!=b || b!=c)
                        std::cerr<<"Fixed rotary mismatch at "<<x<<","<<y
                            <<" radius="<<radial<<" center="<<knobCx<<","<<knobCy
                            <<" colors="<<a<<","<<b<<","<<c<<"\n";
                    check(a==b && b==c,
                          "machined knob bezel/knurl/lighting moved during rotation");
                }
                if(radial>=0.20f && radial<=0.62f && (a!=b || b!=c))
                    ++movingPointerPixels;
            }
        check(fixedRingPixels>100 && movingPointerPixels>20,
              "rotary pointer did not move independently of the fixed metal");
        std::cout << "PASS: physical knob ring stationary; ivory pointer moves at min/zero/max\n";
        // The final screenshot left INPUT at +18dB; this is an intentional
        // custom edit, NOT a fresh unmodified startup. Assert the new
        // immediate feedback, then return to neutral before preset tests.
        check(static_cast<bool>(preset->getProperties().getWithDefault(
                  "skellMasterPresetModified",false)),
              "rotating neutral INPUT to +18 must mark CUSTOM immediately");
        input->setValue(0.0,juce::sendNotificationSync);
        juce::Thread::sleep(55);
        juce::Timer::callPendingTimersSynchronously();
        check(!static_cast<bool>(preset->getProperties().getWithDefault(
                  "skellMasterPresetModified",false)),
              "returning neutral INPUT to 0 must clear its edited marker");

        // Monitoring toggles and CLEAR must never change mix/audio parameters.
        check(static_cast<bool>(peak->onClick) && static_cast<bool>(truePeak->onClick)
              && static_cast<bool>(clear->onClick),
              "monitor buttons must have working actions");
        const float beforeDrive=processor.parameters.getRawParameterValue("drive")->load();
        peak->onClick();
        truePeak->onClick();
        clear->onClick();
        check(std::abs(processor.parameters.getRawParameterValue("drive")->load()-beforeDrive)<.001f,
              "passive output monitoring modified audio");
        check(preset->getNumItems()==5
              && preset->getItemId(0)==1 && preset->getItemId(4)==5,
              "preset menu must contain only the five core presets");
        check(preset->getSelectedId()==0 && preset->getText().isEmpty()
              && !static_cast<bool>(preset->getProperties().getWithDefault(
                  "skellMasterPresetModified",false)),
              "restored neutral INPUT and preset ID0 must be unmodified");
        check(processor.usesReferenceClip(),"fresh instance must enable new CLIP sound");
        check(processor.usesChainOrderedClip(),
              "fresh instance must place Soft Clip Pro before L2-style limiter");
        check(processor.usesRefinedControlCurves(),
              "fresh v8 instances must use the refined compressor controls");
        // Reproduce a timer tick before ComboBox's asynchronous notification.
        preset->setSelectedId(2,juce::sendNotificationAsync);
        juce::Thread::sleep(50);
        juce::Timer::callPendingTimersSynchronously();
        check(preset->getSelectedId()==2,
              "timer must preserve a pending user preset selection");
        preset->onChange();
        check(processor.getFactoryPresetId()==2,
              "asynchronous preset selection must reach the processor");
        preset->setSelectedId(2,juce::sendNotificationSync);
        check(std::abs(processor.parameters.getRawParameterValue("drive")->load()-15.0f)<.12f,
              "WARM factory preset must apply its real DSP drive setting");
        preset->setSelectedId(3,juce::sendNotificationSync);
        check(std::abs(processor.parameters.getRawParameterValue("transients")->load()-22.0f)<.12f,
              "PUNCH factory preset must apply its real DSP transient setting");
        preset->setSelectedId(1,juce::sendNotificationSync);
        check(preset->getSelectedId()==1 && preset->getText()=="DEFAULT"
              && processor.getFactoryPresetId()==1,
              "existing factory ID1 must be named DEFAULT without renumbering");
        check(std::abs(processor.parameters.getRawParameterValue("drive")->load())<.12f,
              "DEFAULT factory preset must restore zero drive");
        // Exercise *every* original published factory value, not only a
        // handful of characteristic knobs. Factory5 includes the owner's
        // approved full-song calibration. IDs and other factories stay stable.
        static constexpr float frozenOriginals[5][11]={
            {0,0,0,0,0,0,0,0,0,0,0},
            {0,15,18,10,0,0,12,0,0,0,0},
            {0,8,12,7,0,0,0,22,0,0,0},
            {0,10,24,18,0,0,8,8,0,0,12},
            {5,8,20,11,-1,3,12,0,0,3,35}
        };
        const char* frozenKnobs[]={
            "input","drive","glue","dynamics","lows","highs",
            "sauce","transients","output","ceiling","clipper"
        };
        for(int oldId=1;oldId<=5;++oldId) {
            preset->setSelectedId(oldId,juce::sendNotificationSync);
            check(processor.getFactoryPresetId()==oldId &&
                  preset->getSelectedId()==oldId,
                  "published factory identity or selector ID was renumbered");
            for(int k=0;k<11;++k)
                check(std::abs(
                    processor.parameters.getRawParameterValue(frozenKnobs[k])->load()
                    -frozenOriginals[oldId-1][k])<.12f,
                    "published old factory control setting changed");
            check((processor.parameters.getRawParameterValue("deuceMatch")->load()>=.5f)
                  ==(oldId==5),
                  "published Deuce Match factory state changed");
        }
        // User-reference-matched starting point: deliberately uses only the
        // existing 11 stable APVTS parameter IDs; old presets stay unchanged.
        preset->setSelectedId(5,juce::sendNotificationSync);
        check(preset->getText()=="SKELL MASTER",
              "reference preset label must be visible in the real selector");
        const struct {const char* id; float expected;} matched[]={
            {"input",5.0f},{"drive",8.0f},{"glue",20.0f},
            {"dynamics",11.0f},{"lows",-1.0f},{"highs",3.0f},
            {"sauce",12.0f},{"transients",0.0f},{"output",0.0f},
            {"ceiling",3.0f},{"clipper",35.0f}
        };
        for (const auto& value : matched)
            check(std::abs(processor.parameters.getRawParameterValue(value.id)->load()
                           -value.expected)<.12f,
                  "SKELL MASTER factory parameter snapshot is incorrect");
        check(processor.parameters.getRawParameterValue("deuceMatch")->load()>=0.5f,
              "SKELL MASTER must enable the real Deuce DSP stages");
        check(processor.usesReferenceCalibration(),
              "selecting SKELL MASTER must enable the current reference calibration");
        {
            auto legacy=processor.parameters.copyState();
            legacy.setProperty("skellMasterStateVersion",8,nullptr);
            legacy.setProperty("skellMasterFactoryPreset",5,nullptr);
            legacy.setProperty("skellMasterClipAlgorithm",1,nullptr);
            legacy.setProperty("skellMasterClipPlacement",1,nullptr);
            legacy.setProperty("skellMasterRefinedControlCurves",1,nullptr);
            legacy.setProperty("skellMasterReferenceCalibration",1,nullptr);
            auto xml=legacy.createXml();
            juce::MemoryBlock blob;
            juce::AudioProcessor::copyXmlToBinary(*xml,blob);
            SkellyMasterAudioProcessor oldSession;
            oldSession.setStateInformation(blob.getData(),static_cast<int>(blob.getSize()));
            check(!oldSession.usesReferenceCalibration(),
                  "v8 host states must retain their previous limiter recovery");
            legacy.setProperty("skellMasterStateVersion",9,nullptr);
            legacy.setProperty("skellMasterReferenceCalibration",0,nullptr);
            xml=legacy.createXml();
            juce::AudioProcessor::copyXmlToBinary(*xml,blob);
            SkellyMasterAudioProcessor explicitLegacy;
            explicitLegacy.setStateInformation(blob.getData(),static_cast<int>(blob.getSize()));
            oldSession.prepareToPlay(44100,256);
            explicitLegacy.prepareToPlay(44100,256);
            juce::AudioBuffer<float> a(2,256),b(2,256);
            juce::MidiBuffer midi;
            for(int block=0;block<96;++block) {
                for(int i=0;i<256;++i) {
                    const int n=block*256+i;
                    const float sample=static_cast<float>(
                        .9*std::sin(n*2.0*juce::MathConstants<double>::pi*61.0/44100.0)
                        *(n%4410<900 ? 1.0 : .12));
                    a.setSample(0,i,sample); a.setSample(1,i,sample*.63f);
                    b.setSample(0,i,sample); b.setSample(1,i,sample*.63f);
                }
                oldSession.processBlock(a,midi);
                explicitLegacy.processBlock(b,midi);
                for(int c=0;c<2;++c)
                    for(int i=0;i<256;++i)
                        check(a.getSample(c,i)==b.getSample(c,i),
                              "v8 migration changed legacy reference audio");
            }
            std::cout<<"PASS: v8 reference calibration migration preserves exact audio\n";
        }
        // v7 projects knew ID6, but never IDs7..9; malformed v7 ID9
        // must saturate to ID6 without enabling an unintended new factory.
        auto oldV7=processor.parameters.copyState();
        oldV7.setProperty("skellMasterStateVersion",7,nullptr);
        oldV7.setProperty("skellMasterFactoryPreset",9,nullptr);
        auto oldV7Xml=oldV7.createXml();
        check(static_cast<bool>(oldV7Xml),"legacy v7 XML exists");
        juce::MemoryBlock oldV7Blob;
        if(oldV7Xml)juce::AudioProcessor::copyXmlToBinary(*oldV7Xml,oldV7Blob);
        SkellyMasterAudioProcessor v7Restored;
        v7Restored.setStateInformation(oldV7Blob.getData(),
                                    static_cast<int>(oldV7Blob.getSize()));
        check(v7Restored.getFactoryPresetId()==6
              && !v7Restored.usesRefinedControlCurves(),
              "old v7 states must cap identities and keep the old 100% compressor DSP");
        preset->setSelectedId(5,juce::sendNotificationSync);
        // v8 intentionally recalibrates only the primary factory #5 to the
        // owner's five-plugin reference. The old 14%/2dB assertions became
        // stale when this factory was explicitly updated and hid later tests.
        check(processor.getFactoryPresetId()==5 &&
              std::abs(processor.parameters.getRawParameterValue("clipper")->load()-35.0f)<.12f
              && std::abs(processor.parameters.getRawParameterValue("highs")->load()-3.0f)<.12f
              && std::abs(processor.parameters.getRawParameterValue("ceiling")->load()-3.0f)<.12f,
              "selecting factory #5 must restore current reference CLIP35/HIGHS3/CEILING3");
        // Editing factory #5 now creates a variant of the recalibrated sound.
        clipper->setValue(43.0,juce::sendNotificationSync);
        check(static_cast<bool>(preset->getProperties().getWithDefault(
                  "skellMasterPresetModified",false)),
              "published factory * must update immediately without a 30Hz timer");
        juce::Thread::sleep(70);
        juce::Timer::callPendingTimersSynchronously();
        check(static_cast<bool>(preset->getProperties().getWithDefault(
                  "skellMasterPresetModified",false)),
              "edited factory should display modified status");
        check(processor.getFactoryPresetId()==5,
              "customization must preserve saved base factory identity");
        check(preset->getSelectedId()==0,
              "editing a preset must allow choosing that same preset again");
        preset->setSelectedId(5,juce::sendNotificationSync);
        check(preset->getSelectedId()==5 && processor.getFactoryPresetId()==5,
              "reselecting SKELL MASTER must restore the same selected identity");
        check(std::abs(processor.parameters.getRawParameterValue("clipper")->load()-35.0f)<.12f,
              "reselecting SKELL MASTER must restore SKELL MASTER 35% clip setting");
        check(!static_cast<bool>(preset->getProperties().getWithDefault(
                  "skellMasterPresetModified",false)),
              "reapplying a factory must clear edited status");
        for (const auto& value : matched)
            check(std::abs(processor.parameters.getRawParameterValue(value.id)->load()
                           -value.expected)<.12f,
                  "reselecting SKELL MASTER must restore the measured SKELL MASTER settings");
        preset->setSelectedId(1,juce::sendNotificationSync);
        check(std::abs(processor.parameters.getRawParameterValue("input")->load())<.12f &&
              std::abs(processor.parameters.getRawParameterValue("highs")->load())<.12f &&
              std::abs(processor.parameters.getRawParameterValue("clipper")->load())<.12f &&
              processor.parameters.getRawParameterValue("deuceMatch")->load()<0.5f,
              "SKELL MASTER must not alter original DEFAULT factory sound or leave Deuce stages enabled");
        clipper->setValue(43.0,juce::sendNotificationSync);
        check(std::abs(processor.parameters.getRawParameterValue("clipper")->load()-43.0f)<.12f,
              "clipper knob must automate real DSP parameter");
        input->setValue(4.0, juce::sendNotificationSync);
        check(std::abs(processor.parameters.getRawParameterValue("input")->load()-4.0f)<.12f,
              "input slider automation attachment failed");
        bypass->setToggleState(true, juce::sendNotificationSync);
        check(processor.parameters.getRawParameterValue("bypass")->load()>=.5f,
              "bypass button automation attachment failed");
        check(static_cast<bool>(reset->onClick), "reset callback missing");
        reset->onClick();
        check(preset->getSelectedId()==0
              && processor.getFactoryPresetId()==0
              && preset->getItemText(0)=="DEFAULT",
              "reset must show DEFAULT without changing legacy startup ID0 or DSP");
        check(std::abs(processor.parameters.getRawParameterValue("input")->load())<.12f,
              "reset did not restore input default");
        check(processor.parameters.getRawParameterValue("bypass")->load()<.5f,
              "reset did not restore bypass default");
        check(std::abs(processor.parameters.getRawParameterValue("clipper")->load())<.12f,
              "reset did not restore clipper default");
        check(processor.parameters.getRawParameterValue("deuceMatch")->load()<.5f,
              "reset did not disable Deuce Match mode");

        // Regression: RESET must clear hidden legacy choices too. Previously
        // DEFAULT showed zero knobs but kept the old post-limiter CLIP mode,
        // making the next CLIP automation sound different from a fresh v8.
        processor.setReferenceClipEnabled(false);
        processor.setRefinedControlCurvesEnabled(false);
        check(!processor.usesReferenceClip() && !processor.usesRefinedControlCurves(),
              "legacy CLIP / compressor mode test precondition failed");
        reset->onClick();
        check(processor.getFactoryPresetId()==0 && preset->getSelectedId()==0
              && processor.usesReferenceClip()
              && processor.usesChainOrderedClip()
              && processor.usesRefinedControlCurves(),
              "RESET must restore fresh v8 CLIP placement and compressor response");
        check(std::abs(processor.parameters.getRawParameterValue("clipper")->load())<.12f
              && processor.parameters.getRawParameterValue("deuceMatch")->load()<.5f,
              "RESET must remain neutral after restoring hidden DSP modes");
        juce::MemoryBlock neutralResetState;
        processor.getStateInformation(neutralResetState);
        SkellyMasterAudioProcessor restoredNeutralReset;
        restoredNeutralReset.setStateInformation(
            neutralResetState.getData(),static_cast<int>(neutralResetState.getSize()));
        check(restoredNeutralReset.getFactoryPresetId()==0
              && restoredNeutralReset.usesReferenceClip()
              && restoredNeutralReset.usesChainOrderedClip()
              && restoredNeutralReset.usesRefinedControlCurves(),
              "DEFAULT reset modes must survive the real host state format");

        // Host project state must preserve the named factory identity as well as its real DSP mode.
        preset->setSelectedId(5,juce::sendNotificationSync);
        check(processor.getFactoryPresetId()==5,
              "selecting SKELL MASTER must store its non-automatable factory identity");
        juce::MemoryBlock savedState;
        processor.getStateInformation(savedState);
        SkellyMasterAudioProcessor restoredState;
        restoredState.setStateInformation(savedState.getData(),
                                          static_cast<int>(savedState.getSize()));
        check(restoredState.parameters.getRawParameterValue("deuceMatch")->load()>=0.5f,
              "Deuce Match did not survive host state save/reload");
        check(restoredState.getFactoryPresetId()==5,
              "factory preset identity did not survive host state save/reload");
        check(restoredState.usesReferenceCalibration(),
              "current reference calibration must survive host state save/reload");
        check(restoredState.usesReferenceClip(),
              "clip algorithm state did not survive save/reload");
        check(restoredState.usesChainOrderedClip(),
              "v7 chain placement did not survive state save/reload");
        check(restoredState.parameters.getRawParameterValue("bypass")->load()<0.5f,
              "SKELL MASTER factory state unexpectedly restored bypass");
        for (const auto& value : matched)
            check(std::abs(restoredState.parameters.getRawParameterValue(value.id)->load()
                           -value.expected)<.12f,
                  "SKELL MASTER parameter value did not survive host state save/reload");
        {
            SkellyMasterAudioProcessorEditor restoredEditor(restoredState);
            juce::ComboBox* restoredPreset=nullptr;
            for(int i=0;i<restoredEditor.getNumChildComponents();++i)
                if(auto* combo=dynamic_cast<juce::ComboBox*>(restoredEditor.getChildComponent(i)))
                    if(combo->getComponentID()=="skell-master-preset") restoredPreset=combo;
            check(restoredPreset && restoredPreset->getSelectedId()==5,
                  "restored editor did not display the saved SKELL MASTER factory name");
        }

        // v6 hosted sessions published a REFERENCE CLIP sound *after* the
        // mastering limiter. Recalling those states must keep the old clip
        // order even if the knob is at 35% or higher.
        auto v6State=processor.parameters.copyState();
        v6State.setProperty("skellMasterStateVersion",6,nullptr);
        v6State.setProperty("skellMasterClipAlgorithm",1,nullptr);
        v6State.removeProperty("skellMasterClipPlacement",nullptr);
        auto v6Xml=v6State.createXml();
        check(static_cast<bool>(v6Xml),"v6 reference clip host XML exists");
        juce::MemoryBlock v6Blob;
        if(v6Xml)juce::AudioProcessor::copyXmlToBinary(*v6Xml,v6Blob);
        SkellyMasterAudioProcessor oldV6;
        oldV6.setStateInformation(v6Blob.getData(),static_cast<int>(v6Blob.getSize()));
        check(oldV6.usesReferenceClip() && !oldV6.usesChainOrderedClip(),
              "v6 Reference CLIP must keep post-limiter topology");
        {
            SkellyMasterAudioProcessorEditor oldV6Editor(oldV6);
            juce::ComboBox* v6Menu=nullptr;
            for(int i=0;i<oldV6Editor.getNumChildComponents();++i)
                if(auto* combo=dynamic_cast<juce::ComboBox*>(oldV6Editor.getChildComponent(i)))
                    if(combo->getComponentID()=="skell-master-preset") v6Menu=combo;
            check(v6Menu!=nullptr,"v6 mode switching menu must be accessible");
            if(v6Menu) {
                check(v6Menu->getNumItems()==5
                      && oldV6.usesReferenceClip() && !oldV6.usesChainOrderedClip(),
                      "opening the simplified menu must retain v6 clip topology");
            }
        }

        // Legacy v4 sessions predate the non-automatable factory identity and must restore neutral SELECT.
        auto legacyState=processor.parameters.copyState();
        for(int index=legacyState.getNumChildren()-1;index>=0;--index)
            if(legacyState.getChild(index).getProperty("id").toString()=="deuceMatch")
                legacyState.removeChild(index,nullptr);
        legacyState.setProperty("skellMasterStateVersion",4,nullptr);
        legacyState.removeProperty("skellMasterFactoryPreset",nullptr);
        auto legacyXml=legacyState.createXml();
        check(static_cast<bool>(legacyXml),"legacy host state XML must be created");
        juce::MemoryBlock legacyBlob;
        if(legacyXml)juce::AudioProcessor::copyXmlToBinary(*legacyXml,legacyBlob);
        SkellyMasterAudioProcessor restoredLegacyState;
        restoredLegacyState.setFactoryPresetId(5);
        restoredLegacyState.setStateInformation(legacyBlob.getData(),static_cast<int>(legacyBlob.getSize()));
        check(restoredLegacyState.parameters.getRawParameterValue("deuceMatch")->load()<0.5f,
              "legacy project without Deuce Match must default to OFF");
        check(restoredLegacyState.getFactoryPresetId()==0,
              "legacy host state without factory identity must restore SELECT");
        check(!restoredLegacyState.usesReferenceClip()
              && !restoredLegacyState.usesChainOrderedClip()
              && !restoredLegacyState.usesRefinedControlCurves(),
              "v1-5 project must retain CLIP, topology, and old compressor response");
        // A hostile/old v6 state with an impossible factory identity 6
        // must keep the historic saturation clamp at 5. Only v7 can save
        // the new reference factory 6.
        auto oldBadFactory=processor.parameters.copyState();
        oldBadFactory.setProperty("skellMasterStateVersion",6,nullptr);
        oldBadFactory.setProperty("skellMasterFactoryPreset",6,nullptr);
        auto badFactoryXml=oldBadFactory.createXml();
        check(static_cast<bool>(badFactoryXml),"old bad-factory XML created");
        juce::MemoryBlock oldBadBlob;
        if(badFactoryXml)juce::AudioProcessor::copyXmlToBinary(*badFactoryXml,oldBadBlob);
        SkellyMasterAudioProcessor migratedOldFactory;
        migratedOldFactory.setStateInformation(oldBadBlob.getData(),
                     static_cast<int>(oldBadBlob.getSize()));
        check(migratedOldFactory.getFactoryPresetId()==5,
              "pre-v7 malformed factory ID6 must not enable the new preset");

        // XML numeric properties are text after deserialization; clamp invalid positive IDs.
        auto malformedFactoryState=processor.parameters.copyState();
        malformedFactoryState.setProperty("skellMasterStateVersion",5,nullptr);
        malformedFactoryState.setProperty("skellMasterFactoryPreset",juce::String("99"),nullptr);
        auto malformedFactoryXml=malformedFactoryState.createXml();
        check(static_cast<bool>(malformedFactoryXml),"malformed factory state XML must be created");
        juce::MemoryBlock malformedFactoryBlob;
        if(malformedFactoryXml)juce::AudioProcessor::copyXmlToBinary(*malformedFactoryXml,malformedFactoryBlob);
        SkellyMasterAudioProcessor restoredMalformedFactoryState;
        restoredMalformedFactoryState.setStateInformation(malformedFactoryBlob.getData(),
            static_cast<int>(malformedFactoryBlob.getSize()));
        check(restoredMalformedFactoryState.getFactoryPresetId()==5,
              "out-of-range serialized factory identity must clamp to the last valid preset");

        // A numeric prefix followed by junk used to parse as factory ID 5,
        // displaying SKELL MASTER while the saved Deuce Match mode was OFF.
        auto suffixFactoryState=processor.parameters.copyState();
        suffixFactoryState.setProperty("skellMasterStateVersion",5,nullptr);
        suffixFactoryState.setProperty("skellMasterFactoryPreset",juce::String("5junk"),nullptr);
        for(int index=0;index<suffixFactoryState.getNumChildren();++index) {
            auto child=suffixFactoryState.getChild(index);
            if(child.getProperty("id").toString()=="deuceMatch") {
                child.setProperty("value",0.0f,nullptr);
                break;
            }
        }
        auto suffixFactoryXml=suffixFactoryState.createXml();
        check(static_cast<bool>(suffixFactoryXml),"suffix factory state XML must be created");
        juce::MemoryBlock suffixFactoryBlob;
        if(suffixFactoryXml)juce::AudioProcessor::copyXmlToBinary(*suffixFactoryXml,suffixFactoryBlob);
        SkellyMasterAudioProcessor restoredSuffixFactoryState;
        restoredSuffixFactoryState.setStateInformation(suffixFactoryBlob.getData(),
            static_cast<int>(suffixFactoryBlob.getSize()));
        check(restoredSuffixFactoryState.getFactoryPresetId()==0,
              "malformed numeric-prefix factory identity must restore neutral SELECT");
        check(restoredSuffixFactoryState.parameters.getRawParameterValue("deuceMatch")->load()<0.5f,
              "malformed factory identity must not imply Deuce Match is enabled");

        processor.parameters.getParameter("deuceMatch")->setValueNotifyingHost(0.0f);
        std::cout << "PASS: true-peak safety, Deuce Match state recall, factory identity, clipper, bypass and automation bindings\n";

        // Host-like audio checks exercise the actual JUCE wrapper + 4x resampler.
        // Explicitly isolate the test state: earlier UI cases leave the
        // ceiling at +3 and BYPASS modified. A hardcoded -1dB sample ceiling
        // cannot be asserted against those unrelated preceding selections.
        auto* ceilingParameter=processor.parameters.getParameter("ceiling");
        check(ceilingParameter!=nullptr,"audio fixture requires ceiling parameter");
        ceilingParameter->setValueNotifyingHost(
            ceilingParameter->convertTo0to1(0.0f));
        processor.parameters.getParameter("bypass")->setValueNotifyingHost(0.0f);
        processor.parameters.getParameter("deuceMatch")->setValueNotifyingHost(1.0f);
        check(std::abs(processor.parameters.getRawParameterValue("ceiling")->load())<0.01f
              && processor.parameters.getRawParameterValue("bypass")->load()<0.5f
              && processor.parameters.getRawParameterValue("deuceMatch")->load()>=0.5f,
              "host audio fixture must enter active mastering at -1dB target");
        for (const int sampleRate : {44100, 48000, 96000}) {
            processor.prepareToPlay(static_cast<double>(sampleRate),512);
            const auto latency=processor.getLatencySamples();
            check(latency>=32 && latency<sampleRate/30,
                  "reported plugin latency outside reasonable range");
            skelly::StereoTruePeakMonitor outputTruePeak;
            outputTruePeak.reset();
            double maxOutputTruePeak=0.0;
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
                    for (int i=0;i<blockSize;++i)
                        maxOutputTruePeak=std::max(maxOutputTruePeak,
                            static_cast<double>(outputTruePeak.process(
                                buffer.getSample(0,i),buffer.getSample(1,i))));
                }
            }
            check(maxOutputTruePeak<=std::pow(10.0,-1.25/20.0)+3.0e-4,
                  "host processor exceeded the -1.25 dBTP safety regression ceiling");
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
