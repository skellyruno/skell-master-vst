# V14 actual hosted quality audit — 2026-11-25

Exact PR head 29784ca601f2e80e4774dda3dc82388b2327529e; PR merge checkout 09a379ed3f8983073534a82e089b1c909cddf4fe. CI 45562363243: Windows 15/15, Linux 14/14 tests passed, staged Windows pluginval 1 passed/0 failed. Prior Windows stack-fault test now passes after heap allocation correction. Artifact Windows 14998423610 SHA256 2876b5816e7d0e7b5af016f94405e1335b4529ed6bf9dcafab2c4647d1ca78aa; Linux 14998976122 SHA256 23c09bf3612e30398f630ea786c955bbf050872239eac6a51483f37008113ff8. Windows ZIP integrity passed.

## Preserved baseline
Actual 44.1k full 99.648s private song factory5 Clip35 default settings: V9 artifact from run 45455261584 (checkout 8c468b2a283fc8d36b1d7fe36f29c3ebc268f9a2) versus V14 4x, 4,395,625 stereo frames including tail, zero differing samples in 24-bit PCM. PDC 105, same output RMS and host reconstructed peak -0.395392 dBTP. CPU 6.275/6.410% one core sequential observation. V13/V14 4x 13s excerpt interior also zero differences. No private audio committed.

## Nine real music reference-mode renders
Same 13s excerpt, resampled at 48/96k; remove reported latency and one second at each edge for LU and independent 16x polyphase peak estimate. Full render passed existing host peak checks. CPU is one sequential Linux offline run, not FL Studio or repeat statistics.
|Hz|OS|CPU %|PDC samples|LU delta vs 4x|16x peak estimate dBTP|
|---|---|---|---|---|---|
|44100|2|4.323|103|-0.242|-0.521|
|44100|4|7.521|105|0|-0.470|
|44100|8|11.359|105|-0.177|-0.527|
|48000|2|4.349|108|-0.218|-0.453|
|48000|4|7.229|110|0|-0.471|
|48000|8|11.839|110|-0.161|-0.460|
|96000|2|8.884|180|+0.013|-0.472|
|96000|4|14.371|182|0|-0.478|
|96000|8|24.542|182|+0.017|-0.480|

Full song at 44.1k 2x and 8x also passed. Intro (1–17s), main (20–85s), end (86–98s) loudness delta vs 4x: 2x -0.146/-0.222/-0.194 LU; 8x -0.103/-0.162/-0.139 LU. 20ms envelope absolute error vs 4x after per-window loudness matching: 2x .129/.126/.134 dB, 8x .102/.099/.107 dB. No blanket makeup gain promoted: small material/rate-dependent discrepancies and peak headroom matter. 4x remains authoritative reference.

## Creative stress
Actual 9 rate/OS combinations, factory1, input +12, Sauce100, Spatial Width100, Clip0/Deuce off: all hosted peak checks pass. Independent interior 16x estimates (2/4/8 respectively) at 44.1k -1.427/-1.417/-1.412; 48k -1.384/-1.405/-1.389; 96k -1.380/-1.385/-1.385 dBTP. Actual guard coverage confirmed on tested song; not certification for arbitrary audio.

## Reusable gate
tools/audit_quality_modes.py takes local runner, plugin bundle, private WAV, NEW output directory and optional rates/creative switch. Reports host validation, PDC, offline CPU, LU, 16x estimate and hashes. Exact blob 8f0d0ceed9b4e690c1ed5dfebe8f5a086d80bd3f ran against actual V14 at 44.1k all three modes; replicated LU/peak results. Full matrix and creative matrix previously executed separately with same measurement procedure. Output files must stay private. No production source change in audit commit; no rebuild triggered intentionally. V14 compiled artifacts remain from the exact tested source head, not audit-only commit. Next: verify more independent songs, true plugin alias-specific measurements, FL Studio reinitialization and UI, then revisit program-dependent compression/recovery. JUCE 8.0.12 local retrieval still DNS-blocked; no real new Quick4 captures. No claim that 8x sounds better or matches reference better.
