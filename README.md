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

## November 7 FL Studio polish and DSP revision

- Default window is now **702×445** (35% smaller in width and height than the previous 1080×684 default), still resizable at a fixed aspect ratio.
- Stereo meter pointers are constrained to the meter faces and the readouts mask the pivot, preventing needles extending below the glass.
- The decorative star-skull is smaller and moved away from the header tagline; redundant footer skull removed.
- Higher-contrast **CEILING** label, improved compact button and slider text sizing, and text-entry boxes which scale with the editor window.
- The final host-rate output stage now uses a **linked stereo sample-peak gain guard** (immediate gain reduction, 85 ms release), preserving L/R balance instead of hard-clipping each channel independently.
- The main 4× oversampled lookahead limiter now uses a program-dependent release (65–220 ms) to reduce abrupt volume recovery after large peaks.
- New portable C++ regression checks exercise the peak guard's ceiling, stereo ratio, release, and below-ceiling transparency.
- Build workflow cancels superseded push builds on `main` while preserving manual workflow dispatch.

**Not yet certified:** sample-peak compliance is not the same as validated true-peak limiting. Test real mixes and DAW automation, inter-sample peaks, LUFS, THD, CPU and latency before shipping as a commercial mastering suite.

## November 7 — next UI and mastering DSP pass

- Input value displays now use softer rounded recessed plates with improved legibility at the compact 702×445 default.
- Increased the readable size of selected captions and meter markings while retaining the 35%-smaller default and resizability.
- The 30 Hz meter timer invalidates only live meter/status areas instead of repainting the entire editor, reducing avoidable GPU/GUI work.
- **SAUCE stereo enhancement** now widens mostly mid/high side information above a ~220 Hz side crossover instead of boosting bass-side information indiscriminately.
- The imager tracks mid/side energy and reduces added width when the signal is strongly anti-correlated, making negative-phase material less vulnerable to worsening.
- Output gain-reduction metering includes the host-rate safety stage as well as the upstream compressors/limiter.
- Added portable regression tests for bass preservation, high-frequency width, anti-phase protection and exact 0% neutrality.

This is an incremental quality and performance pass. **It does not claim certified true-peak performance, LUFS metering, real-time CPU benchmarking or subjective listening validation.** Test in FL Studio before publishing as a commercial release.

## November 7 — precision / CPU polish

- **DRIVE shaper:** precalculates its normalization curve during `prepare()`, eliminating one expensive `tanh` call per oversampled stereo frame while matching the previous saturation transfer within 1e-6 in dedicated tests.
- **Limiter recovery:** holds peak attenuation for the configured lookahead interval, preventing recovery before the corresponding delayed transient emerges.
- **Analog meters:** 1.5-second amber peak-hold pointers with graceful decay, without adding a new control or DAW parameter.
- **Compact readability:** slightly larger editable-value fonts and ASCII-safe hardware legends (including `4X OVERSAMPLING`).
- **Automation and projects:** unchanged parameter IDs, ranges, state format and factory defaults; no CPU-heavy analysis added to the audio callback.

The VST3 build and tests confirm buildability, not certified reconstructed true-peak limiting or sound-quality equivalence to third-party commercial master processors.

## Hardware-look revision — original control layout preserved

This release focuses exclusively on the existing editor's visual materials.
It does **not** relocate the 8 primary mastering controls, dual output meters,
gain-reduction display, output/ceiling controls, or bypass/reset buttons.
The compact 702×445 default, resizing, parameters, presets and DSP are unchanged.

- Anodized dark-steel rack panels with directional brushed highlights and machined borders.
- Raised metal knob collars, shallow grip-knurl engraving, brushed faceplate highlights and consistent neon pointer illumination.
- Smoked-glass stereo meters with bevels and a restrained lens reflection.
- Tactile, inset hardware-style switches, recessed readout windows, and steel screws.
- Dedicated gain-reduction meter bezel with consistent faceplate materials.
- All artwork remains procedural JUCE vector drawing with no external bitmap or font dependencies.

The Windows VST3 workflow verifies compilation and portable DSP tests. Visual appearance and mouse interaction still require an FL Studio check before release.

## November 10 — robustness tests

- Sanitizes non-finite audio at the engine and final peak-guard boundaries, preventing a NaN or infinity from corrupting persistent EQ/compressor/limiter state.
- Explicitly zeros sub-audible filter/envelope tails to prevent denormal CPU spikes on long silence.
- Adds a busy-chain CPU probe that reports measured processing time relative to processed audio duration at the 4× internal rate; broad failure guard catches extreme regressions but is not a hard real-time guarantee.
- Portable regression tests now inject non-finite samples, exercise recovery, and check lowpass denormal tails.
- Existing IDs, preset state, GUI layout and all mastering controls are unchanged.

