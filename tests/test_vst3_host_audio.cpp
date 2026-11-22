// Windows VST3 host audio regression. Loads the INSTALLED/BUNDLE VST3 through
// JUCE's standard VST3 format instead of linking a processor directly.
// Optional command: SkellyVST3HostAudioTests <bundle.vst3> <input.wav> <output.wav> [--factory 1..9] [--set "Parameter=VALUE"] [--bool "Parameter=0|1"]
// With no WAV, deterministic independent stereo noise and a transient/bass program are used.
// Never commit private user's recordings; local WAV comparisons stay local.
#include <JuceHeader.h>
#include "../Source/DSP/MasteringEngine.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <random>
#include <string>
#include <vector>

static void require(bool condition, const char* why) {
    if (!condition) throw std::runtime_error(why);
}

static void require(bool condition, const std::string& why) {
    if (!condition) throw std::runtime_error(why);
}

struct Measurements {
    double samplePeak=0.0, reconstructedPeak=0.0, sumSquares=0.0;
    double midEnergy=0.0, sideEnergy=0.0, firstInputImpulse=-1.0, firstOutputImpulse=-1.0;
    double strongestShortGain=1.0, worstEnergy=0.0;
    size_t count=0;
    skelly::StereoTruePeakMonitor meter;
    void add(float l, float r) {
        require(std::isfinite(l) && std::isfinite(r), "VST3 output is not finite");
        samplePeak=std::max(samplePeak,static_cast<double>(std::max(std::abs(l),std::abs(r))));
        reconstructedPeak=std::max(reconstructedPeak,static_cast<double>(meter.process(l,r)));
        const double mid=0.5*(static_cast<double>(l)+r);
        const double side=0.5*(static_cast<double>(l)-r);
        midEnergy+=mid*mid;
        sideEnergy+=side*side;
        sumSquares+=0.5*(static_cast<double>(l)*l+static_cast<double>(r)*r);
        ++count;
    }
};

static float parseDisplayedNumericParameterValue(const juce::String& text) {
    const auto utf8 = text.toStdString();
    char* end = nullptr;
    const float value = std::strtof(utf8.c_str(), &end);
    require(end != utf8.c_str() && std::isfinite(value),
            "VST3 host did not return a numeric parameter value");
    return value;
}

// A hosted VST3 wrapper may not preserve AudioProcessorParameter::isBoolean()
// even when the underlying JUCE AudioParameterBool is correctly exported.
// Prefer the explicit flag, then recognize JUCE's stable Off/On text contract.
// Numeric stepped parameters still format as numbers and cannot pass this test.
static bool isBooleanParameter(const juce::AudioProcessorParameter& parameter) {
    if (parameter.isBoolean()) return true;
    // JUCE's hosted VST3 adapter may expose a boolean as a two-step parameter
    // while dropping the original AudioParameterBool flag and text labels.
    if (parameter.getNumSteps() == 2) return true;
    const auto values = parameter.getAllValueStrings();
    if (values.size() == 2
        && ((values[0].equalsIgnoreCase("Off") && values[1].equalsIgnoreCase("On"))
            || (values[0].equalsIgnoreCase("False") && values[1].equalsIgnoreCase("True"))))
        return true;
    const auto zeroText = parameter.getText(0.0f, 128).trim();
    const auto oneText = parameter.getText(1.0f, 128).trim();
    return (zeroText.equalsIgnoreCase("Off") && oneText.equalsIgnoreCase("On"))
        || (zeroText.equalsIgnoreCase("False") && oneText.equalsIgnoreCase("True"));
}

static void setParameterByName(juce::AudioPluginInstance& instance,
                               const juce::String& name, float actual) {
    int matches = 0;
    for (auto* param : instance.getParameters()) {
        if (param->getName(128).equalsIgnoreCase(name)) {
            require(!isBooleanParameter(*param),
                    "Numeric override cannot target a boolean parameter; use --bool");
            const auto normalized = param->getValueForText(juce::String(actual, 6));
            require(std::isfinite(normalized) && normalized >= 0.0f && normalized <= 1.0f,
                    "host parameter conversion produced an invalid normalized value");
            // Hosted VST3 parameters expose normalized values. Round-trip the
            // displayed value before setting it so an out-of-range request or
            // an illegal step cannot silently clamp to a different control.
            const float applied = parseDisplayedNumericParameterValue(
                param->getText(normalized, 128));
            require(std::abs(applied - actual) <= 0.01f,
                    "Numeric override is outside the legal parameter range or step: "
                    + name.toStdString());
            param->beginChangeGesture();
            param->setValueNotifyingHost(normalized);
            param->endChangeGesture();
            ++matches;
        }
    }
    require(matches == 1,
            "Expected exactly one non-boolean VST3 parameter with the requested public name");
}

