# Deuce reference provenance — 2026-11-17

This note records the exact private source assets inspected for the SKELL MASTER / Deuce sound-match work. Original WAV/FST bytes remain in the owner's Library and are **not committed** to this repository.

## Keep the three FL Studio chains distinct

### REC vocal track — `rec deuce.fst`
SHA-256: `068911190350164db40955c0f111711fecd9cc7e194d3edb35fba861fa013168` (1,682 bytes; FL Studio 26.1.6.5639)

1. Fruity Parametric EQ 2
2. Fruity Limiter
3. Soundgoodizer — Mode A, raw serialized amount 43
4. Fruity Limiter

The Soundgoodizer amount is a raw integer, **not a verified percentage**.

### MAIN vocal track — `main deuce.fst`
SHA-256: `989b263f5b720a70b0d3cc24b1a4a6be914f6b80c599d0621f15232ff0999f17` (27,414 bytes; FL Studio 26.1.6.5639)

1. Waves REQ 6 Stereo
2. Waves SSLChannel Stereo
3. Fruity Compressor
4. Waves DeEsser Stereo
5. Soundgoodizer — Mode C, raw serialized amount 503
6. Empty slot
7. Slate Digital Fresh Air
8. Waves C6 Stereo
9. Waves PuigTec EQP1A

The REQ 6 active stored bands are 92 Hz / +0.1 dB / 1.41; 206 Hz / -4.4 dB / 6.5; 414 Hz / -4.3 dB / 6.5; 3,487 Hz / -4.3 dB / 6.5; 7,828 Hz / 0 dB / 0.8; 11,232 Hz / +3.7 dB / 0.9. The two stored stereo sides have identical tokens. The DeEsser state contains frequency-like 5,860 Hz and threshold-like -29.9 fields; the complete physical-control mapping is not established. Proprietary Fresh Air state is not decoded.

### Master bus — `master.fst`
SHA-256: `dcdb0f487b612e61c293bda574168188117c3b42b907428133f1e048c39d9e11` (10,496 bytes; FL Studio 26.1.6.5639)

The embedded Waves XML confirms the separate master-bus order:

1. Waves API-2500
2. Waves Vitamin
3. Waves L2
4. Waves API-550

Corroborated settings:
- API-2500: Mastering 1; MED knee; NORM thrust; OLD type. Attack/ratio/release values are screenshot-derived approximations, not fully decoded raw-token mappings.
- Vitamin: crossovers 90 / 500 / 2,594 / 9,310 Hz; stored fader values 33.6 / 29.9 / 28.1 / 26.2 / 24.0 (not dB); widths 1.00; Punch 3.0; Direct 0.0; Trim -8.5.
- L2: threshold -2.8 dB; output ceiling -1.1 dB; ARC enabled.
- API-550: post-L2 presence stage; screenshot-derived +2 dB at 5 kHz. Exact raw token-to-knob mapping remains approximate.

The MAIN vocal-track chain and master-bus chain are different references. Do not conflate them.

## Original audio provenance and measurements

All six original WAVs are 18.75 seconds, 44.1 kHz, stereo, 24-bit PCM:

| Source | SHA-256 |
|---|---|
| `deuce w beat.wav` — no master | `38d3a4c82c640b5ed24ee5b61ec04ab33c80717618e1b97fe845a590e85da484` |
| `deuce my master preset.wav` — user master | `cfad397db8807f68b1b513653487721bdcb0daa7f3d5d204b9370f26c0b8b429` |
| `deuce test dry vocals.wav` | `e8386b6e634a602e34ea49544e961bfb798b11c0801b5a4da165411a0f16fa3a` |
| `deuce test record preset vocals.wav` | `ff5cbeb52c0f4e35341d59bcef6060a1bab813ddea27786b21840c142b10c820` |
| `deuce test main preset no rec preset.wav` | `06d7743c0fb1a3cb822d77342537eb685c23b196fc298336ad85d5ed82c76913` |
| `deuce test main preset vocals.wav` | `ead7acd89fa3579d6b4a36d04513d505d15be4238d1b4b4754a887d5d3555168` |