## Windows UI and host integration verification

The Windows workflow runs a JUCE offscreen renderer and a host-like processor
smoke test in addition to portable DSP tests. It checks **five editor sizes**
(620×393 through 1800×1140), verifies no top-level knob/button bounds escape or
overlap, validates that input/bypass/reset controls update parameters, and saves
two actual rendered PNG snapshots in the `SkellyMaster-UI-Regression` artifact.
Host tests process 44.1/48/96 kHz sample streams with variable block sizes,
check sample-peak ceilings, finite output, and sane reported latency.

This improves automated UI validation but is **not** a replacement for FL Studio
host compatibility, visual human review, loudness/true-peak measurements, or
real-song listening tests. The GUI's visual design and DSP parameter IDs are
unchanged; resizing no longer recreates every text-entry box unnecessarily.

## CI warning cleanup

Removed the obsolete `JUCE_DISPLAY_SPLASH_SCREEN` compile definition that JUCE 8
explicitly ignores, and updated official GitHub checkout/artifact actions to
their Node 24 versions to avoid obsolete Node 20 warnings. Both artifact
categories now have 14-day retention.

## November 10 — screenshot-driven finishing details

Reviewed the **actual JUCE rendered screenshots** at 620×393, 702×445 and 1200×760,
not concept images. Shifted the bottom-right M-01 badge clear of the decorative
metal screw and increased legibility of the manufacturer, control subtitles and
master bus tagline at compact sizes. Layout and DSP remain otherwise unchanged.

## November 10 — host input safety

- Host-rate samples are validated **before** the JUCE 4× oversampling filters.
  Non-finite inputs are replaced with silence so they cannot corrupt the
  oversampler's filter state; extraordinary signals above ±256 linear
  (+48.2 dBFS) are bounded as defensive protection.
- The JUCE integration regression now injects NaN, positive/negative infinity
  and extreme finite values at 44.1, 48 and 96 kHz, then checks output stays
  finite, below the configured sample ceiling, and recovers normal energy.
- Nominal program material, 702×445 window, parameter IDs, and preset formats
  are unchanged.

This is sample-peak safety, not certified inter-sample true-peak limiting.
Real projects and DAW host validation remain mandatory for a commercial
mastering release.

## November 10 — idle graphics and profiling

- The 30 Hz meter timer only invalidates the meter bridge when the peak
  readouts, peak-hold cursors, or reduction readout actually change.
  Thresholds compare against the last submitted paint state, not the previous
  30 Hz tick, preserving gradual cursor release and the final zero readout.
  Bypass-status repaint is handled separately, so idle audio doesn't redraw
  the premium hardware artwork repeatedly.
- CI uses verbose CTest output to expose the existing busy-chain CPU timing
  probe and the integration-test pass details for each sample rate.
- The visual design, control rectangles, parameter automation IDs and DSP
  processing algorithm are unchanged by this GUI optimization.

## SkellCore headless JUCE renderer (permanent development gate)