static double getNumericParameterByName(juce::AudioPluginInstance& instance,
                                            const juce::String& name) {
    int matches = 0;
    double actual = 0.0;
    for (auto* param : instance.getParameters()) {
        if (param->getName(128).equalsIgnoreCase(name)) {
            require(!isBooleanParameter(*param),
                    "Numeric value lookup cannot target a boolean parameter");
            const auto display = param->getText(param->getValue(), 128);
            actual = display.getDoubleValue();
            require(std::isfinite(actual),
                    "Host parameter display value is not finite");
            ++matches;
        }
    }
    require(matches == 1,
            "Expected exactly one numeric VST3 parameter with the requested public name");
    return actual;
}

static void setBoolParameterByName(juce::AudioPluginInstance& instance,
                                   const juce::String& name, bool enabled) {
    const auto requested = name.trim();
    // Hosts may expose the dedicated bypass parameter separately from the
    // ordinary parameter list. Use JUCE's explicit bypass contract first.
    if (requested.equalsIgnoreCase("Bypass")) {
        if (auto* bypass = instance.getBypassParameter()) {
            bypass->beginChangeGesture();
            bypass->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
            bypass->endChangeGesture();
            return;
        }
    }
    int matches = 0;
    for (auto* param : instance.getParameters()) {
        const auto publicName = param->getName(128).trim();
        if (publicName.equalsIgnoreCase(requested)) {
            require(isBooleanParameter(*param),
                    "Boolean override must target a boolean VST3 parameter");
            param->beginChangeGesture();
            param->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
            param->endChangeGesture();
            ++matches;
        }
    }
    if (matches != 1) {
        std::cerr << "VST3 boolean parameter lookup failed: requested=["
                  << requested.toStdString() << "] available=[";
        for (int i = 0; i < instance.getParameters().size(); ++i) {
            if (i) std::cerr << ", ";
            std::cerr << "[" << instance.getParameters()[i]->getName(128).toStdString()
                      << "; steps=" << instance.getParameters()[i]->getNumSteps()
                      << "; bool=" << instance.getParameters()[i]->isBoolean() << "]";
        }
        if (auto* bypass = instance.getBypassParameter())
            std::cerr << "] dedicated-bypass=[" << bypass->getName(128).toStdString()
                      << "; steps=" << bypass->getNumSteps()
                      << "; bool=" << bypass->isBoolean();
        std::cerr << "]" << std::endl;
    }
    require(matches == 1,
            "Expected exactly one boolean VST3 parameter with the requested public name");
}

static bool getBoolParameterByName(juce::AudioPluginInstance& instance,
                                   const juce::String& name) {
    const auto requested = name.trim();
    if (requested.equalsIgnoreCase("Bypass")) {
        if (auto* bypass = instance.getBypassParameter())
            return bypass->getValue() >= 0.5f;
    }
    int matches = 0;
    bool enabled = false;
    for (auto* param : instance.getParameters()) {
        if (param->getName(128).trim().equalsIgnoreCase(requested)) {
            require(isBooleanParameter(*param),
                    "Boolean value lookup must target a boolean VST3 parameter");
            enabled = param->getValue() >= 0.5f;
            ++matches;
        }
    }
    require(matches == 1,
            "Expected exactly one boolean VST3 parameter with the requested public name");
    return enabled;
}