Full-length measured results:
- No-master mix: -10.094 LUFS, 9.218 dB crest, +2.242 dBTP diagnostic.
- User master: -7.659 LUFS, 7.606 dB crest, +2.455 dBTP diagnostic.
- Master delta: +2.435 LU; crest decreases 1.612 dB; stereo side/mid increases 1.047 dB.
- At integrated-loudness match, master-minus-no-master band deltas: 20–60 Hz -1.20 dB; 60–120 -1.51; 120–250 -0.82; 250–500 -0.91; 500–1k -0.06; 1–2k +0.37; 2–4k +0.77; 4–8k +1.55; 8–16k +1.47; 16–21k +1.02.
- Matched 20 / 50 / 200 / 400 ms envelope MAE: 1.442 / 1.284 / 1.144 / 1.079 dB.

The REC / MAIN vocal prints are dual-mono, not stereo-width references. Their full-length loudness is dry -18.173 LUFS; REC -16.225; MAIN-only -14.050; REC→MAIN -13.181. These are separate vocal-track prints and must not be compared directly to the beat-containing master-bus pair.

The original master audio already exceeds 0 dBTP in reconstructed intersample estimates. Do not reproduce that unsafe overs. A 15-second private A/B package was generated locally at -19 LUFS for the vocal group and -16 LUFS for the master group; all preview gains are attenuations and all 16× Kaiser diagnostic peaks are below -1 dBTP. The page is reference-to-reference only; it does not contain a current Skell Master VST3 render.

## Current plugin implementation boundary

The SKELL MASTER factory enables the saved, automatable `deuceMatch` parameter. Its current path uses the original five-band approximation at 4× before the internal limiter, a post-downsample +2 dB / 5 kHz presence bell, and the 257-tap / 4-phase true-peak guard. This is a measured first-pass approximation, **not** an API-2500 / Vitamin / L2 / API-550 clone or exact null match. The original unsafe master ceiling is not copied.

## Verification and remaining gates

- PR #15 exact-head Windows Release VST3 build passed.
- CTest 9/9 passed; hosted synthetic VST3 audio passed at 44.1 / 48 / 96 kHz.
- Strict moduleinfo JSON and two unique VST3 class IDs verified in the downloaded artifact.
- No private WAV/FST bytes were pushed to GitHub.
- Native owner-reference audio through this exact VST3, FL Studio session save/reload, real-device CPU, and user listening acceptance remain unverified.


## 2026-11-17 owner-Library reference recheck and alignment

The original owner-Library files were materialized read-only for this audit; no WAV or FST was uploaded to GitHub or modified.

- `deuce w beat.wav`: SHA-256 `38d3a4c82c640b5ed24ee5b61ec04ab33c80717618e1b97fe845a590e85da484`; 44.1 kHz, stereo, 24-bit PCM, 18.75 s.
- `deuce my master preset.wav`: SHA-256 `cfad397db8807f68b1b513653487721bdcb0daa7f3d5d204b9370f26c0b8b429`; same format and duration.
- `master.fst`: SHA-256 `dcdb0f487b612e61c293bda574168188117c3b42b907428133f1e048c39d9e11`.
- `rec deuce.fst`: SHA-256 `068911190350164db40955c0f111711fecd9cc7e194d3edb35fba861fa013168`.
- `main deuce.fst`: SHA-256 `989b263f5b720a70b0d3cc24b1a4a6be914f6b80c599d0621f15232ff0999f17`.

Raw FST strings and embedded Waves XML confirm the master chain order API-2500 → Vitamin → L2 → API-550, with Waves 15.0.70.71 state blocks and Setup A active. The REC chain is Fruity Parametric EQ 2 → Fruity Limiter → Soundgoodizer → Fruity Limiter. The MAIN vocal chain contains REQ 6 Stereo → SSLChannel Stereo → Fruity Compressor → DeEsser Stereo → Soundgoodizer → Fresh Air → C6 Stereo → PuigTec EQP1A. Non-Waves binary parameter maps are not claimed decoded by this pass. A local JSON artifact `Deuce_FST_Chain_Audit_2026-11-17.json` records source hashes and raw Waves parameter tokens.

