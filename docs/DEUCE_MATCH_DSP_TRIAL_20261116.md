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
3. The opt-in Deuce Match path uses `StereoTruePeakGuard`: a 257-tap / 4-phase Kaiser reconstruction detector, 32 host-sample lookahead, stereo-linked gain, 85 ms release and 1.0 dB internal margin. Legacy factories retain the original `StereoPeakGuard` sample-peak limiter. Bypass keeps the same delay but does not attenuate.

The original four factory presets retain their parameter values and keep `deuceMatch=OFF`. Existing parameter IDs remain unchanged; `deuceMatch` is appended after `bypass`. State schema advances to v4, and V1/V2/V3 sessions receive an explicit neutral `deuceMatch=OFF` value. A UI regression exercises APVTS save/reload and the legacy missing-parameter default. The `ceiling` control is a true-peak target in Deuce Match mode, with a 1.0 dB internal reconstruction margin. The plugin reports a fixed 32-sample PDC delay in both modes; legacy factories keep their original sample-peak limiting and use the true-peak delay in bypass.

## Verification gates

- Portable CTest: pending this branch's CI.
- Windows VST3 build and real hosted VST3 synthetic render at 44.1/48/96 kHz: pending this branch's CI.
- Native reference-audio render / synchronized A-B: not yet run on this branch.
- FL Studio session save/reload and user listening acceptance: not run.
- The old offline five-band page is retained as a comparison baseline; it is not represented as the output of this new production-path code.


## 2026-11-16 — independent true-peak calibration correction

The original 48-tap / 4-phase guard failed the exact-head Windows CTest on the deterministic full-band burst: the independent 257-tap Kaiser reconstruction exceeded the user-facing -1.0 dBTP ceiling. A standalone C++ reproduction of the branch's 48-tap detector/guard also showed that its 0.8 dB margin was not robust against the high-crest stress signal. The earlier offline music excerpt measurements did not expose this transient failure.

The Deuce Match guard now uses a 257-tap / 4-phase Kaiser reconstruction detector. It evaluates the interval around the sample delayed by 32 host frames, with stereo-linked instantaneous attack and 85 ms release. The internal margin is 1.0 dB. Coefficients are prepared in `prepareToPlay`; the audio callback uses fixed-size history and delay arrays only. Legacy factories still use the original `StereoPeakGuard` sample limiter; the new true-peak guard runs in bypass as a fixed PDC delay so automation does not change host latency. The original 48-tap `StereoTruePeakMonitor` remains available for UI telemetry.

A standalone C++ prototype of the new detector/guard was checked against the independent 16x windowed-sinc oracle. With the 1.0 dB margin, deterministic full-band bursts measured approximately -1.19 dBTP at 44.1 / 48 / 96 kHz; a 20 kHz tone measured -1.75 / -1.50 / -1.87 dBTP. The guard-only prototype took about 3.8–8.6 ms to process 250 ms of stereo audio across 44.1–96 kHz on this container. These are isolated prototype measurements, not a full plugin CPU benchmark or a Windows VST3 result.

The repository regression now checks both the independent 257-tap Kaiser reconstruction and an independent 16x windowed-sinc reconstruction on deterministic full-band bursts at 44.1 / 48 / 96 kHz, in addition to the 20 kHz cases, stereo linking, bypass alignment, reset and finite recovery. A separate regression confirms legacy factories retain sample-peak-only behavior while Deuce Match protects the same inter-sample stress signal. Exact-head Windows CI must pass before this DSP change is considered verified. The private reference WAV/FST bytes remain outside GitHub; native music A/B, FL Studio session save/reload, real-device CPU/latency profiling and user listening acceptance remain open.