// JUCE's hosted VST3 state is an outer VST3PluginState XML wrapper.
// The actual processor state is base64 inside its IComponent child; editing
// only an attribute on the outer wrapper does not reach the plugin at all.
static std::unique_ptr<juce::XmlElement> getComponentStateXml(
    juce::AudioPluginInstance& instance,
    std::unique_ptr<juce::XmlElement>& wrapper) {
    juce::MemoryBlock hostState;
    instance.getStateInformation(hostState);
    require(hostState.getSize() > 0, "VST3 host state snapshot is empty");
    wrapper = juce::AudioProcessor::getXmlFromBinary(
        hostState.getData(), static_cast<int>(hostState.getSize()));
    require(wrapper != nullptr, "VST3 host state XML is missing");
    require(wrapper->hasTagName("VST3PluginState"),
            "Unexpected JUCE hosted VST3 state wrapper");
    auto* component = wrapper->getChildByName("IComponent");
    require(component != nullptr, "VST3 IComponent state is missing");
    juce::MemoryBlock componentState;
    require(componentState.fromBase64Encoding(component->getAllSubText()),
            "VST3 IComponent state base64 is invalid");
    auto componentXml = juce::AudioProcessor::getXmlFromBinary(
        componentState.getData(), static_cast<int>(componentState.getSize()));
    require(componentXml != nullptr, "VST3 processor component state XML is missing");
    require(componentXml->hasTagName("SKELLY_MASTER_PARAMETERS"),
            "VST3 IComponent did not contain the Skell Master processor state");
    return componentXml;
}

static int getFactoryIdentityFromHostState(juce::AudioPluginInstance& instance) {
    std::unique_ptr<juce::XmlElement> wrapper;
    const auto componentXml = getComponentStateXml(instance, wrapper);
    return componentXml->getIntAttribute("skellMasterFactoryPreset", -1);
}

static void setFactoryIdentityForHostTest(juce::AudioPluginInstance& instance, int presetId) {
    std::unique_ptr<juce::XmlElement> wrapper;
    auto componentXml = getComponentStateXml(instance, wrapper);
    componentXml->setAttribute("skellMasterFactoryPreset", presetId);

    juce::MemoryBlock updatedComponentState;
    juce::AudioProcessor::copyXmlToBinary(*componentXml, updatedComponentState);
    auto* component = wrapper->getChildByName("IComponent");
    require(component != nullptr, "VST3 IComponent state disappeared");
    // IComponent is a container element. JUCE serializes the stream as a
    // child text node (appendStateFrom uses addTextElement); XmlElement::setText
    // only works on text nodes and is a no-op/assertion on this container.
    component->deleteAllChildElements();
    component->addTextElement(updatedComponentState.toBase64Encoding());

    juce::MemoryBlock updatedHostState;
    juce::AudioProcessor::copyXmlToBinary(*wrapper, updatedHostState);
    instance.setStateInformation(updatedHostState.getData(),
                                  static_cast<int>(updatedHostState.getSize()));
    require(getFactoryIdentityFromHostState(instance) == presetId,
            "VST3 host could not restore the non-automatable factory identity");
}

static int applyCommandLineOverrides(juce::AudioPluginInstance& instance,
                                      int argc, char** argv, int firstOption) {
    // The host fixture uses exact published factory value snapshots to
    // verify every preset's real VST3 output. '--factory' is NON-automatable
    // and must be restored through the real saved plugin state. Overrides
    // are applied after selecting a factory, regardless of CLI option order.
    int factoryId=5;
    for(int i=firstOption;i<argc;i+=2) {
        require(i+1<argc,"Expected complete flag/value pair");
        const std::string option=argv[i];
        if(option=="--factory") {
            const std::string value=argv[i+1];
            require(value.size()==1 && value[0]>='1' && value[0]<='9',
                    "Hosted factory identity must be an integer 1..9");
            factoryId=static_cast<int>(value[0]-'0');
        }
    }
    // Frozen published 1..5 and opt-in v7/v8 additions. Keep this fixture
    // independent of the UI constants: host and UI regressions detect drift.
    static constexpr float factoryValues[9][11]={
        {0,0,0,0,0,0,0,0,0,0,0},
        {0,15,18,10,0,0,12,0,0,0,0},
        {0,8,12,7,0,0,0,22,0,0,0},
        {0,10,24,18,0,0,8,8,0,0,12},
        {4,8,17,11,-1,2,20,0,0,0,14},
        {4,8,17,11,-1,3,20,0,0,0,35},
        {0,15,15,8,2,-1,20,0,0,0,0},
        {-2,8,6,4,0,1,0,60,0,0,0},
        {0,12,14,11,0,1,8,8,2,0,20}
    };
    static constexpr const char* factoryNames[11]={
        "Input","Drive","Glue","Dynamics","Lows","Highs",
        "Sauce","Transients","Output Trim","Ceiling Adjust","Clip"
    };
    if(factoryId!=5) {
        for(int k=0;k<11;++k)
            setParameterByName(instance,factoryNames[k],
                               factoryValues[factoryId-1][k]);
        setBoolParameterByName(instance,"Deuce Match",
                               factoryId==5 || factoryId==6);
    }
    for (int i = firstOption; i < argc;) {
        const std::string option = argv[i++];
        require((option == "--set" || option == "--bool" ||
                 option == "--factory") && i < argc,
                "Expected --factory 1..9, --set NAME=VALUE or --bool NAME=0|1");
        if(option=="--factory") { ++i; continue; }
        const std::string assignment = argv[i++];
        const auto separator = assignment.find('=');
        require(separator != std::string::npos && separator > 0
                && separator + 1 < assignment.size(),
                "Parameter override must use NAME=VALUE syntax");
        const std::string name = assignment.substr(0, separator);
        const std::string value = assignment.substr(separator + 1);
        if (option == "--set") {
            char* end = nullptr;
            const double parsed = std::strtod(value.c_str(), &end);
            require(end != value.c_str() && end != nullptr && *end == '\0'
                    && std::isfinite(parsed)
                    && std::isfinite(static_cast<float>(parsed)),
                    "Numeric parameter override must be a finite number");
            setParameterByName(instance, name, static_cast<float>(parsed));
        } else {
            require(value == "0" || value == "1",
                    "Boolean parameter override must be 0 or 1");
            const bool requested = value == "1";
            setBoolParameterByName(instance, name, requested);
            require(getBoolParameterByName(instance, name) == requested,
                    "Boolean VST3 parameter override did not stick: " + name);
        }
    }
    return factoryId;
}