A fresh full-length cross-correlation of the beat-containing master pair measures the user-master file 297 samples (6.735 ms) later than the no-master input, with normalized magnitude 0.827 after 1.5 kHz low-pass mono correlation; polarity is unchanged. Full-length integrated loudness is -10.094 LUFS (no-master) and -7.659 LUFS (user-master), a +2.435 LU difference. Crest factor falls by 1.612 dB and side/mid level increases by 1.047 dB. 16× Kaiser resampling estimates +2.239 dBTP and +2.512 dBTP respectively; these are estimates, not certified true-peak readings.

The new local audition package `SkellMaster_Deuce_MasterBus_Aligned_AB_2026-11-17` trims the measured 297-sample target delay and uses the same 15-second input-time interval. The shared safety attenuation is -3.512 dB; level-matched mode attenuates the louder mastered reference by a further 2.439 dB. No track is boosted. All previews are stereo 24-bit FLAC at 44.1 kHz. This remains reference-to-reference only; no Skell Master VST3 render or user listening approval is claimed.

Do not conflate the separate approved vocal-only `Deuce_v25_Repro_Audit_15s.flac` (SHA-256 `812cbbf758a2f378278e6a613fbe82f0068e34144fa7f2a6a737865d28b0273c`) with the beat-containing master-bus pair. The v2.5 vocal champion is a separate vocal-chain reference and is not the SKELL MASTER master-bus target.


## 2026-11-18 current-pass reference recheck and alignment correction

This pass re-opened the Library's self-contained 15-second pages, not the full-length 18.75-second WAV/FST sources.

- Four Library copies of SkellMaster_Reference_AB_15s.html are byte-identical: 8,281,675 bytes; SHA-256 469335809a874d5f0... (the complete hash is recorded in the local audit package).
- The embedded A/B excerpts are 44.1 kHz stereo PCM_24, exactly 15.000 seconds. Extracted FLAC SHA-256 values: no-master cddad67334ff8f1fcf1009c0d093a664ddc57e95668bde4a0e8e9fc0874ace55; user-master a36e6af0f2f93fe9e32f36e99180ea685713bf85c3f829808cdbbb65c1164625.
- Re-measured 15-second excerpt loudness is -10.03894 LUFS (no master) and -7.59986 LUFS (user master), +2.43908 LU. Both sample peaks reach 0 dBFS. Independent 16x Kaiser resample_poly estimates are +2.256 and +2.509 dBTP; these are diagnostics, not certified true-peak readings.
- The corrected local package SkellMaster_Deuce_Reference_AB_Corrected_2026-11-17.zip has SHA-256 92de1f79ee2e88a0857993ad0cd8526e55eb8c69471fc320c1608d781523b6bd. Its player starts both excerpts at the same relative 0:00 and applies no sample-delay compensation or phase/null alignment. It applies a shared -3.60 dB safety attenuation; level-matched mode applies a further -2.434748 dB attenuation to the louder master track. No track is boosted.
- The superseded page's text claimed a 294-sample shift while its JSON reported 31 samples; inspection of the playback code showed that no offset was applied. The separate 297-sample full-length cross-correlation estimate above is diagnostic only and is not used for playback. The earlier 263-sample residual-alignment claim is withdrawn.
- SkellMaster_FiveBand_Reference_AB_15s.html is also available (SHA-256 27b044eea9220580434522c28b99c37a3e41b9970e4d03ac248720b6497d5b77). It labels B as an experimental candidate and explicitly warns that B is an isolated offline five-band/EQ/width/linked-gain prototype, not the compiled Skell Master VST3 and not perceptual approval.
- The full-length WAV/FST files were not re-materialized or reparsed in this pass. Their hashes and chain analysis above remain evidence from the previous read-only audit; this pass re-verified only the embedded 15-second excerpts. No private WAV/FST bytes were uploaded to GitHub.

## 2026-11-18 hosted-state regression

