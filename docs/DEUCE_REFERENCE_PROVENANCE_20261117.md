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

Do not conflate the separate approved vocal-only `Deuce_v25_Repro_Audit_15s.flac` (SHA-256 `$(await tools.mcp__GitHub__fetch_file({repository_full_name:"skellyruno/skell-master-vst",path:"docs/DEUCE_REFERENCE_PROVENANCE_20261117.md",ref:parent})).result.sha`) with the beat-containing master-bus pair. The v2.5 vocal champion is a separate vocal-chain reference and is not the SKELL MASTER master-bus target.
