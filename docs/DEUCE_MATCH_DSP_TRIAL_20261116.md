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

The master is +2.439 LU louder. After loudness matching, its relative energy change is about -1.18 dB at 20–60 Hz, -1.49 dB at 60–120 Hz, +1.53 dB at 4–8 kHz, and +1.48 dB at 8–16 kHz. The previous A/B page's text claimed a 294-sample shift while its JSON reported 31 samples; inspection of its playback code showed that no offset was applied. The corrected 2026-11-17 reference-to-reference package uses the same relative 0:00 for both tracks, no sample-delay compensation and no phase/null alignment. A separate 297-sample full-length cross-correlation estimate is diagnostic only and is not used for playback; the earlier 263-sample residual-alignment claim is withdrawn.

The prior offline five-band candidate, measured at -14 LUFS, had a 0.36 dB mean six-band spectral error and about 0.52 dB 200 ms envelope error against its reference excerpt. It is an offline prototype render, not a native VST3 render and not perceptual approval.

## Production-path change in this draft

The named `SKELL MASTER` factory now enables a real, saved APVTS boolean `deuceMatch` (appended after existing IDs; default OFF). When enabled:

1. `FiveBandColorPrototype` runs inside `MasteringEngine` in the existing 4x oversampled path, after compression/imaging and before its internal clipper/lookahead limiter; wet 0.75 with intensities {0.38, 0.55, 0.64, 0.72, 0.84}.
2. `PostLimiterPresencePrototype` applies the reference-derived +2 dB / 5 kHz bell after downsampling, before final output safety.
3. The opt-in Deuce Match path uses `StereoTruePeakGuard`: a 257-tap / 4-phase Kaiser reconstruction detector, 32 host-sample lookahead, stereo-linked gain, 85 ms release and 1.2 dB internal margin. Legacy factories retain the original `StereoPeakGuard` sample-peak limiter. Bypass keeps the same delay but does not attenuate.

**Bypass CPU correction (2026-11-17):** When the latency-aligned host bypass is engaged, the engine now fades the optional five-band color stage to neutral while continuing to update crossover history. This avoids running the nonlinear band-gain path on a bypassed signal and preserves click-free reactivation. No factory values, APVTS IDs, state schema, or reported latency change.

**Topology correction (2026-11-17):** The five-band stage now runs inside `MasteringEngine` at 4x, after the existing compression/imaging stages and before the internal clipper/lookahead limiter. The wrapper no longer applies it after `processFrame`. The +2 dB / 5 kHz presence bell remains after downsampling and before the final true-peak guard. This corrects the known pre-L2/post-L2 ordering mismatch, but the five-band processor remains an original approximation, not a Waves Vitamin clone; full-chain sound matching still requires a native VST3 render against the same private reference and user listening acceptance.

The original four factory presets retain their parameter values and keep `deuceMatch=OFF`. Existing parameter IDs remain unchanged; `deuceMatch` is appended after `bypass`. State schema advances to v4, and V1/V2/V3 sessions receive an explicit neutral `deuceMatch=OFF` value. A UI regression exercises APVTS save/reload and the legacy missing-parameter default. The `ceiling` control is a true-peak target in Deuce Match mode, with a 1.2 dB internal reconstruction margin. The plugin reports a fixed 32-sample PDC delay in both modes; legacy factories keep their original sample-peak limiting and use the true-peak delay in bypass.

## Verification gates

- Portable CTest: pending this branch's CI.
- Windows VST3 build and real hosted VST3 synthetic render at 44.1/48/96 kHz: pending this branch's CI.
- Native reference-audio render / synchronized A-B: not yet run on this branch.
- FL Studio session save/reload and user listening acceptance: not run.
- The old offline five-band page is retained as a comparison baseline; it is not represented as the output of this new production-path code.


## 2026-11-16 — independent true-peak calibration correction