PR #17 commit e9c24cd9e846376b241ffb2c4e1021e510c07757 passed Windows workflow run #238: Release VST3 build, strict moduleinfo JSON, CTest 9/9, UI interaction test and hosted synthetic VST3 renders at 44.1/48/96 kHz. The new state test renders the same 1.25-second stereo transient/bass/body/air signal through the source instance and a fresh instance after factory-state restore, then repeats with bypass restored. Maximum output difference was exactly 0 at all three rates; output remained finite and below full scale. Reported host latency was 105 / 110 / 182 samples at 44.1 / 48 / 96 kHz. The 9-second synthetic hosted renders measured 6.15% / 6.74% / 13.28% one-core realtime CPU. The separate 1-second external-WAV CI smoke uses a generated fixture, not owner audio.

This validates host-state audio equivalence and the Windows VST3 synthetic path. It does not constitute a native owner-reference render, FL Studio save/reload, real-device CPU measurement or user listening acceptance. A Linux-hosted VST3 renderer is being added for local private-reference rendering; its first CI build is pending.

## 2026-11-17 actual compiled VST3 reference render (run #238)

This section supersedes the earlier statements that no native owner-reference render had been completed. The owner files were materialized read-only and rendered locally; no private WAV/FST bytes were uploaded to GitHub.

### Source identity and chain separation

