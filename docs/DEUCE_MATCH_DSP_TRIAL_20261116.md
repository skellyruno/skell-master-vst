# SKELL MASTER — Deuce Match DSP trial (2026-11-16)

Status: draft engineering candidate; not merged, not a new approved sound champion.

## Reference evidence

The private full-length WAVs and FST files remain outside GitHub. This run re-extracted the existing self-contained 15-second lossless reference pages from the user's Library:

- `SkellMaster_Reference_AB_15s.html`: A = `deuce w beat.wav` (no master chain); B = `deuce my master preset.wav`.
- `SkellMaster_FiveBand_Reference_AB_15s.html`: A = established master reference; B = prior experimental five-band candidate.

Both contain 44.1 kHz stereo 24-bit FLAC excerpts, exactly 15 seconds. No raw master WAV or private FST was uploaded to this repository.

Recomputed measurements from the embedded FLACs:

| Measurement | No-chain source | Original master |
|---|---:|---:|
| Integrated loudness | -10.039 LUFS | -7.600 LUFS |
| Sample peak | 0.000 dBFS | 0.000 dBFS |
| 4x polyphase true-peak estimate | +2.234 dBTP | +2.467 dBTP |
| Crest (sample peak / RMS) | 9.284 dB | 7.659 dB |

The master is +2.439 LU louder. After loudness matching, its relative energy change is about -1.18 dB at 20–60 Hz, -1.49 dB at 60–120 Hz, +1.53 dB at 4–8 kHz, and +1.48 dB at 8–16 kHz. The old A/B page's 294-sample pre-shift leaves a measured residual alignment of about 263 samples (5.96 ms) in this excerpt.

The prior offline five-band candidate, measured at -14 LUFS, had a 0.36 dB mean six-band spectral error and about 0.52 dB 200 ms envelope error against its reference excerpt. It is an offline prototype render, not a native VST3 render and not perceptual approval.

## Production-path change in this draft

The named `SKELL MASTER` factory now enables a real, saved APVTS boolean `deuceMatch` (appended after existing IDs; default OFF). When enabled:

1. `FiveBandColorPrototype` runs in the existing 4x oversampled signal path at wet 0.75 with intensities {0.38, 0.55, 0.64, 0.72, 0.84}.
2. `PostLimiterPresencePrototype` applies the reference-derived +2 dB / 5 kHz bell after downsampling, before final output safety.
3. `StereoTruePeakGuard` replaces the production sample-only guard. It uses the existing 48-tap / 4-phase detector, 11 host-sample lookahead, stereo-linked gain, 85 ms release and 0.8 dB margin. Bypass keeps the same delay but does not attenuate.

The original four factory presets retain their parameter values and keep `deuceMatch=OFF`. Existing parameter IDs remain unchanged; `deuceMatch` is appended after `bypass`. State schema advances to v4, and V1/V2/V3 sessions receive an explicit neutral `deuceMatch=OFF` value. A UI regression exercises APVTS save/reload and the legacy missing-parameter default. The `ceiling` control now represents a true-peak target, with an internal 0.8 dB reconstruction margin.

## Verification gates

- Portable CTest: pending this branch's CI.
- Windows VST3 build and real hosted VST3 synthetic render at 44.1/48/96 kHz: pending this branch's CI.
- Native reference-audio render / synchronized A-B: not yet run on this branch.
- FL Studio session save/reload and user listening acceptance: not run.
- The old offline five-band page is retained as a comparison baseline; it is not represented as the output of this new production-path code.


## 2026-11-16 — independent true-peak calibration correction

The first 0.3 dB safety margin passed a regression that used the same 4x FIR detector as both limiter and test oracle. Re-checking the recovered 15-second master/no-chain excerpts with an independent 16x windowed-sinc reconstruction and FFmpeg `ebur128=peak=true` showed the guarded output at about **-1.03 / -1.1 dBTP**, above the test's -1.25 dBTP target. The test was circular; the audio evidence exposed the gap.

A 0.6 dB margin improved the music excerpts, but the deterministic full-band stress case still exposed under-read when checked with a longer independent 4x reconstruction. The final guard margin is now **0.8 dB**. A local offline Python mirror of the guard (not the compiled VST3) measured the two original excerpts at approximately **-1.52 / -1.53 dBTP** with a 257-tap Kaiser 4x reference; FFmpeg `ebur128=peak=true` reported **-1.6 dBTP** for both. The reference excerpt's integrated loudness moved from -7.60 to -9.12 LUFS; no loudness normalization was applied.

The portable C++ regression now uses two independent test oracles: a 16-phase, 65-tap Blackman-windowed sinc check on a 20 kHz tone at 44.1 / 48 / 96 kHz, and a 257-tap Kaiser 4x polyphase check on the deterministic full-band burst. Both are compared with the user-facing **-1.0 dBTP** ceiling, not the previous test-only -1.25 dBTP threshold. A local standalone C++ harness copied the branch's monitor/guard logic and passed with 20 kHz output peaks of -1.79 / -1.69 / -1.80 dBTP and a 48 kHz full-band burst at -1.09 dBTP. The harness was compiled with `g++ -std=c++17 -O2 -Wall -Wextra -Werror`; it is not a substitute for the full repository CTest or Windows VST3 build.

The new exact-head Windows workflow must pass this independent regression. The offline reference calibration is not a native VST3 music render; FL Studio session recall, target-music VST3 A/B, CPU/latency profiling and user listening acceptance remain open.