static void requireHostStateRoundTrip(juce::AudioPluginInstance& source,
                                       juce::AudioPluginInstance& restored,
                                       const char* context) {
    const int sourceFactoryIdentity = getFactoryIdentityFromHostState(source);
    juce::MemoryBlock state;
    source.getStateInformation(state);
    require(state.getSize() > 0, "VST3 host state snapshot is empty");
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    require(getFactoryIdentityFromHostState(restored) == sourceFactoryIdentity,
            "VST3 host state did not preserve non-automatable factory identity");

    const auto sourceParameters = source.getParameters();
    const auto restoredParameters = restored.getParameters();
    require(sourceParameters.size() == restoredParameters.size(),
            "VST3 host state changed the parameter count");
    for (int i = 0; i < sourceParameters.size(); ++i) {
        const auto sourceName = sourceParameters[i]->getName(128);
        const auto restoredName = restoredParameters[i]->getName(128);
        require(sourceName == restoredName,
                "VST3 host state changed parameter ordering or identity");
        require(std::abs(sourceParameters[i]->getValue()
                         - restoredParameters[i]->getValue()) <= 1.0e-5f,
                context);
    }
}

// Compare the real hosted audio path after a host-state restore. Parameter equality
// alone can miss hidden mode/state drift that only appears once DSP starts running.
static void requireHostRenderedStateMatch(juce::AudioPluginInstance& source,
                                          juce::AudioPluginInstance& restored,
                                          double sampleRate,
                                          int blockSize,
                                          const char* context) {
    require(source.getLatencySamples() == restored.getLatencySamples(),
            "VST3 host-state restore changed reported latency");
    source.reset();
    restored.reset();

    juce::AudioBuffer<float> sourceBuffer(2, blockSize);
    juce::AudioBuffer<float> restoredBuffer(2, blockSize);
    juce::MidiBuffer sourceMidi, restoredMidi;
    const juce::int64 totalFrames = static_cast<juce::int64>(sampleRate * 1.25);
    const juce::int64 pulsePeriod = static_cast<juce::int64>(sampleRate * 0.5);
    double maximumDifference = 0.0;
    double maximumOutputPeak = 0.0;

    for (juce::int64 position = 0; position < totalFrames; position += blockSize) {
        const int count = static_cast<int>(std::min<juce::int64>(
            blockSize, totalFrames - position));
        sourceBuffer.clear();
        restoredBuffer.clear();

        for (int i = 0; i < count; ++i) {
            const juce::int64 frame = position + i;
            const juce::int64 local = frame % pulsePeriod;
            const double t = static_cast<double>(frame) / sampleRate;
            const double transient = local < 120
                ? 0.85 * std::exp(-static_cast<double>(local) / 24.0) : 0.0;
            const double bass = 0.22 * std::sin(
                2.0 * juce::MathConstants<double>::pi * 61.0 * t);
            const double body = 0.11 * std::sin(
                2.0 * juce::MathConstants<double>::pi * 550.0 * t);
            const double air = 0.04 * std::sin(
                2.0 * juce::MathConstants<double>::pi * 9500.0 * t);
            const float left = static_cast<float>(transient + bass + body + air);
            const float right = static_cast<float>(
                0.82 * transient + 0.87 * bass + 0.67 * body - 0.75 * air);
            sourceBuffer.setSample(0, i, left);
            sourceBuffer.setSample(1, i, right);
            restoredBuffer.setSample(0, i, left);
            restoredBuffer.setSample(1, i, right);
        }

        sourceMidi.clear();
        restoredMidi.clear();
        source.processBlock(sourceBuffer, sourceMidi);
        restored.processBlock(restoredBuffer, restoredMidi);

        for (int i = 0; i < count; ++i) {
            const float sourceLeft = sourceBuffer.getSample(0, i);
            const float sourceRight = sourceBuffer.getSample(1, i);
            const float restoredLeft = restoredBuffer.getSample(0, i);
            const float restoredRight = restoredBuffer.getSample(1, i);
            require(std::isfinite(sourceLeft) && std::isfinite(sourceRight)
                    && std::isfinite(restoredLeft) && std::isfinite(restoredRight),
                    "VST3 state roundtrip produced non-finite audio");
            maximumDifference = std::max(maximumDifference,
                std::max(std::abs(static_cast<double>(sourceLeft) - restoredLeft),
                         std::abs(static_cast<double>(sourceRight) - restoredRight)));
            maximumOutputPeak = std::max(maximumOutputPeak,
                std::max(std::abs(static_cast<double>(sourceLeft)),
                         std::abs(static_cast<double>(sourceRight))));
        }
    }

    require(maximumDifference <= 2.0e-6, context);
    require(maximumOutputPeak <= 1.0,
            "VST3 state roundtrip produced an out-of-range sample");
    std::cout << "PASS: hosted VST3 state audio equivalence at " << sampleRate
              << " Hz; max difference " << maximumDifference
              << ", peak " << maximumOutputPeak << '\n';
}