- `deuce w beat.wav`: SHA-256 `38d3a4c82c640b5ed24ee5b61ec04ab33c80717618e1b97fe845a590e85da484`; 44.1 kHz, stereo, PCM_24, 18.750 s. This is the no-master input for the master-bus comparison.
- `deuce my master preset.wav`: SHA-256 `cfad397db8807f68b1b513653487721bdcb0daa7f3d5d204b9370f26c0b8b429`; same format and duration; this is the user master-bus target.
- `master.fst`: SHA-256 `dcdb0f487b612e61c293bda574168188117c3b42b907428133f1e048c39d9e11`; embedded plugin order verified as Waves API-2500 → Vitamin → L2 → API-550A. The API-2500 uses Mastering 1 / MED knee / NORM thrust / OLD type; Vitamin crossovers are 90 / 500 / 2,594 / 9,310 Hz, with stored fader values 33.6 / 29.9 / 28.1 / 26.2 / 24.0 (not dB), widths 1.00, Punch 3.0, Direct 0.0, Trim -8.5; L2 threshold -2.8 dB / ceiling -1.1 dB / ARC on; API-550A is post-L2 with screenshot-derived +2 dB at 5 kHz. Proprietary raw-token mappings remain partly approximate.
- `rec deuce.fst`: SHA-256 `068911190350164db40955c0f111711fecd9cc7e194d3edb35fba861fa013168`; Fruity Parametric EQ 2 → Fruity Limiter → Soundgoodizer (mode A, raw amount 43) → Fruity Limiter.
- `main deuce.fst`: SHA-256 `989b263f5b720a70b0d3cc24b1a4a6be914f6b80c599d0621f15232ff0999f17`; REQ 6 Stereo → SSLChannel Stereo → Fruity Compressor → DeEsser Stereo → Soundgoodizer (mode C, raw amount 503) → Fresh Air → C6 Stereo → PuigTec EQP1A. These REC/MAIN track chains are separate from the master-bus FST.
- The owner-preferred `Deuce_v25_Repro_Audit_15s.flac` remains a separate vocal-only champion (SHA-256 `812cbbf758a2f378278e6a613fbe82f0068e34144fa7f2a6a737865d28b0273c)); it was not used in this beat-containing master-bus comparison.

### Actual hosted VST3 render

- Compiled plugin: Skell Master Linux VST3, PR #17 head `8ecb75846c839b756c37591ca2241d6c71e8cd2e`, CI merge commit `12789f4b78eed72031332746f736c960833ea311`, workflow run `44238858598`. The render is a real JUCE-hosted VST3 output, not a Python DSP approximation and not an FL Studio render.
- Factory: `SKELL MASTER`, identity 5, Deuce Match enabled; Input +4 dB, Drive 8%, Glue 17%, Dynamics 11%, Lows -1 dB, Highs +3 dB, Sauce 20%, Transients 0%, Output Trim 0 dB, Ceiling Adjust 0, Clip 14%.
- Input rendered locally: `deuce w beat.wav`. Output: 44.1 kHz stereo PCM_24, 828,004 frames including 105 samples reported PDC and the safety tail. Render output SHA-256 `5afef12c0c21e3bcab08edd2a7b8ba6e4103b8977a3acadd148a0040d4cc85c4`.
- Hosted VST3 reported 105 samples / 2.381 ms latency and 4.97% of one core for processBlock on this container at 44.1 kHz / 257-sample blocks. This is not a target-laptop CPU measurement.
- Candidate output remained finite, had no sample-full-scale samples, sample peak -2.00 dBFS, host 48-tap reconstructed-peak readout -1.763 dBTP, and independent 16× Kaiser diagnostic -1.794 dBTP. The source and user-master target both contain samples at 0 dBFS and their independent 16× estimates are +2.251 and +2.511 dBTP; those unsafe overs are intentionally not copied.

### 15-second same-take comparison

A local self-contained package `SkellMaster-Deuce-VST3-AB-run238.zip` (SHA-256 `67495660d84d01630d5e6194f5f1659b17fd5bbe5af146c247e320551091f5c0`) contains synchronized 15-second stereo PCM_24 FLAC previews for A: user master target, B: actual hosted VST3 output, and C: no-master input, plus `index.html` and `metrics.json`. The preview removes the exact 105-sample VST3 PDC from B. No target offset or phase/null alignment is applied: the full-band raw target/input correlation peak is 294 samples, but local one-second peaks vary, so a single target offset is not treated as verified. A shared -3.60 dB safety trim is applied; level-match mode only attenuates louder tracks (A by 2.787 dB, C by 0.349 dB; B is never boosted).

Measured first-15-second results:

| Metric | No-master input | User master target | Skell Master VST3 |
|---|---:|---:|---:|
| Integrated loudness | -10.039 LUFS | -7.601 LUFS | -10.388 LUFS |
| Sample peak | 0.00 dBFS | 0.00 dBFS | -2.00 dBFS |
| 16× reconstructed-peak estimate | +2.251 dBTP | +2.511 dBTP | -1.794 dBTP |
| Crest | 9.284 dB | 7.660 dB | 8.514 dB |
| Side/mid | -17.673 dB | -16.625 dB | -15.907 dB |

The VST3 candidate is 2.787 LU quieter than the user master target and 0.349 LU quieter than the no-master input. After attenuating the target to the candidate's loudness, candidate-minus-target broad-band power differences are -0.108 dB (20–90 Hz), +0.541 dB (90–500 Hz), -0.410 dB (500–2,594 Hz), +0.041 dB (2,594–9,310 Hz), and +0.674 dB (9,310–19,000 Hz). Same-relative-start envelope MAE is 0.987 / 0.654 / 0.435 / 0.341 dB at 20 / 50 / 200 / 400 ms. These are objective diagnostics, not perceptual approval or an exact Waves-chain match.

### Exact-head CI and remaining gates

- Workflow run #238 Windows Release VST3 build passed; Windows CTest **9/9**, including true-peak, five-band topology, post-limit presence, UI geometry/contrast and hosted VST3 state-audio equivalence. The restored VST3 instance matched the source instance sample-for-sample (maximum difference 0) at 44.1 / 48 / 96 kHz; reported PDC was 105 / 110 / 182 samples. Strict moduleinfo JSON and two unique VST3 CIDs passed.
- The Linux VST3 build and hosted tests passed **8/8**; the local owner-reference render used that compiled Linux VST3 artifact.
- The Windows artifact from run #238 predates the program-name correction below and is preview-only. The PR #17 branch had a confirmed VST3 program-list defect: `getNumPrograms()` exposed one program while `getProgramName(0)` returned an empty string. The branch now returns `Default`, adds a hosted-VST3 program-name regression, and runs pluginval strictness 5 in the Windows workflow. Exact-head CI/pluginval for those follow-up commits is pending.
- FL Studio scan, automation, session save/reopen and owner listening acceptance remain unverified. No factory values, DSP coefficients, APVTS IDs, saved-state schema, PDC, or SkellCore shared controls were changed in this pass.
