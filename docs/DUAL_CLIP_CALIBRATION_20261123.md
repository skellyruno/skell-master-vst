# Dual-reference stereo calibration

## Scope

The owner requested further measured matching with Clip zero and Clip 35, preserving the analog-style reference voicing. No new control was requested. Both cases were rendered through the actual Windows VST 3 using the private full song and committed `tools/reference_compare.py`. Native rate was 44100 Hz, analysis interval 0.25–98.75 seconds, reference delay 31 and candidate latency 105 samples. Ending 85–98.75 seconds was checked separately.

The retained change increases factory 5's existing side trim from -0.274 to -0.38 dB. It adds approximately 0.106 dB of side attenuation to the existing correction before the final true-peak guard. The current shelves, broad bell, harmonic stages, preset values, compressor timing and output ceiling remain the measured baseline. Processing topology, APVTS IDs and latency are preserved. The phase-coherent stereo-ratio regression uses the calibrated coefficient.

## Full-song measurements

| Metric | Clip 0 previous | Clip 0 calibrated | Clip 35 previous | Clip 35 calibrated |
|---|---:|---:|---:|---:|
| LUFS-I | -8.007436 | -8.011184 | -6.099252 | -6.102664 |
| Crest dB | 7.482877 | 7.480820 | 5.421181 | 5.419116 |
| Side/mid error dB | +0.120610 | +0.019806 | +0.092114 | -0.008007 |
| Five-band MAE dB | 0.114002 | 0.119612 | 0.093074 | 0.098377 |
| 20 ms envelope MAE dB | 0.907461 | 0.908196 | 0.761732 | 0.763518 |
| 50 ms envelope MAE dB | 0.628018 | 0.629244 | 0.525216 | 0.527480 |
| Independent 16x dBTP | -0.461617 | -0.461838 | -0.437225 | -0.436111 |

Reference Clip 0/Clip 35 LUFS are -7.175226/-5.493227, crest 7.272327/5.437936 dB, side/mid -13.117383/-13.647907 dB and independent peaks +3.556655/+2.588582 dBTP.

The retained tradeoff primarily improves stereo matching at both settings. Band error rises approximately 0.005–0.006 dB, and envelope changes are below 0.003 dB. No loudness improvement is claimed. Existing safe peak constraints remain relevant to the residual 0.836/0.609 LU gaps.

Ending side/mid errors improve from +0.179711 to +0.0759 dB at Clip 0 and from +0.2879 to +0.1851 dB at Clip 35. Intro width improves at Clip 0, from +0.2696 to +0.1688 dB, while Clip 35's intro becomes narrower, from -0.3034 to -0.4001 dB. Ending band errors change from 0.091847 to 0.0961 dB at Clip 0 and 0.0730 to 0.0720 dB at Clip 35. The calibration is a full-song tradeoff with a documented intro regression.

## Rejected experiments

- Input 6/7 reduced the Clip 0 loudness gap but widened the output and worsened intro/ending envelopes.
- Output Trim 1/2 and combinations with Glue 12 or Dynamics 6 moved loudness and crest closer while worsening stereo and held-out envelopes. These values were not promoted.
- Glue attack 1 ms versus 27 ms worsened overall crest/envelope error at both clip settings. The original 27 ms attack was restored.
- Glue release 80 ms versus 220 ms gave a small Clip 0 improvement but worsened clipped tone/envelopes and produced -0.2524 dB ending crest error. The original 220 ms release was restored.
- Highs 4 with the stereo correction improved intro band error but increased middle/ending band error substantially. Highs 3 was retained.

Local private previews contain the two references, previous outputs and calibrated outputs, aligned to the same 98.5 seconds and normalized to -14 LUFS. No recordings or previews are committed. Listening has not yet established a subjective quality improvement.

## Rate and safety checks

The compiled calibrated VST3 passed full-song hosted state equivalence, finite output and peak checks at 48 and 96 kHz with Clip 0 and Clip 35, using polyphase-resampled copies of the same private source. Reported latency stayed 110/182 samples at those rates and 105 samples at native 44.1 kHz. Independent 16x peaks were -0.459027/-0.371462 dBTP at 48 kHz for Clip 0/35, and -0.460086/-0.417155 dBTP at 96 kHz. Native 44.1 kHz peaks are in the table above. These rate checks assess headroom and stability, while reference-matching measurements use the native 44.1 kHz files.

Full-song host wall-time CPU readings varied with concurrent machine load. At 44.1 kHz the two calibrated renders reported 17.36/17.41% of one core at buffer 257. At 48 kHz they reported 18.98/27.35%, and at 96 kHz 43.38/37.35%. A constant coefficient substitution adds no processing stage or realtime allocation. These measurements support no CPU-improvement claim.
