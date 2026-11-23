# Windows reference follow-up

## Verified Windows build

Source `9ac29ec25ca9720412294bd02935346aae9b18bd` fixes neutral DEFAULT transient overshoot with an allocated-in-prepare host-rate dry delay and a 12 ms transition. Audio processing uses fixed storage. Automation IDs, factory numbering and reported latency are retained.

Windows run [45085028009](https://github.com/skellyruno/skell-master-vst/actions/runs/45085028009) passed 9/9 CTest, external WAV smoke tests and strictness-5 pluginval. The checkout was PR merge `39c81d3d33593cc19f8cd748810200beeda8fb41`, whose tree equals source `9ac29ec`.

The previous Windows smoke test failed with +1.644759 dB DEFAULT transient overshoot. It now reports +0.000000 dB peak difference and correlation 1.000000 on both channels. Processor tests compare directly against delayed source samples, including a first-sample impulse.

[Verified draft Windows ZIP](https://github.com/skellyruno/skell-master-vst/actions/runs/45085028009/artifacts/14778529615). Archive SHA256 `9dc6b4de2a7391187d142dc1cac9d3627db3c72441901f8f9e478ca4c4378e81`.

## Full-song compiled VST3 comparison

Private source and references were extracted locally from the owner's ZIP. Measurements use committed `tools/reference_compare.py`, 44.1 kHz, 0.25–98.75 seconds, reference delay 31 samples and candidate delay 105 samples. Every candidate was rendered through the downloaded Windows VST3. No audio is included in this repository.

The listening candidate changes existing controls to Input +5 dB, Glue 20%, Sauce 12%. All other factory5 controls retain their current values. These are experimental settings for listening. Factory5's saved defaults retain their existing values pending the owner's comparison.

| Metric | Owner five-plugin target | Current factory5 | Listening candidate |
|---|---:|---:|---:|
| Integrated LUFS | -5.493227 | -6.252561 | -6.099252 |
| Crest dB | 5.437936 | 5.564078 | 5.421181 |
| Side/mid dB | -13.647907 | -13.619110 | -13.555793 |
| Five-band MAE dB | — | 0.115126 | 0.093074 |
| 20 ms envelope MAE dB | — | 0.807459 | 0.761732 |
| 50 ms envelope MAE dB | — | 0.570920 | 0.525216 |
| Independent 16x peak dBTP | +2.588582 | -0.422668 | -0.4372 |

Candidate level-matched band differences are +0.0496, -0.1147, -0.0907, +0.0967, -0.1137 dB for 20–90 / 90–500 / 500–2500 / 2500–9310 / 9310–20000 Hz.

Intro band MAE improves from 0.3869 to 0.3324 dB and 50 ms envelope MAE from 0.6614 to 0.5465 dB. Middle band MAE improves from 0.1006 to 0.0948 dB. The held-out ending has a small regression, with band MAE 0.0724 to 0.0730 dB, 20 ms envelope MAE 0.7785 to 0.7841 dB and 50 ms envelope MAE 0.4810 to 0.5143 dB. Ending crest error becomes -0.1324 dB. Both versions are retained for listening.

Lower compression improved loudness while worsening overall tone and envelope errors. Input +5 alone reduced the loudness gap to 0.5023 LU but produced -0.2616 dB ending crest error. Glue 20 restores crest, and reducing Sauce brings whole-song width error below 0.1 dB. This establishes a measured gain-staging improvement using existing controls. Compressor attack/release equivalence remains unproven.

The source itself contains 154590 full-scale channel samples, including a contiguous 72-frame run. The four-plugin export measures -7.175226 LUFS and +3.556655 dBTP. These clipped exports constrain achievable transient matching. A simple +0.606 dB lift of the selected candidate would exceed 0 dBTP.

## Remaining verification

CI hosted tests pass at 44.1/48/96 kHz with 105/110/182 samples latency. CI CPU measurements vary by runner. Local full-song candidate CPU was 14.01% of one core at 44.1 kHz, block 257, AMD64 Family 23 Model 24. Concurrent work and differing hardware prevent a performance improvement claim.

Three compiled Windows editor images were inspected at 620×393, 702×445 and 1200×760. Images from the verified fix are byte-identical to the inspected prior build. A separate local MSVC build passed 8/9 tests, including all DSP/host tests, but failed the stationary-knob pixel assertion. The CI package passes that assertion. The local rendering discrepancy remains unresolved and must be investigated before claiming local UI certification.

FL Studio save/reopen remains unverified. The private listening set aligns both renders and the owner reference to the same 98.5 seconds and applies integrated normalization to -14 LUFS.

The selected settings also passed hosted full-song renders after polyphase resampling the private source to 48 and 96 kHz. Reported latency was 110/182 samples and host reconstructed peaks were -0.350590/-0.274175 dBTP. Independent 16x peaks were -0.370862/-0.416796 dBTP. Local CPU was 15.365%/29.288% of one core at buffer 257. Resampled source may contain intersample peaks above the original PCM ceiling. These checks validate stability and headroom at those rates, while reference matching remains measured at native 44.1 kHz.