static juce::File locateVST3(const juce::String& userArg) {
    if (userArg.isNotEmpty()) return juce::File(userArg);
    const auto root=juce::File::getCurrentWorkingDirectory();
    // CTest executes with cwd = build/, while developers often launch the
    // executable from the repository root. Support both cases reliably.
    const auto nested=root.getChildFile("build");
    const auto build=nested.isDirectory() ? nested : root;
    require(build.isDirectory(),"Cannot discover Windows build directory");
    // Filesystem path uses UTF-16 on Windows. Traverse bundles via JUCE.
    juce::Array<juce::File> found;
    build.findChildFiles(found,juce::File::findDirectories,true,"*.vst3");
    require(!found.isEmpty(),"Built Skelly Master.vst3 bundle was not found");
    return found.getFirst();
}

int main(int argc, char** argv) {
    try {
        juce::ScopedJuceInitialiser_GUI gui;
        // --rate N exercises the compiled VST3 at each supported host rate
        // using deterministic generated programme. Owner reference WAVs stay local
        // and are never sent to CI; CI separately smoke-tests file I/O with a
        // generated synthetic WAV fixture.
        const bool rateMode=argc>=3 && std::string(argv[1])=="--rate";
        const juce::File bundle=locateVST3(
            (!rateMode && argc>=2) ? juce::String::fromUTF8(argv[1]) : juce::String());
        require(bundle.exists(),"VST3 path does not exist");
        juce::AudioPluginFormatManager formats;
        // JUCE 8.0.9+ removed AudioPluginFormatManager::addDefaultFormats().
        // Register the actual headless VST3 host implementation explicitly:
        // this executable renders audio and intentionally never creates a UI.
        juce::addHeadlessDefaultFormatsToManager(formats);
        juce::AudioPluginFormat* vst3=nullptr;
        for(int i=0;i<formats.getNumFormats();++i) {
            auto* format=formats.getFormat(i);
            if(format->getName().containsIgnoreCase("VST3")) {vst3=format;break;}
        }
        require(vst3!=nullptr,"JUCE VST3 host format not enabled");
        juce::OwnedArray<juce::PluginDescription> descriptions;
        vst3->findAllTypesForFile(descriptions,bundle.getFullPathName());
        require(!descriptions.isEmpty(),"JUCE could not scan compiled VST3");
        require(descriptions.getFirst()->name.containsIgnoreCase("Master"),
                "Unexpected VST3 found");

        juce::AudioFormatManager audioFormats;
        audioFormats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader;
        const bool externalWav=!rateMode && argc>=3;
        if(externalWav) {
            const juce::File input(juce::String::fromUTF8(argv[2]));
            require(input.existsAsFile(),"User WAV input not found");
            reader.reset(audioFormats.createReaderFor(input));
            require(reader!=nullptr,"Could not read WAV input");
            require(reader->numChannels==2,"Reference WAV must be stereo");
        }
        const double sampleRate=reader ? reader->sampleRate
            : rateMode ? std::stod(argv[2]) : 44100.0;
        require(sampleRate==44100.0 || sampleRate==48000.0 || sampleRate==96000.0,
                "Unsupported test sample rate; expected 44100, 48000 or 96000 Hz");
        const int blockSize=257;
        juce::String error;
        auto plugin=formats.createPluginInstance(*descriptions.getFirst(),sampleRate,
                                                 blockSize,error);
        if (!plugin) throw std::runtime_error("Cannot instantiate built VST3: " + error.toStdString());
        const int hostProgramCount = plugin->getNumPrograms();
        // VST3 hosts are not required to expose JUCE's legacy AudioProcessor
        // program list. Some JUCE VST3 host instances report zero programs even
        // when the wrapped processor implements one named Default program.
        // The direct processor contract is tested by SkellyUITests, while
        // pluginval validates the staged VST3 bundle independently.
        require(hostProgramCount == 0 || hostProgramCount == 1,
                "VST3 host exposed an unexpected number of legacy programs");
        if (hostProgramCount == 1) {
            require(plugin->getProgramName(0) == "Default",
                    "VST3 program 0 must have a non-empty Default name");
            require(plugin->getProgramName(1).isEmpty(),
                    "out-of-range VST3 program index must remain empty");
        } else {
            std::cout << "INFO: VST3 host does not expose the legacy program list; "
                         "processor contract is checked separately\\n";
        }
        plugin->setPlayConfigDetails(2,2,sampleRate,blockSize);
        plugin->prepareToPlay(sampleRate,blockSize);
        require(plugin->getLatencySamples()>=32,"VST3 latency omitted the 32-sample true-peak lookahead");
        const int reportedLatency=plugin->getLatencySamples();

        // Real hosted VST3 automation, no private/mutable factory preset data.
        // These are the already-public values of the current SKELL MASTER
        // reference preset. Existing preset identities/IDs are untouched.
        setParameterByName(*plugin,"Input",4.0f);
        setParameterByName(*plugin,"Drive",8.0f);
        setParameterByName(*plugin,"Glue",17.0f);
        setParameterByName(*plugin,"Dynamics",11.0f);
        setParameterByName(*plugin,"Lows",-1.0f);
        setParameterByName(*plugin,"Highs",2.0f); // Keep host render seed in sync with factory preset 5.
        setParameterByName(*plugin,"Sauce",20.0f);
        setParameterByName(*plugin,"Transients",0.0f);
        setParameterByName(*plugin,"Output Trim",0.0f);
        setParameterByName(*plugin,"Ceiling Adjust",0.0f);
        setParameterByName(*plugin,"Clip",14.0f);
        setBoolParameterByName(*plugin,"Deuce Match",true);
        // Opt-in factory identity and values override this frozen factory5
        // baseline. With no flags, the existing render remains identical.
        // Explicit --set/--bool always supersedes the chosen factory value.
        const int requestedFactory=externalWav
            ? applyCommandLineOverrides(*plugin, argc, argv, 4) : 5;
        // Factory identity is deliberately non-automatable and is not present
        // in getParameters(). Seed the named SKELL MASTER identity through the
        // real serialized host-state path so the following VST3 roundtrip
        // verifies it separately from the exposed APVTS parameters.
        setFactoryIdentityForHostTest(*plugin, requestedFactory);

        // Exercise the actual hosted VST3 component-state path, not only the
        // processor/UI fixture. Every public parameter must survive a fresh
        // plugin instance restoring the saved binary state.
        auto restoredPlugin = formats.createPluginInstance(
            *descriptions.getFirst(), sampleRate, blockSize, error);
        require(restoredPlugin != nullptr,
                "Cannot instantiate second VST3 for host-state roundtrip: " + error.toStdString());
        restoredPlugin->setPlayConfigDetails(2, 2, sampleRate, blockSize);
        restoredPlugin->prepareToPlay(sampleRate, blockSize);
        requireHostStateRoundTrip(*plugin, *restoredPlugin,
            "VST3 host state did not preserve all factory parameter values");
        requireHostRenderedStateMatch(*plugin, *restoredPlugin, sampleRate, blockSize,
            "Deuce Match host-state reload changed the rendered audio");

        // Bypass is independently serialized and must preserve the same
        // latency-aligned dry output after a fresh hosted-state restore.
        setBoolParameterByName(*plugin,"Bypass",true);
        requireHostStateRoundTrip(*plugin, *restoredPlugin,
            "VST3 host state did not preserve bypass alongside Deuce Match");
        requireHostRenderedStateMatch(*plugin, *restoredPlugin, sampleRate, blockSize,
            "Bypass host-state reload changed the rendered audio");
        setBoolParameterByName(*plugin,"Bypass",false);
        plugin->reset();
        restoredPlugin->releaseResources();

        const juce::int64 inputFrames=reader ? reader->lengthInSamples
                                           : static_cast<juce::int64>(sampleRate*9);
        require(inputFrames>0 && inputFrames<sampleRate*600,
                "Input duration invalid or longer than ten minutes");
        const int tail=std::max(1024,reportedLatency+1024);
        const juce::int64 totalFrames=inputFrames+tail;
        Measurements metrics;
        metrics.meter.reset();
        juce::AudioBuffer<float> buffer(2,blockSize);
        juce::MidiBuffer midi;
        // Stream rendered blocks directly to disk. The previous whole-file
        // vector plus a second full AudioBuffer doubled peak memory usage and
        // could approach 1 GiB for the documented 10-minute / 96 kHz limit.
        std::unique_ptr<juce::AudioFormatWriter> outputWriter;
        if(argc>=4) {
            const juce::File outputPath(juce::String::fromUTF8(argv[3]));
            std::unique_ptr<juce::OutputStream> stream=outputPath.createOutputStream();
            require(stream!=nullptr,"Could not create WAV output");
            auto* wav=audioFormats.findFormatForFileExtension("wav");
            require(wav!=nullptr,"JUCE WAV writer unavailable");
            outputWriter=wav->createWriterFor(stream,
                juce::AudioFormatWriterOptions{}
                    .withSampleRate(sampleRate)
                    .withNumChannels(2)
                    .withBitsPerSample(24));
            require(outputWriter!=nullptr,"Could not create output WAV encoder");
        }

        std::mt19937 syntheticNoiseRng(0x51e11u);
        std::uniform_real_distribution<float> syntheticNoise(-0.95f, 0.95f);
        double dspSeconds=0.0;
        const auto started=std::chrono::steady_clock::now();
        for (juce::int64 pos=0; pos<totalFrames;pos+=blockSize) {
            const int count=static_cast<int>(std::min<juce::int64>(blockSize,totalFrames-pos));
            buffer.clear();
            if (reader && pos < inputFrames) {
                const int readable=static_cast<int>(std::min<juce::int64>(count,inputFrames-pos));
                reader->read(&buffer,0,readable,pos,true,true);
            } else if (!reader) {
                for(int i=0;i<count;++i) {
                    const juce::int64 n=pos+i;
                    if(n>=inputFrames) continue;
                    if(n < static_cast<juce::int64>(sampleRate * 0.25)) {
                        // Exercise independent L/R broadband peaks through the real
                        // hosted VST3 path before the musical transient fixture.
                        buffer.setSample(0, i, syntheticNoise(syntheticNoiseRng));
                        buffer.setSample(1, i, syntheticNoise(syntheticNoiseRng));
                        continue;
                    }
                    const double t=static_cast<double>(n)/sampleRate;
                    const double kick=(n%22050<160)
                        ? 1.15*std::exp(-static_cast<double>(n%22050)/37.0):0.0;
                    const double bass=0.32*std::sin(2.0*juce::MathConstants<double>::pi*61.0*t);
                    const double mid=0.21*std::sin(2.0*juce::MathConstants<double>::pi*550.0*t);
                    const double air=0.07*std::sin(2.0*juce::MathConstants<double>::pi*9500.0*t);
                    buffer.setSample(0,i,static_cast<float>(kick+bass+mid+air));
                    buffer.setSample(1,i,static_cast<float>(kick*.82+bass*.86+mid*.65-air*.72));
                }
            }
            // JUCE VST3 processBlock may be called with a smaller final block.
            if(count<blockSize) buffer.clear(count,blockSize-count);
            midi.clear();
            const auto dspStarted=std::chrono::steady_clock::now();
            plugin->processBlock(buffer,midi);
            dspSeconds+=std::chrono::duration<double>(
                std::chrono::steady_clock::now()-dspStarted).count();
            for(int i=0;i<count;++i) {
                const float l=buffer.getSample(0,i),r=buffer.getSample(1,i);
                metrics.add(l,r);
            }
            if(outputWriter)
                require(outputWriter->writeFromAudioSampleBuffer(buffer,0,count),
                        "Failed to stream rendered WAV block");
        }
        const double renderSeconds=std::chrono::duration<double>(
            std::chrono::steady_clock::now()-started).count();
        // Close the writer before returning so the WAV header/frame count is
        // finalized even when the caller immediately inspects the output.
        outputWriter.reset();
        plugin->releaseResources();
        const auto dB=[](double linear) {
            return 20.0*std::log10(std::max(1.e-14,linear));
        };
        const double rms=std::sqrt(metrics.sumSquares/std::max<size_t>(1,metrics.count));
        const double width=10.0*std::log10(
            std::max(1.e-14,metrics.sideEnergy) / std::max(1.e-14,metrics.midEnergy));
        const double ceilingOffset = getNumericParameterByName(*plugin, "Ceiling Adjust");
        const double ceilingDb = skelly::detail::ceilingOffsetToDb(ceilingOffset);
        const double configuredSampleCeiling = skelly::detail::dbToLinear(ceilingDb);
        const bool deuceMatchEnabled = getBoolParameterByName(*plugin, "Deuce Match");
        const bool referenceClipEnabled = getNumericParameterByName(*plugin, "Clip")>0;
        const bool bypassEnabled = getBoolParameterByName(*plugin, "Bypass");
        // The new-reference VST3 instance also uses the final reconstructed
        // true-peak guard with CLIP alone, even when Deuce Match is OFF.
        // Legacy restored states continue using historical sample-peak mode.
        // Bypass is aligned dry audio, without either ceiling constraint.
        if (!bypassEnabled) {
            require(metrics.samplePeak<=configuredSampleCeiling+3.e-4,
                    "Hosted VST3 escaped the configured sample-peak ceiling");
            // New v8 Warm+/Punch+/Loud+ all opt into reconstructed
            // true-peak safety even when CLIP=0 and Deuce Match is OFF.
            // Original published factories retain their legacy sample
            // guard for saved-project parity.
            if (deuceMatchEnabled || referenceClipEnabled ||
                (requestedFactory>=7 && requestedFactory<=9)) {
                // The production guard uses a rate-adaptive 0.5/0.45/0.4dB margin at 44.1/48/96kHz. The hosted
                // 48-tap monitor must remain 0.1dB below the configured
                // true-peak target; portable tests also check an independent 16x oracle.
                const double configuredTruePeakCeiling =
                    skelly::detail::dbToLinear(ceilingDb - 0.1);
                require(metrics.reconstructedPeak<=configuredTruePeakCeiling+3.e-4,
                        "Hosted reference CLIP / Deuce escaped the true-peak guard margin");
            }
        }
        require(std::isfinite(metrics.reconstructedPeak)&&std::isfinite(width),
                "Hosted VST3 true peak/width invalid");
        require(rms>1.e-5,"VST3 unexpectedly silent");
        std::cout<<"VST3_HOST_RENDER PASS"
                 <<" input="<<(externalWav?"private_wav":"synthetic")
                 <<" sample_rate="<<sampleRate
                 <<" frames="<<inputFrames
                 <<" latency_samples="<<reportedLatency
                 <<" sample_peak_dbfs="<<dB(metrics.samplePeak)
                 <<" reconstructed_peak_dbtp="<<dB(metrics.reconstructedPeak)
                 <<" rms_dbfs="<<dB(rms)
                 <<" crest_db="<<dB(metrics.samplePeak/rms)
                 <<" side_mid_db="<<width
                 <<" wall_seconds="<<renderSeconds
                 <<" audio_seconds="<<totalFrames/sampleRate
                 <<" cpu_percent_one_core="<<100.0*dspSeconds/(totalFrames/sampleRate)
                 <<'\n';
        return EXIT_SUCCESS;
    } catch(const std::exception& e) {
        std::cerr<<"VST3_HOST_RENDER FAIL: "<<e.what()<<'\n';
        return EXIT_FAILURE;
    }
}
