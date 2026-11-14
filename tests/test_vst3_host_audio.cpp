// Windows VST3 host audio regression. Loads the INSTALLED/BUNDLE VST3 through
// JUCE's standard VST3 format instead of linking a processor directly.
// Optional command: SkellyVST3HostAudioTests <bundle.vst3> <input.wav> <output.wav>
// With no WAV, a deterministic synthetic transient/bass stereo program is used.
// Never commit private user's recordings; local WAV comparisons stay local.
#include <JuceHeader.h>
#include "../Source/DSP/MasteringEngine.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

static void require(bool condition, const char* why) {
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

static void setParameterByName(juce::AudioPluginInstance& instance,
                               const juce::String& name, float actual) {
    bool found=false;
    for (auto* param : instance.getParameters()) {
        if (param->getName(128).equalsIgnoreCase(name)) {
            const auto value=param->getValueForText(juce::String(actual,2));
            require(std::isfinite(value),"host param conversion nonfinite");
            param->beginChangeGesture();
            param->setValueNotifyingHost(value);
            param->endChangeGesture();
            found=true;
        }
    }
    require(found,"Cannot find VST3 parameter by expected public name");
}

static juce::File locateVST3(const juce::String& userArg) {
    if (userArg.isNotEmpty()) return juce::File(userArg);
    const auto root=juce::File::getCurrentWorkingDirectory();
    const auto build=root.getChildFile("build");
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
        const juce::File bundle=locateVST3(argc>=2?juce::String::fromUTF8(argv[1]):juce::String());
        require(bundle.exists(),"VST3 path does not exist");
        juce::AudioPluginFormatManager formats;
        formats.addDefaultFormats();
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
        const bool externalWav=argc>=3;
        if(externalWav) {
            const juce::File input(juce::String::fromUTF8(argv[2]));
            require(input.existsAsFile(),"User WAV input not found");
            reader.reset(audioFormats.createReaderFor(input));
            require(reader!=nullptr,"Could not read WAV input");
            require(reader->numChannels==2,"Reference WAV must be stereo");
        }
        const double sampleRate=reader ? reader->sampleRate : 44100.0;
        const int blockSize=257;
        juce::String error;
        auto plugin=formats.createPluginInstance(*descriptions.getFirst(),sampleRate,
                                                 blockSize,error);
        if (!plugin) throw std::runtime_error("Cannot instantiate built VST3: " + error.toStdString());
        plugin->setPlayConfigDetails(2,2,sampleRate,blockSize);
        plugin->prepareToPlay(sampleRate,blockSize);
        require(plugin->getLatencySamples()>=0,"Negative VST3 latency");
        const int reportedLatency=plugin->getLatencySamples();

        // Real hosted VST3 automation, no private/mutable factory preset data.
        // These are the already-public values of the current SKELL MASTER
        // reference preset. Existing preset identities/IDs are untouched.
        setParameterByName(*plugin,"Input",4.0f);
        setParameterByName(*plugin,"Drive",8.0f);
        setParameterByName(*plugin,"Glue",17.0f);
        setParameterByName(*plugin,"Dynamics",11.0f);
        setParameterByName(*plugin,"Lows",-1.0f);
        setParameterByName(*plugin,"Highs",3.0f);
        setParameterByName(*plugin,"Sauce",20.0f);
        setParameterByName(*plugin,"Transients",0.0f);
        setParameterByName(*plugin,"Output Trim",0.0f);
        setParameterByName(*plugin,"Ceiling Adjust",0.0f);
        setParameterByName(*plugin,"Clip",14.0f);

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
        std::vector<float> output;
        if(argc>=4) output.reserve(static_cast<size_t>(totalFrames)*2);

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
            plugin->processBlock(buffer,midi);
            for(int i=0;i<count;++i) {
                const float l=buffer.getSample(0,i),r=buffer.getSample(1,i);
                metrics.add(l,r);
                if(argc>=4) {output.push_back(l);output.push_back(r);}
            }
        }
        const double renderSeconds=std::chrono::duration<double>(
            std::chrono::steady_clock::now()-started).count();

        if(argc>=4) {
            const juce::File outputPath(juce::String::fromUTF8(argv[3]));
            auto stream=outputPath.createOutputStream();
            require(stream!=nullptr,"Could not create WAV output");
            auto* wav=audioFormats.findFormatForFileExtension("wav");
            require(wav!=nullptr,"JUCE WAV writer unavailable");
            auto writer=std::unique_ptr<juce::AudioFormatWriter>(
                wav->createWriterFor(stream.get(),sampleRate,2,24,{},0));
            require(writer!=nullptr,"Could not create output WAV encoder");
            stream.release(); // Writer owns stream
            juce::AudioBuffer<float> audio(2,static_cast<int>(totalFrames));
            for(int i=0;i<audio.getNumSamples();++i) {
                audio.setSample(0,i,output[static_cast<size_t>(2*i)]);
                audio.setSample(1,i,output[static_cast<size_t>(2*i+1)]);
            }
            require(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples()),
                    "Failed to write rendered WAV");
        }
        plugin->releaseResources();
        const auto dB=[](double linear) {
            return 20.0*std::log10(std::max(1.e-14,linear));
        };
        const double rms=std::sqrt(metrics.sumSquares/std::max<size_t>(1,metrics.count));
        const double width=10.0*std::log10(
            std::max(1.e-14,metrics.sideEnergy) / std::max(1.e-14,metrics.midEnergy));
        require(metrics.samplePeak<=std::pow(10.0,-1.0/20.0)+3.e-4,
                "Hosted VST3 escaped -1dBFS sample-peak guard");
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
                 <<" cpu_percent_one_core="<<100.0*renderSeconds/(totalFrames/sampleRate)
                 <<'\n';
        return EXIT_SUCCESS;
    } catch(const std::exception& e) {
        std::cerr<<"VST3_HOST_RENDER FAIL: "<<e.what()<<'\n';
        return EXIT_FAILURE;
    }
}
