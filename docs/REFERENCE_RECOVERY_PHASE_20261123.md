# Factory 5 recovery and Clip phase calibration

## Measured scope

This continues draft PR35 on the existing branch. Private recordings remain outside Git. The real Windows VST3 processed the owner's full 99.648435-second source, with Clip 0 compared to the four-plugin reference and Clip 35 to the five-plugin reference. Analysis uses committed `tools/reference_compare.py`, native 44.1 kHz, reference delay 31, candidate delay 105, interval 0.25–98.75 seconds. No candidate-specific lag search was used. The phase fit used only 20–85 seconds; intro and ending were held out.

Factory 5 retains Input 5, Drive 8, Glue 20, Dynamics 11, Lows -1, Highs 3, Sauce 12, Transients 0, Output 0, Ceiling offset 3 and Clip 35. Parameter IDs, preset numbering, 4x oversampling and delay buffers remain unchanged. No width control was added.

## DSP change

The opt-in v9 factory 5 response uses 40/150 ms internal limiter recovery instead of 65/220 ms. Glue's 27/220 ms timing remains unchanged. A causal four-stage, unit-magnitude allpass bank before the clipper improves the measured clipped transient envelopes. At Clip zero the bank is bypassed. Through Clip 0–35, its corner frequencies move logarithmically from above the audio band to 90/500/2594/9310 Hz at the 44.1 kHz host rate. Coefficients use a bilinear analog-frequency mapping across rates. Q values are 0.495856529115, 0.617989725037, 0.856550120135 and 1.298156850978.

The prepare-time 1025-point coefficient table avoids audio-thread trigonometry/allocation. Smoothed Clip drives coefficient interpolation; reversed denominator coefficients retain static allpass magnitude. A 12 ms bypass transition is used only for activation/deactivation. No permanent parallel dry/wet phase blend is used at intermediate settings. An exploratory fit estimated a 31.366872-sample diagnostic delay; **that delay was not applied**, and neither evaluation offsets nor PDC were changed.

Existing factory 5 side correction smoothly varies from -0.52 dB at Clip zero to -0.40 dB at Clip 35. The previous v8 response retains its -0.38 dB trim and exact recovery. A new serialized v9 flag opts new factory 5 selections into calibration. Older saved states keep the previous response, including when resaved. Their preset label is CUSTOM until SKELL MASTER is explicitly selected again.

## Native full-song comparison

Previous is source `c652f9a`; candidate is the recovery/phase implementation in this document. All errors are candidate versus the correct owner reference. Band/envelope errors are measured after loudness matching.

| Metric | Clip 0 previous | Clip 0 candidate | Clip 35 previous | Clip 35 candidate |
|---|---:|---:|---:|---:|
| LUFS-I | -8.011184 | -7.871349 | -6.102664 | -6.150321 |
| Loudness error LU | -0.835959 | -0.696123 | -0.609437 | -0.657095 |
| Crest error dB | +0.208493 | +0.066125 | -0.018820 | -0.020446 |
| Side/mid error dB | +0.019806 | -0.017341 | -0.008007 | -0.070749 |
| Five-band MAE dB | 0.119612 | 0.1104 | 0.098377 | 0.1270 |
| 20 ms envelope MAE dB | 0.908196 | 0.8872 | 0.763518 | 0.5298 |
| 50 ms envelope MAE dB | 0.629244 | 0.6020 | 0.527480 | 0.4685 |
| Independent 16x dBTP | -0.461838 | -0.462130 | -0.436111 | -0.436803 |

Reference Clip 0/35 loudness is -7.175226/-5.493227 LUFS, crest 7.272327/5.437936 dB and true peak +3.556655/+2.588582 dBTP. Safe Skell outputs cannot copy those unsafe peak values. No subjective sound-quality percentage is claimed.

The clipped candidate reduces 20 ms envelope error about 31%, with a modest tonal regression and 0.048 LU less loudness. Its intro/main/ending 20 ms errors are 0.5782/0.5170/0.4911 dB and 50 ms errors 0.5325/0.4479/0.4275 dB. Ending crest error is -0.1425 dB. Intro stereo error is -0.4338 dB, versus approximately -0.4001 previously; main/ending stereo errors are +0.1124/+0.1076 dB. These remain measurable limitations.

At Clip zero, candidate intro/main/ending band errors are 0.4640/0.1165/0.1092 dB, 20 ms envelope errors 0.9452/0.8615/0.8981 dB, and 50 ms errors 0.8023/0.5401/0.6014 dB. Ending crest error is -0.1039 dB. The no-Clip intro remains the largest tonal mismatch. The ending band error is worse than the previous 0.0961 dB despite whole-song improvement.

Intermediate Clip 7/14/21/28 full-song loudness is -7.7327/-7.3114/-6.7034/-6.2415 LUFS (rounded), with progressively falling crest and no loudness reversal. Independent peaks are -0.4982/-0.4424/-0.4723/-0.4656 dBTP. There are no owner references for intermediate positions, so these checks establish progression and safety, not a reference match there.

## Cross-rate safety

Full private-song renders at 48 kHz and 96 kHz passed finite-output and hosted saved-state audio-equivalence checks at both Clip endpoints. Latency stayed 110/182 samples, versus 105 at 44.1 kHz. Independent 16x peaks at 48 kHz are -0.434920/-0.328156 dBTP (Clip 0/35), and at 96 kHz -0.460088/-0.424790 dBTP. Resampled sources assess stability/headroom; matching claims use the original native-rate references.

The final true-peak guard remains 85 ms with original 0.50/0.45/0.40 dB margins. Faster guard releases of 20/30 ms, extra margins and gain-hold variants failed the existing independent reconstruction or attenuation bounds and were rejected. No safety threshold was relaxed. Full-song CPU readings were affected by other machine work and establish no performance improvement.

## Rejected alternatives and gates

- Always-on two/four-allpass versions improved clipped envelopes but made Clip zero significantly less dense, so they were rejected.
- Faster 10/80 ms recovery with the phase bank worsened held-out crest. Program-dependent recovery and 35/130 ms timing were less balanced than 40/150 ms.
- Extra input/output drive, reduced Glue/Dynamics, Highs 4 and faster Glue attack/release failed overall or held-out comparisons. See the preceding dual-reference audit.

Local Windows Release build and all nine CTests pass. This includes calibrated fast Clip automation, mono/rate safety, v8 state migration, v9 state roundtrip, finite-input handling and compiled VST3 hosting at 44.1/48/96 kHz. Two local test defects were fixed: the long-lived portable fixture exceeded Windows' stack when a nested response engine was created, and the rotary ring pixel mask intersected the moving pointer shadow. The fixture now allocates that engine on the heap; the exact-pixel stationary-ring assertion samples the actual JUCE layout outside the shadow. No production DSP threshold was relaxed. Actual compiled editor images at 620/1200 pixels and low/high pointer positions were inspected.

Hardware is AMD Ryzen 5 3450U, four cores/eight logical processors, Windows, Release build, host buffer 257. Full-song CPU readings with other machine work present were 19.36%/22.16% of one core at 44.1 kHz, 32.44%/33.81% at 48 kHz and 60.25%/65.24% at 96 kHz (Clip 0/35). They demonstrate faster-than-realtime offline processing on this machine, not a controlled CPU regression bound or deadline certification. PDC values are unchanged. Exact-commit CI/pluginval and current four-scene editor captures must be reported separately. FL Studio save/reopen and owner listening remain independent release gates. Six private matched previews (reference, previous and candidate at both Clip endpoints) were generated at -14 LUFS, outside Git.