Skelly Master uses the **shared SkellCore real-editor screenshot harness** from
[skell-vst-template](https://github.com/skellyruno/skell-vst-template), pinned
to renderer-tested Git commit `5ac8e885f1563f804c66072797b7038bc2939fae`.
CMake exposes an opt-in `SKELLY_MASTER_BUILD_UI_RENDERER` target linked with
`SkellCore::UIRenderer`, the plugin's actual Processor and Editor, and JUCE.
This development executable is **not part of the released VST3**.

The [SkellCore screenshot workflow](https://github.com/skellyruno/skell-master-vst/actions/workflows/ui-screenshots.yml)
runs on every relevant UI/source push, pull request and manual dispatch.
Linux/Xvfb captures 10 actual editor states: minimum (620×393), default
(702×445), medium (960×608), reference (1200×760), maximum
(1800×1140), bypass at two sizes, plus active DRIVE, EQ, and full-mastering
settings. It validates bounds, control overlap, scene differences and output
image entropy. The artifact includes `manifest.json`, all 10 PNGs and a
six-panel `contact-sheet.png`.

Local Linux usage:

```sh
cmake -S . -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKELLY_MASTER_BUILD_UI_RENDERER=ON
cmake --build build-ui --target SkellyMasterUIRenderer --parallel 3
xvfb-run -a ./build-ui/SkellyMasterUIRenderer --output ui-screenshots
```

Skelly Master currently has **one hardware UI skin and one control page**.
It has no custom preset-browser, additional tabs or plugin-created overlay.
Native popup menus and actual pointer hover still need host/interactive checks;
unsupported UI states are not fabricated for screenshots.

**Workflow rule:** Any meaningful editor changes must pass
**build → render real editor → inspect PNGs → fix → render again**.
Windows VST3/host regression tests and DSP tests remain independent gates.

## SkellCore verified visual polish — adaptive hardware UI cache

Inspection of the actual SkellCore 620×393, 702×445 and 1800×1140
captures found the control arrangement and hardware rendering intact, but
editable values stayed relatively small on very large windows. Their label
font and editable text-box caps now scale with the editor.

The static brushed-metal faceplate is cached at a **quantized, adaptive**
resolution instead of always allocating a 2400×1520 ARGB image. At the
702px default, the target is approximately 1152×730, a reduction of roughly
77% in cached-faceplate memory per open plugin window. This runs exclusively
on the GUI thread when needed and never reallocates from the audio callback.
All parameter IDs, DSP, meter behavior, theme and hardware layout are
preserved. SkellCore screenshot CI must pass again before release.

## November 10 — visual QA revision

Visual review of actual SkellCore screenshots found overly regular chipped
metal strips and a missing preferred serif font on Linux. The wear is now
irregular and less aggressive, the dial stamp is smaller, and editable
readouts and captions explicitly select Georgia on Windows / DejaVu Serif
in CI. Both remain ordinary system fonts: no font assets are embedded.
All functional controls and the audio engine are untouched.

## November 10 — emerald vintage rack UI (actual JUCE)

Reference-directed hardware design, with **all red lacquer replaced by deep
emerald green**. Keeps the master plugin's existing 10 controls, bypass/reset,
VU and gain-reduction metering, parameters, automation, preset identity and
DSP unchanged.

The live, compiled JUCE editor now renders worn iron and chipped green enamel
plates, brass screws and surrounds, aged-gold skull insignia, engraved
serif-inspired typography, dual amber analog-style VU meters with skull dial
stamps, gold mechanical knob indicators, recessed value plates and brushed
side handles. Procedural patina is deterministic, built into the adaptive
GUI-only backdrop cache. The meter/knob readouts remain live, draggable and
attached to real APVTS parameters — **the supplied reference picture is NOT
used as a static UI background**.

After changes, the pinned SkellCore renderer captures actual Windows-equivalent
JUCE editor states on Linux/Xvfb at five size classes and different automation
settings, then the independent Windows workflow compiles the VST3 and runs
portable DSP and JUCE host integration tests. Hardware materials are decorative
and are not processed on the audio thread.

## Photographic black-metal and emerald patina

The two largest surface materials now tile **real photographed black iron
grain and green enamel patina** from the supplied reference, at a restrained
opacity under the existing engraved trim and patina code. Only green top and
bottom plates use green; the chassis, rack face, and central hardware remain
black. The material layer is rendered into the cached static backdrop on the
GUI thread. Tests verify all four embedded resources decode before rendering.

## Reference-derived meter face

A compact JPEG asset reproduces the vintage amber VU dial's scale, skull stamp,
aged-gold bezel and paper grain. The meter's displayed needle is still
calculated and drawn from the **actual left/right output levels**, while
its dBFS readout and peak-hold marker remain real-time JUCE graphics.
A full procedural fallback is retained. Both the dial and photographic
rotary face are embedded as JUCE BinaryData; there is no external asset
loading when the plugin runs.

## Photographic hardware asset pass

The original reference knob has been extracted as a small, transparent indexed
PNG and compiled into the VST3 through JUCE BinaryData. The **actual JUCE
sliders remain fully interactive, automatable, resizable and attached to the
existing APVTS IDs**; only the knob face is a photographic sprite. The software
rotates its gold pointer according to the real parameter value. The analog
ring, text, readouts, meters, and heavy green/black plates remain JUCE-drawn.
A procedural vector fallback remains available if the photo cannot decode.

The design preserves a black chassis, emerald green enamel nameplate and
bottom strip, aged-gold trim, and existing control positions. This is the
first of the image-asset iterations; it does not change DSP or VST3 identity.

## Hardware accuracy pass — knob, chassis, enamel, meters and regression

The photographic rotary cap has been scaled to fill its fixed engraved
graduation collar and calibrated to the reference's brass pointer. Rotation
still follows real APVTS values. The iron housing uses darker neutral-black
metal, stronger reference-derived photographic grain, and nonrepeating chipped
edges, while **emerald enamel remains limited to the title plate and lower
master-bus strip**. The amber VU meter receives a low-opacity glass reflection,
a machined inset rim and a brighter live needle. All changes are GUI-only,
with unchanged controls, DSP, automation IDs, latency and preset states.

The regression workflow now checks the photographic knob's transparent
corners/aspect, amber dial resolution, four image decodes, five editor sizes,
controls and the existing audio/CPU suite. SkellCore screenshots remain the
authoritative actual-editor visual inspection output.

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
| CLIP | New v7: 4× oversampled pre-limiter clipping; **35%=Soft Clip Pro 11% saturation / -1.2 dB threshold / 0dB input gain**; 100%=harder clip | 0…100% / **0** |
| BYPASS | 12 ms crossfade to latency-aligned dry audio | off |

### New opt-in factory: 5-PLUGIN MATCH (ID 6)

The first five factories (IDs 1..5) retain their **published parameter values**
and names. A sixth selectable preset, **5-PLUGIN MATCH**, is added using
internal saved factory identity `6` and menu item ID `10`, without
renumbering the existing factory IDs or non-preset menu actions 6..9.
Fresh selection explicitly enables the v7 clip-before-L2 topology.

The new preset uses the original SKELL MASTER settings except
**HIGHS=3%/dB control** (up from published factory5's 2), **CLIP=35%**
(up from 14%). Its source-calibrated final API-style EQ is an
**opt-in +0.8dB broad 1.4kHz bell** after the native limiter and before
the host-rate true-peak safety stage. This bell is activated by saved
factory identity 6 only and is neutral for factory IDs 0..5. It has
no extra host latency, requires no host parameter IDs, tracks filter
history while inactive and crossfades the wet correction over 12ms.
The settings remain editable with the existing 11 stable APVTS knobs;
`*` indicates an edited factory and REAPPLY CURRENT restores ID6.

Why: a private 99.648s owner four-plugin master baseline matched the
latest clip-0 v7 render in crest (7.31 vs 7.29dB) while loudness was
about 1.75LU lower due largely to Skell's preserved -1.5dB ceiling.
After LUFS matching, the earlier Skell clip-0 spectrum was roughly
0.6dB low in the 500-2594Hz band and 0.7dB low in the highest band.
A tested +1 step to the HIGHS control addressed the air gap without
overwriting the old factory; the final API-like bell is the separate
mid-band correction hypothesis. Audition and independent final-WAV
spectral measurement still determine its suitability.

Older saved state: v1-v6 factory IDs remain capped at 5, and
v7 at 6, so malformed historical values cannot accidentally enable new
factories. State v8 supports factory IDs7..9 while preserving the v7
reference-clip placement property, selections and parameter automation.

### Additive factory voicing: WARM MASTER+, PUNCH MASTER+, LOUD MASTER+ (v8)

**No changes** to the existing published factory IDs 1..5, the existing
5-PLUGIN MATCH factory ID6, any APVTS automation IDs, any parameter ranges,
or the previous v6/v7 saved-session sounds. Three additional, opt-in
factories have internal saved IDs **7, 8, 9** and menu IDs **11, 12, 13**.
The menu action IDs 6..9 remain reserved and unchanged.

- **WARM MASTER+** (ID7): INPUT0, DRIVE15, GLUE15, DYNAMICS8,
  LOWS+2dB, HIGHS-1dB, SAUCE20, TRANSIENTS0, OUTPUT0, CEILING0, CLIP0.
  On an eight-second source excerpt it adds ~1.69dB of low/mid energy
  relative to the published WARM, for a fuller, darker character.
- **PUNCH MASTER+** (ID8): INPUT-2dB, DRIVE8, GLUE6, DYNAMICS4,
  LOWS0, HIGHS+1dB, SAUCE0, TRANSIENTS60, OUTPUT0, CEILING0, CLIP0.
  It preserves ~0.46dB more crest factor than original PUNCH on the
  same exact source excerpt while sacrificing ~0.45dB of RMS level.
- **LOUD MASTER+** (ID9): INPUT0, DRIVE12, GLUE14, DYNAMICS11,
  LOWS0, HIGHS+1dB, SAUCE8, TRANSIENTS8, OUTPUT+3dB,
  CEILING0, CLIP25. This version provides ~2.75dB more RMS than the
  published LOUD on the same excerpt, but with **stronger peak shaving**
  (crest ~3.79dB vs 6.54dB). Use deliberately for density rather
  than transparent mastering; match levels when auditioning.

Measurements use the actual **compiled PR35 Linux VST3**, the owner's
8s private unmastered excerpt and the same sample-rate/clip region for
all settings. They are **not** general loudness guarantees or a claim
that any preset exactly recreates a proprietary processor. The owner
audio remains local/private and off GitHub.

State version **8** supports IDs7..9. Loading an older v7 session
still caps its factory identity at6; v1-v6 identities still cap at5,
even if malformed data says otherwise. Selecting the new v8 factories
explicitly enables the modern 4x chain-ordered clipping topology.
Individual controls remain exposed under their original 11 automation
IDs; no extra parameter/CPU or host PDC is added simply by adding
factory choices. Host regression runs all 9 factory states, checks all
11 knob names produce a real VST3 audio change, and validates
selection/edit-marker/reapply/host-state recall.

*Engineering note:* the 11-knob extreme-value sweep on this reference
excerpt verified live DSP for every control, but full 100% GLUE and
DYNAMICS are intentionally very aggressive compressor settings.
Their nonlinear loudness behavior should be auditioned, not mistaken
for a linear 0..100% makeup knob. Preserve the existing control mappings
in old projects until an explicit versioned alternative is A/B tested.

### Owner's five-plugin master reference and compatibility

The actual FL Studio `.fst` and six plugin screenshots resolve the source
chain order to **API-2500 → Vitamin → StandardCLIP → L2 → API-550A**. The
master-only WAV excludes the StandardCLIP processing, so it remains a
separate baseline for the four-plugin mastering sound.

**Actual StandardCLIP snapshot:** Soft Clip Pro, 11% Soft-Clip Saturator,
clipping at **-1.20 dB**, 8x oversampling, 0dB input and output gain, and
Ceiling **Off**. The separate Waves L2 settings show threshold **-2.8 dB**
and output ceiling **-1.1 dB**. These are source-derived settings, NOT a
claim that StandardCLIP input gain is +2.23 dB. That previous number was
a fitted gain for the *combined output difference* and could not identify
the actual input gain of the plugin.

- For **new v7 instances**, Skelly's existing CLIP knob at **35%** is a
  reference-style original nonlinear saturator (11% nominal cubic
  intensity) into a smooth quadratic clipping shoulder at -1.2dBFS.
  The clip stage is **before** the lookahead limiter, following the
  owner's actual mixer order; 0% is neutral and higher settings have
  progressively deeper saturation/lower clipping thresholds. It is
  **not** proprietary SIR DSP or a sample-exact clone.
- The L2 screenshot's **-2.8dB threshold** and **-1.1dB output ceiling**
  imply about **+1.7dB input drive** into the L2 limiter. For new v7
  chain-ordered sessions with Deuce Match enabled, Skell applies that
  makeup **before** its lookahead limiter and removes the old post-limiter
  +2.4dB boost. This avoids double-limiting the same mastered peaks.
  Restored v6 and older saved states keep their original post-stage makeup.
- New chain-order clipping still uses the existing **4x processing** and
  final host-rate true-peak guard. The screenshot's **8x** StandardCLIP
  oversampling is **not yet matched**. Do not call the current sound an
  8x emulation; extra band-limited oversampling requires explicit
  latency, CPU and alias-rejection tests before implementation.
- State version **7** adds a *non-automatable* `skellMasterClipPlacement`
  flag. Restoring **v6** Reference CLIP saved states keeps their original
  **post-limiter** sound. Restoring pre-v6 states retains the legacy
  pre-limiter CLIP signal path. The menu action **USE 35% CHAIN CLIP**
  explicitly opts an older v6 state into the new order. Factory preset
  identities and APVTS parameter IDs, order, and ranges remain unchanged.
- The Waves Vitamin multiband contour (crossover points
  90/500/2594/9310Hz) and final API-550A post-L2 EQ are separate
  four-plugin mastering-core matching tasks. Screenshots alone do not
  validate a completed sound match. FL Studio state/automation roundtrip,
  latest real editor screenshots, and subjective audition remain necessary.

The editor includes **dual analog-inspired output peak meters**, peak gain reduction readout, double-click knob reset, global reset, MIDI/DAW automation through APVTS, session state save/restore, mono/stereo operation and resizable fixed-aspect UI. The DAW reports the fixed 4× oversampling-filter latency plus **1.5 ms lookahead** (rounded to host samples), rather than hiding it.

## Real DSP architecture

1. Input gain → continuously blended odd-harmonic soft saturation → DC cleanup.
2. Biquad low/high shelves with parameter smoothing.
3. Separate stereo-linked slow glue and fast dynamics compressors with 6 dB knees.
4. Stereo-linked transient enhancement from fast and slow onset envelopes.
5. Band-isolated harmonic coloration and mild mid/side width (SAUCE).
6. Output trim → 4× oversampled lookahead limiter with adaptive recovery and 0.2 dB headroom → linked host-rate sample-peak gain guard with a final numerical ceiling safeguard.
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
