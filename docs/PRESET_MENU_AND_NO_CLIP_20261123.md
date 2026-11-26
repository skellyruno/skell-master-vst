# Preset menu and no-clip comparison

## Verified build

Source `262b9b869960526d1603b5246f47f7007a682956` passed Windows 9/9 CTest, external WAV checks and strictness-5 pluginval, plus Linux validation in [run 45186644277](https://github.com/skellyruno/skell-master-vst/actions/runs/45186644277). Tested merge `22baf1567876be26781aacc87caa5ea4fe6f1966`. [Windows VST3 artifact](https://github.com/skellyruno/skell-master-vst/actions/runs/45186644277/artifacts/14824487690). The downloaded ZIP checksum matched `036bb388f60d0d49a7b51e2bab9df2c4ac91f04ff0a01b184fe55c1842ac9e5b`.

The downloaded CI VST3's full-song Clip0 render matched the measured local render byte-for-byte. It reported 105 samples latency and 13.98% of one core at 44100 Hz and block257 on the local machine. This is a single run rather than a controlled performance comparison.

## Preset selection

The menu contains DEFAULT, WARM MASTER, PUNCH MASTER, LOUD MASTER and SKELL MASTER. The owner requested removal of the plus variants, 5-PLUGIN MATCH and processing-mode/action entries. Saved host factory identities and parameter values remain supported by the processor.

The timer now updates the menu selection only after the processor's factory identity changes. This prevents it from replacing a pending asynchronous ComboBox selection with the previous factory. Edited values clear the current menu selection and display CUSTOM, so choosing that same factory again reapplies it. The regression fixture exercises a timer tick between selection and callback, and reselects edited SKELL MASTER.

The compiled standalone editor was inspected in its default view, open five-item popup, loaded SKELL MASTER view and alternate theme. Mouse selection loaded Input5/Glue20/Sauce12. Editing Input to6 and reselecting SKELL MASTER restored Input5. The local build retains the previously documented stationary-knob pixel assertion discrepancy.

5-PLUGIN MATCH used the same general reference-chain mode with older Input4/Glue17/Sauce20 and ceiling-offset0 calibration. It differed from the current SKELL MASTER sound. Its menu entry is retired as requested.

## Full-song comparison with Clip zero

The actual compiled Windows VST3 rendered the owner's private `master test.wav` with factory5 and `Clip=0`. Other controls retained Input5/Glue20/Sauce12 and current factory5 values. The no-clip target is the owner's separate four-plugin export. Evaluation used committed `tools/reference_compare.py`, native 44100 Hz, fixed 31/105-sample reference/candidate delays, and the 0.25–98.75 second window.

| Metric | Four-plugin reference | Skell Clip zero |
|---|---:|---:|
| Integrated LUFS | -7.175226 | -8.007436 |
| Crest dB | 7.272327 | 7.482877 |
| Side/mid dB | -13.117383 | -12.996773 |
| Independent 16x peak dBTP | +3.556655 | -0.461617 |

Level-matched band differences in 20–90 / 90–500 / 500–2500 / 2500–9310 / 9310–20000 Hz are +0.0499, -0.0718, -0.0339, +0.0649, -0.3496 dB. Five-band MAE is 0.114002 dB. Envelope MAE is 0.907461 dB at 20 ms and 0.628018 dB at 50 ms. The intro's band MAE is 0.409100 dB, middle 0.109537 dB, and held-out ending 0.091847 dB. Intro high-band energy is 1.2477 dB below the target after level matching.

Clip zero has close whole-song tonal balance with remaining loudness, envelope and intro treble differences. These measurements do not define a percentage of perceptual similarity. The local private listening pair uses the same alignment and 98.5-second region at -14 LUFS. No recordings or previews are committed.
