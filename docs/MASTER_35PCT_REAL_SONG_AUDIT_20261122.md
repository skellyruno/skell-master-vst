# SKELL MASTER — real 35% CLIP, transient and limiter recovery audit
**2026-11-22, private owner audio. Draft PR #35. Do not publish source media.**

## Evidence and scope
The user supplied `master test preset.zip`, containing three stereo 44.1-kHz,
24-bit PCM, 99.648-second WAVs: dry premaster, four-plugin master, and the
five-plugin StandardCLIP version. All three passed ZIP CRC verification.
Actual hosted Linux JUCE VST3 renderer: build run 45078324863 (head
`84596b52fe994bd8f78968b2c94413307a18489c`, corresponding checkout
merge `a90fa430`). Skell exported with 105 samples reported PDC; owner's
masters were aligned by their observed 31-sample offset. Remove PDC before
comparisons. The private files remain outside source control.

## Whole-song result (actual compiled VST3)
| Signal | LUFS-I | RMS dBFS | Crest dB | Side/Mid dB |
|---|---:|---:|---:|---:|
| Owner four-plugin master | -7.175 | -7.272 | 7.272 | -13.117 |
| Skell factory5 CLIP 0 | -8.098 | -8.061 | 7.461 | -13.071 |
| Owner five-plugin master (StandardCLIP) | -5.493 | -5.438 | 5.438 | -13.648 |
| Skell factory5 CLIP 35 | -6.234 | -6.049 | 5.449 | -13.656 |
| Skell factory5 CLIP 45 | -5.927 | -5.695 | 5.095 | -13.745 |
| Skell factory5 CLIP 65 | -5.642 | -5.440 | 4.840 | -13.682 |

The 35% anchor is already **within 0.011 dB crest** and **0.008 dB
side-to-mid** of the user's clip-enabled master, but is **0.740 LU quieter**.
Making the clipper stronger to 65% nearly matches integrated loudness, yet
loses 0.598 dB crest and increases the five-band tonal error. Do not promote
65% to default without independent subjective approval.

After LUFS matching at 35%, relative spectrum against the five-plugin
reference is **+0.225, -0.153, -0.363, +0.121, -0.030 dB** over
20–90, 90–500, 500–2500, 2500–9310 and 9310–20000 Hz.
Mean absolute five-band error **0.178 dB**.
Aligned 20-ms programme-envelope mean absolute error **0.821 dB**,
50-ms error **0.584 dB**. Transient-onset comparisons show local
discrepancies even though *global* crest factor is matched.
Do not claim numerical crest equality implies an exact transient waveform.

The un-clipped chain also runs **0.923 LU** quieter than the user's
four-plugin reference; this pre-clipping gap is the main next target.
An upstream gain experiment at Input +5/+6 made the full five-plugin
render louder but lost crest and stereo match. A ceiling reduction to
0 improved loudness comparison to the owner's four-plugin export but
moved the five-plugin target further away. Preserve the current anchor
while iterating upstream compressor/limiter behaviour with A/B tests.

## Actual aliasing probe (compiled VST3; 44.1 kHz)
Separate 2-second, coherent high-frequency stereo tone renders use
factory5 with DRIVE/SAUCE/GLUE/DYNAMICS/HIGHS/LOWS/INPUT/TRANSIENTS = 0,
Deuce Match disabled, and CLIP = 0/35/65. The 4× oversampled native
CLIP35 produces third/fifth harmonics inside Nyquist, which are musical
content, **not aliasing**. A 7kHz third harmonic at 21kHz measures -58.2dBc
and is *not* a foldback. Actual folded harmonics:
| Sine | Folded harmonic | CLIP 35 (dBc) | CLIP 65 (dBc) |
|---|---|---:|---:|
| 7 kHz | 5th at 9.1 kHz | -119.05 | -110.34 |
| 13 kHz | 3rd at 5.1 kHz | -118.23 | -101.94 |
| 16 kHz | 3rd at 3.9 kHz | -118.25 | -103.85 |
| 16 kHz | 5th at 8.2 kHz | -110.03 | -97.58 |
| 19 kHz | 3rd at 12.9 kHz | -118.09 | -103.36 |

The CLIP35 4× anti-alias chain is performing well on these tests. More
oversampling would cost CPU and must be justified by new aliasing evidence
at other source rates and high drive. These tests do *not* prove equivalence
to SIR StandardCLIP's proprietary 8× engine. 

## Output safety and CPU
On this song, factory5 CEILING +3 / CLIP35 yields native VST3
sample peak **-0.600 dBFS** and host 4× reconstructed peak
**-0.422 dBTP**. This **does not meet a -1dBTP target**; the owner's clipped
reference is also intersample-unsafe (earlier independent 16× estimate
+2.58dBTP). Treat the factory5 high-ceiling mode as an explicit creative
ceiling, not a certified -1dBTP master. With CEILING 0 the Skell host
peak is about -1.322dBTP, but the loudness falls to -7.10 LUFS.
Host single-core CPU for this full-song render varies around
5.5–5.9% at 44.1kHz across these shared-runner measurements: *not*
a controlled before/after performance benchmark. PDC 105 samples.

