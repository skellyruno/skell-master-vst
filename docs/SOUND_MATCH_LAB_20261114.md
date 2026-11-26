# Sound-quality experiment — 2026-11-14 🩻

## Purpose and compatibility
Improve **Skell Master’s entire audio chain** using the user's real unmastered and
custom-mastered stereo reference. Treat the supplied audio and FL Studio
`master.fst` as private: **do not commit either recording or the binary preset.**
All existing factory programs, APVTS IDs, saved-state versions, JUCE appearance
and production audio processing remain unchanged in this experimental pass.
Neither experimental tone class is wired into the released VST3.

## Exact user reference architecture
The original FL Studio chain is **API-2500 compressor → Vitamin five-band enhancer
→ L2 limiter → API-550A EQ**. This is documented in
`docs/SKELL_MASTER_REFERENCE_CHAIN.md`; notably the final 5 kHz +2 dB boost
comes **after** L2, unlike Skell Master's existing 8.2 kHz pre-limiter shelf.

On the supplied 18.75 s, 44.1 kHz stereo WAVs, integrated loudness was
-10.094 LUFS dry versus -7.659 LUFS mastered (+2.435 LU); crest decreased
1.612 dB; side/mid increased 1.047 dB. Both references have reconstructed
peaks over 0 dBTP in the **diagnostic** 4× polyphase measurement, so their
overshoots are not an acceptable ceiling target.

## Experimental native DSP candidates
- `Source/DSP/FiveBandColorPrototype.h`: 90/500/2594/9310 Hz split, upgraded
  to two cascaded one-poles per crossover for better band isolation. Bands
  recombine exactly in the zero-color state. Stereo-linked rational gain;
  12 ms smoothing for wet and each band; no allocation in `process()`.
  Original filter is *not* a Waves/Vitamin reproduction.
- `Source/DSP/PostLimiterPresencePrototype.h`: separate original RBJ 5 kHz
  bell, fixed +2.00 dB gain at its center, Q=0.9; 12 ms blend smoothing
  and exact zero-wet passthrough. This module is **NOT** in the production
  chain and would require a true-peak-safe final stage if integrated after
  limiting, especially with aggressive mastering.
- Portable CTest modules validate neutral exactness, mono/anti-phase, 60 Hz
  bass and 7 kHz high-band isolation, control transitions, hostile input and
  parameter values, slow silence recovery, CPU probes, and fixed post-EQ
  response at 176.4, 192 and 384 kHz.

## Real music, offline only — quantitative listening candidate
The uploaded source and mastered WAVs were analyzed and used to make a
**private offline** 15-second loudness-matched comparison. The candidate:
1. Processes the unmastered WAV through the *isolated C++* five-band prototype
   at 4× polyphase resampling (wet 0.8, band intensities 0.40/0.35/0.50/0.70/0.72).
2. Uses a separate offline, causal parametric tone stage (70 Hz -1.8 dB,
   360 Hz -0.65 dB, 1.8 kHz +0.22 dB, 5.4 kHz +2.4 dB,
   11.7 kHz +1.1 dB) and a modest 220 Hz high-passed width lift of 10%.
   These EQ filters are **not** the same as the unintegrated +2 dB 5 kHz
   production proposal above.
3. Tests an **offline stereo-linked**, 2 ms lookahead/55 ms release gain-control
   envelope with a -10.5 dBFS detection threshold, then matches the playback
   clip to -14 LUFS. This stage is **not** the JUCE VST3 limiter; its gain
   recovery and sound have not yet passed real FL Studio A/B listening.

Measurements on the 15-second -14 LUFS FLAC comparison:
| Metric | User master reference | Experimental candidate |
|---|---:|---:|
| RMS crest | 7.66 dB | 8.35 dB |
| Side/mid ratio error | reference | +0.28 dB |
| 4× approximate reconstructed peak | -3.95 dBTP | -5.36 dBTP |

Equal-loudness relative-band error (candidate minus reference):
20–90 Hz **+0.55 dB**; 90–500 **+0.15 dB**;
500–2594 **-0.30 dB**; 2594–9310 **+0.04 dB**;
9310–19000 **-0.35 dB**. Mean absolute 50 ms RMS envelope
difference improved from approximately **1.15 dB** in the no-dynamics
candidate to **0.70 dB** in the revised candidate. A good frequency match is
not a proof of matching loudness, pumping, codec distortion or musical punch.

**The offline listening page and original recordings stay out of GitHub.**
No DSP improvement should be described as audible/preferred until a user has
heard the lossless, synchronized comparison.

## Verification status / next gates
- Local C++17 `-Wall -Wextra -Werror -pedantic` and UBSan checks of equivalent
  isolated prototype implementations passed. Fixed presence bell measured
  **+2.0000 dB at 5 kHz**; 100 Hz change ~0.001 dB;
  18 kHz change ~0.19–0.20 dB. Local five-band neutral, mono and antiphase
  checks passed, as did a steady-input smoothing step ~1.21×10^-5.
- The last fully confirmed Windows VST3 baseline was workflow #183. The new
  prototype/CTest commit must get its **own successful Windows build and tests**;
  do not reuse baseline results to claim the new HEAD passes.
- No screenshot CI was run for this experiment. The screenshot workflow is
  manual-only; do not reactivate routine screenshot CI.
- Before any activation: use an **actual VST3-host render** of the user's dry
  WAV and lossless loudness-matched A/B; measure 50ms/400ms envelopes,
  band balance, mono/side image, THD/aliasing, CPU at 44.1/48/96 kHz,
  fixed latency, true peaks and FL Studio preset/state recall.
- Do not silently change any old preset or inject the unverified algorithm
  into all users' existing audio. A new persisted opt-in mode, if selected
  later, must default disabled for old projects and be fully regression-tested.
