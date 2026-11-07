# SKELLY MASTER — M-01

A dark neon-green mastering effect built on your `skell-vst-template-main(1).zip` project: **JUCE 8 / C++17 / VST3 + Windows Standalone**, original vector UI, an automated mastering DSP chain and a Windows GitHub Actions build. No bitmap UI assets or third-party plugin binaries.

> **Engineering status:** portable C++ DSP tests are included. A successful Windows VST3/Standalone compile, compatibility with every DAW, mastering-quality subjective evaluation, peak compliance to BS.1770, and real-world listening tests must be independently verified before selling or claiming commercial flagship parity.

## UI refresh: premium vector faceplate (November 2026)

The interface is implemented in **JUCE C++** (not a rendered mockup or external bitmap).
- High-contrast studio rack faceplate, simplified visual hierarchy, balanced 4×2 control grid.
- Machined dark-metal rotary encoders with illuminated pointers and editable dB / percent values.
- Glass-look dual L/R output peak meters, gain-reduction readout, engine/bypass status.
- Compact Bypass / Reset / Output / Ceiling utility strip and fixed-aspect resizing.
- Original **star-eyed skull** emblem inspired by the supplied reference, without its text, implemented as scalable vector geometry in `Source/UI/StarSkullMark.h`.
- Static decorative artwork is cached at 2× resolution, so the 30 Hz meter refresh does not recreate the entire vector faceplate.

**All existing parameter IDs, DSP topology, DAW automation and preset data are preserved.**
The visual change has not been used as evidence of mastering-audio quality: use an external true-peak/LUFS analyzer and verify real projects before commercial distribution.
The Windows GitHub Action compiles VST3 and Standalone targets, runs the DSP tests, and uploads the VST3 bundle as a ZIP.

## Main controls

| Control | DSP function | Range / default |
| --- | --- | --- |
| INPUT | Smoothed pre-chain gain | −18…+18 dB / **0** |
| DRIVE | Smoothed non-linear harmonic saturation (processed at 4× rate) | 0…100% / **0** |
| GLUE | Linked slow bus compression, soft knee | 0…100% / **0** |
| DYNAMICS | Linked faster level control, soft knee | 0…100% / **0** |
| LOWS | 115 Hz second-order low-shelf | −6…+6 dB / **0** |
| HIGHS | 8.2 kHz second-order high-shelf | −6…+6 dB / **0** |
| SAUCE | HF saturation and subtle stereo-side widening | 0…100% / **0** |
| TRANSIENTS | Dual-envelope attack enhancement | 0…100% / **0** |
| OUTPUT | Post-chain gain **before** limiter | −18…+12 dB / **0** |
| CEILING | Oversampled peak limit / safety clamp | −3…−0.1 dBFS / **−1** |
| BYPASS | 12 ms crossfade to latency-aligned dry audio | off |

The editor includes **dual analog-inspired output peak meters**, peak gain reduction readout, double-click knob reset, global reset, MIDI/DAW automation through APVTS, session state save/restore, mono/stereo operation and resizable fixed-aspect UI. The DAW reports the fixed 4× oversampling-filter latency plus **1.5 ms lookahead** (rounded to host samples), rather than hiding it.

## Real DSP architecture

1. Input gain → continuously blended odd-harmonic soft saturation → DC cleanup.
2. Biquad low/high shelves with parameter smoothing.
3. Separate stereo-linked slow glue and fast dynamics compressors with 6 dB knees.
4. Stereo-linked transient enhancement from fast and slow onset envelopes.
5. Band-isolated harmonic coloration and mild mid/side width (SAUCE).
6. Output trim → 4× oversampled lookahead limiter with 0.2 dB safety margin → sample-safety clamp.
7. 12 ms crossfaded, latency-matched bypass; never use the audio thread for file access or heap allocation.

**True-peak limitation:** 4× oversampling reduces inter-sample peak risk but this is **not yet a certified IEC/ITU true-peak limiter**; additional host-rate sample safeguarding guarantees an output **sample-peak** ceiling when bypass is off, but not mathematical reconstruction-peak compliance between output samples. There is no loudness-normalized automatic makeup, auto-mastering AI, multiband compressor, dithering, or LUFS analyzer in this version. Verify renders and loudness in a dedicated loudness/TP analyzer.

## Download-ready Windows compilation via GitHub

1. Create a GitHub repository (for example `skelly-master-vst`).
2. Upload **all of this zip's project contents**, including hidden `.github`.
3. Select **Actions → Build Skelly Master (Windows VST3 + Standalone) → Run workflow**.
4. The workflow fetches JUCE 8.0.12, builds on Windows x64, runs DSP regression tests, and uploads a VST3 bundle ZIP. The Standalone target is also compiled but is not included in this artifact.
5. Extract the artifact ZIP. Install the **entire** `.vst3` directory into `C:\Program Files\Common Files\VST3\` and rescan plugins in FL Studio. Standalone runs outside your DAW.

**Local with installed JUCE 8:**
```powershell
cmake -S . -B build -A x64 -DJUCE_PATH="C:/JUCE"
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

**Local with automatic JUCE fetch:** remove the `-DJUCE_PATH` option; requires network access to GitHub. `JUCE` licenses and the plugin's own commercial licensing still require review before commercial distribution.

## Notes for mastering

Start with all 8 macro controls at zero. Increase **GLUE** gradually to control dynamics, then **DYNAMICS** for a denser result, use LOWS/HIGHS for subtle ±0.5–1.5 dB corrections, and increase DRIVE/SAUCE/TRANSIENTS only as needed. Use CEILING at −1 dBFS or below and verify exported inter-sample peak externally. **0% GLUE** and **0% DYNAMICS** are bypassed in the detector/gain logic; with all eight macros at zero the audio remains unchanged except for necessary lookahead and oversampling filter characteristics and limiting if the signal exceeds the set ceiling.

## Files

- `Source/PluginProcessor.*` — JUCE host integration, parameters, oversampling, latency, meters.
- `Source/DSP/MasteringEngine.h` — portable DSP processing core; CPU safe, no allocations in its audio callback.
- `Source/PluginEditor.*` — original brushed-metal/rack-meter visual GUI drawn in C++.
- `tests/test_mastering.cpp` — C++ DSP regression smoke tests.
- `.github/workflows/build.yml` — Windows VST3/Standalone CI and artifact release.