The original 48-tap / 4-phase guard failed the exact-head Windows CTest on the deterministic full-band burst: the independent 257-tap Kaiser reconstruction exceeded the user-facing -1.0 dBTP ceiling. A standalone C++ reproduction of the branch's 48-tap detector/guard also showed that its 0.8 dB margin was not robust against the high-crest stress signal. The earlier offline music excerpt measurements did not expose this transient failure.

The Deuce Match guard now uses a 257-tap / 4-phase Kaiser reconstruction detector. It evaluates the interval around the sample delayed by 32 host frames, with stereo-linked instantaneous attack and 85 ms release. The internal margin is 1.2 dB. Coefficients are prepared in `prepareToPlay`; the audio callback uses fixed-size history and delay arrays only. Legacy factories still use the original `StereoPeakGuard` sample limiter; the new true-peak guard runs in bypass as a fixed PDC delay so automation does not change host latency. The original 48-tap `StereoTruePeakMonitor` remains available for UI telemetry.

A standalone C++ prototype of the new detector/guard was checked against the independent 16x windowed-sinc oracle. With the 1.2 dB margin, deterministic full-band bursts measured approximately -1.19 dBTP at 44.1 / 48 / 96 kHz; a 20 kHz tone measured -1.75 / -1.50 / -1.87 dBTP. The guard-only prototype took about 3.8–8.6 ms to process 250 ms of stereo audio across 44.1–96 kHz on this container. These are isolated prototype measurements, not a full plugin CPU benchmark or a Windows VST3 result.

The repository regression now checks both the independent 257-tap Kaiser reconstruction and an independent 16x windowed-sinc reconstruction on deterministic full-band bursts at 44.1 / 48 / 96 kHz, in addition to the 20 kHz cases, stereo linking, bypass alignment, reset and finite recovery. A separate regression confirms legacy factories retain sample-peak-only behavior while Deuce Match protects the same inter-sample stress signal. Exact-head Windows CI must pass before this DSP change is considered verified. The private reference WAV/FST bytes remain outside GitHub; native music A/B, FL Studio session save/reload, real-device CPU/latency profiling and user listening acceptance remain open.


## 2026-11-18 hosted VST3 state-audio regression

The hosted VST3 test now verifies rendered output after host-state restoration, not only serialized parameter values. A fresh VST3 instance is restored from the SKELL MASTER factory state and compared sample-for-sample against the source instance on the same deterministic stereo signal. The test repeats with latency-aligned bypass restored. Windows run #238 on commit e9c24cd9e846376b241ffb2c4e1021e510c07757 passed CTest 9/9; maximum rendered difference was 0 at 44.1 / 48 / 96 kHz, with finite output and reported latency 105 / 110 / 182 samples. This remains synthetic host validation, not owner-audio A/B or FL Studio.

A Linux VST3 host-render target and private local-reference workflow are being added in PR #17. The first Linux build is pending; no owner WAV/FST is sent to CI.


## 2026-11-18 maximum legal output/clip regression

A local render through the exact PR #21 Linux hosted VST3 reproduced a safety-boundary failure using legal public parameter values on the same 15-second Deuce beat-containing input: `Output Trim=12`, `Clip=100`, `Ceiling Adjust=3`, with Deuce Match enabled. The configured sample ceiling was -0.1 dBFS and the hosted 48-tap meter reported -0.825 dBTP, just above the smoke-test minimum of -0.85 dBTP. An independent local 16x Kaiser estimate was -0.861 dBTP. The renderer correctly rejected the output rather than reporting a pass.

The guard's internal margin is increased from 1.0 to 1.2 dB. The hosted CI smoke tests now exercise this maximum legal output/clip/ceiling combination on Windows and Linux, and the independent guard regression requires at least -1.1 dBTP at the -1 dBFS test ceiling. This is a safety-only change; factory parameter values, APVTS/automation IDs, preset state schema, fixed PDC, and the default Deuce Match-off behavior remain unchanged. Exact-head CI and a repeat local render are required before calling the correction verified.
