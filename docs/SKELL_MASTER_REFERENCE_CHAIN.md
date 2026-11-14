# SKELL MASTER — user master-chain reference (2026-11-14) 🩻

## Ground-truth source (private; do not commit media or the full .fst)
The user provided **one FL Studio 26 `master.fst` mixer-track preset**, four screenshot views and two stereo 44.1 kHz / 24-bit / 18.75-second WAV excerpts (usual master and without usual master). The binary FL Studio file was decoded as FLhd / FLdt event records; four separate Fruity Wrapper instances each contain a Waves VST3 v15 preset XML with `ActiveSetup=SETUP_A` and `<Bypass ... Bypass="0" />`. The values below describe the active A snapshot, *not* the inactive B variant.

**Order (not interchangeable):**
1. `comp` — **Waves API-2500** compressor
2. `sauce` — **Waves Vitamin** five-band enhancer
3. `limiter` — **Waves L2** Ultramaximizer
4. `sauce2` — **Waves API-550A** EQ

### Verified reference controls

| Processor | File / screenshot-supported values | Notes |
|---|---|---|
| API-2500 | Preset name `Mastering 1`; MED knee, NORM thrust, OLD type; approximate knob positions around 1 ms attack, 4:1 ratio, 0.5-second release; analog ON; output near 0 | Exact mapping of additional legacy Waves XML parameter indices remains **unverified**. Do not treat visually estimated values as exact serialized thresholds. |
| Vitamin | Crossover frequencies **90 / 500 / 2,594 / 9,310 Hz**; five band faders **33.6 / 29.9 / 28.1 / 26.2 / 24.0**; five width displays **1.00**; Punch **3.0**; Direct **0.0**; screenshot TRIM **−8.5** | Raw band fader numbers are **not dB EQ shelf gains**. These parameters drive multiband enhancement/excitation; don't substitute them for an EQ curve. |
| L2 | Threshold **−2.8 dB**; Out Ceiling **−1.1 dB**; release display **1.00**, **ARC enabled**; 24-bit, Type1 dither, Normal shaping | The WAV file's observed output may include other upstream/downstream gain or clipping. L2 ceiling is not proof of true-peak compliance. |
| API-550A | LF frequency **50 Hz**, LF gain **0 dB**; MF **0.4 kHz**, MF gain **0 dB**; HF frequency **5 kHz**, HF gain **+2 dB**; analog OFF, output knob around 0 | The screenshot's small `−1.2` is a displayed readout; it must **not** be assumed an output setting without further validation. This processor is **after** L2. |

### Actual audio comparison, from previously documented private offline analysis
- Without-chain vs with-chain integrated loudness: **−10.09 vs −7.66 LUFS**, **+2.43 LU**.
- Global RMS crest decrease about **1.61 dB**.
- After normalizing for the integrated-loudness difference, relative spectral energy around **−1.2 to −1.5 dB from 20–120 Hz**; around **+1.5 dB from 4–16 kHz**.
- Side/mid RMS ratio approximately **+1.04 dB** after mastering.
- Both WAVs hit 0 dBFS sample peak and previously measured >0 dBTP reconstructed peaks: preserve target **tone/density**, *not* unsafe peak overs.
- The processors' frequency-dependent phase/group delay and harmonics prevent assuming a single lag or null alignment.

### Comparison to present Skell Master architecture
- Current SKELL MASTER factory snapshot uses existing stable APVTS IDs: Input +4 dB, Drive 8%, Glue 17%, Dynamics 11%, Lows −1 dB, Highs +3 dB, Sauce 20%, Transients 0%, Output 0 dB, Ceiling adjust 0 (=−1 dBFS sample ceiling), CLIP 14%.
- **Current audio order:** input → stereo-linked Drive → pre-compression low/high shelves (115 Hz / 8.2 kHz) → Glue/Dynamics → Transients → `SAUCE` high sheen/width → soft CLIP → lookahead limiter → final sample-peak guard. It is **not equivalent to** API-2500 → 5-band Vitamin → L2 → post-limit API-550A.
- In particular, current `highs` is a *pre-limiter 8.2 kHz shelf*, while the reference has +2 dB around **5 kHz after** L2. Current `SAUCE` is not the Waves five-band harmonic enhancer; current Glue/Dynamics are not known to reproduce API-2500 attack/release/sidechain.
- **Do not silently change existing DSP, old presets, parameter IDs or state formats to force this match.** A separate, explicitly compatible and persistent reference-sound architecture is a possible future proposal if the verified VST3 render proves existing settings insufficient.

### Required next QA before approving a sound-changing DSP revision
1. Fix all existing CTest/real-JUCE regression failures first.
2. Render the *actual* Windows VST3 with current SKELL MASTER on the provided unmastered excerpt, not a Python approximation; independently compare loudness, 50–100 ms envelopes, spectral bands, transient headroom, stereo response and intersample peaks against reference.
3. Only if deficiencies are verified, prototype original, real-time-safe 5-band coloration and post-limiter 5-kHz tone, keeping all prior projects identical by default and testing CPU/latency. Perform lossless, loudness-matched real-music A/B.
4. Check the original FL Studio master export path if exact low-level output or 24-bit dither characteristics matter. Do not ship certified TP claims based on sample-peak guards.
5. Leave PR draft until tests on the **same HEAD**, Windows FL Studio compatibility and human listening are passed.

This document records supplied evidence and a **target**, not a claim that the present DSP exactly reproduces copyrighted/proprietary plugin algorithms or the rendered reference.