## Limiter recovery bug and action
Source audit found the internal 1.5ms lookahead limiter continuing to
track peak attenuation while its *fully settled dry bypass* output was
selected. On re-enabling, the slow programme-dependent 65–220ms release
could carry a stale gain reduction onto quiet attacks. The fix leaves
the active wet limiter coefficient curve and programme DSP unchanged;
it clears its limiter gain and hold only once the 12ms wet/dry
crossfade reaches fully dry, then reactivates peak detection as wet
fades back in. A portable regression feeds 350ms of extremely hot
bypassed programme followed by 70ms of low-level wet audio and checks
stereo transparency and recovery. It is intentionally not a
speculative change to the approved CLIP35 or limiter release curve.

## Release gates
Compile the correction, run Windows/Linux CTests, pluginval strictness5,
actual Windows JUCE screenshots, actual VST3 owner's full-song null
analysis (normal wet output should be unchanged), and an audio host
bypass-transition test. A full-song A/B and FL Studio session save/reopen
remain essential. Never put original user WAVs or FL Studio presets into GitHub.

## Full-song tonal-fit experiment (subsequent draft iteration)
The pre-correction, exact compiled VST3 was separately filtered OFFLINE
using the repository's host-rate RBJ shelf/peak equations against the owner's
identical 84.5-second middle music segment. The four-stage fit at 95Hz
(+0.47dB shelf), 300Hz (+0.87dB/Q0.75), 1350Hz (+1.00dB/Q0.80),
and 5kHz (+0.69dB high shelf) reduced **level-matched 5-band mean
absolute spectral error** from **0.186dB to 0.046dB** in this *offline,
post-output preview*, but **increased sample peaks from -0.600dBFS
to +0.475dBFS** and increased crest factor by about 0.53dB.

Those elevated peaks are **NOT safe output** and the preview is **not**
proof of actual plugin improvement. The candidate filters are now
implemented after L2 but **before the original reconstructed true-peak
guard**, only for SKELL MASTER factory ID5. Run the same source through
the new compiled VST3, independently measure integrated LUFS, crest,
five-band error, stereo, 16x true peaks, 20/50ms envelopes and CPU. If
the real guard interaction degrades crest/envelope or fails peak safety,
reduce or revert this experimental correction. No host IDs, saved-state
schema or latency were changed. The owner reference itself exceeds
0dBTP and its unsafe transient peaks cannot be faithfully copied at
a safe target ceiling.


## Held-out validation rejected the overfit four-band candidate

The initial four-filter fit at 95/300/1350/5000Hz was fit on seconds
0.5–84.5 of the 99.648-second owner song. When evaluated against
the reserved **85–99.3-second segment**, its level-matched five-band
error improved only **0.151 → 0.143dB**; it also raised output peaks
unacceptably before true-peak protection. It is therefore **not** retained.

The retained, simpler experimental correction is **95Hz low shelf
−0.10dB plus 1350Hz bell +0.70dB/Q0.78**, selected only after the
held-out comparison:
| Offline post-output probe | Train 0.5–84.5s 5-band MAE | Held-out 85–99.3s MAE | Train 20ms envelope MAE | Train 50ms envelope MAE |
|---|---:|---:|---:|---:|
| Compiled baseline | 0.187dB | 0.151dB | 0.821dB | 0.596dB |
| Overfit four-band | 0.046dB | 0.143dB | 0.783dB | 0.544dB |
| **Retained two-band** | **0.109dB** | **0.043dB** | **0.787dB** | **0.543dB** |

The two-band offline preview's sample peak remains **+0.124dBFS**,
so this is a *research candidate*, **not** a deliverable master:
the plugin's actual end-of-chain true-peak limiter follows the tone
stage and must be measured with the compiled VST3. Absolute LUFS and
crest after real final limiting remain unverified until that render.
A measured in-plugin positive change is a release prerequisite;
no claim of perceptually audible superiority is made yet.

## Compiled two-band + constrained stereo/makeup follow-up

The real Linux VST3 artifact from exact head `2f14809a`, workflow
`45081842952`, was rendered on the full owner pre-master and measured
*after* the 4x engine and actual reconstructed final true-peak guard.
The prior `f606ffd` and new renders are aligned to the same 105-sample
host latency; owner's clipped reference is aligned to its 31-sample offset.

| Exact hosted full-song | v8 original f606ffd | Two-band 2f14809 |
|---|---:|---:|
| Integrated LUFS | -6.233 | -6.261 |
| Five-band MAE after loudness matching | 0.178dB | 0.103dB |
| 20ms envelope MAE | 0.821dB | 0.804dB |
| 50ms envelope MAE | 0.584dB | 0.565dB |
| Stereo side/mid error vs owner's reference | -0.008dB | **+0.274dB** |
| Crest factor error vs owner's reference | +0.011dB | **+0.150dB** |
| Independent 16x true peak | -0.386dBTP | -0.426dBTP |

To restore the reference's overall stereo width without reverting the
spectral improvement, next experimental commit adds only **-0.274dB
side trim** to the active factory5 post-L2 tonal module. It also adds
**+0.15dB pre-guard makeup** to attempt to reduce excessive crest while
retaining the configured true-peak protection. These numbers are
constrained candidate parameters, **not verified better** until actual
compiled VST3 full-song render, cross-rate tests, and CPU benchmarks.
Bypass, other factory IDs, and all automation parameter IDs remain
unchanged. Never imitate the owner's +2.58dBTP output overs.
