# Native DSP research transfer — 2026-11-24

V9 checkpoint: 2cf021079aa6e80c39d8cd315eac9620ab863ef5 (archive/v9-verified-20261124).
V10 initial 4f79bf6 compiled Windows/Linux; CI 45545638491 failed portable Width regression (8/9 Windows, 7/8 Linux). Prior test incorrectly reused a stereo-state filter and demanded exact mono immediately. Fresh mono null and bounded transition decay now separately asserted. No production DSP changed by this fix. Exact-head CI required.

Research source: skell-vst-decompiler draft PR1, head 00d44bd87a3ae72f981f1180fa149a7d14c7b3d8. Verified static findings do not establish vendor runtime audio equivalence.
- Conventional soft-knee gain computation and linked detector, multi-timescale recovery: experiment with independent fast/slow gain envelopes and transient-aware recovery. Do not copy vendor coefficients or assign TVB times to API2500.
- Six parallel detector groups, peak/power blending: original frequency-aware detector and multiband balance tests, bass sensitivity and stereo-link checks.
- FET polynomial / optical rational color and SOS filters: independently designed, level-matched harmonic transfer, alias and intermodulation evaluation at 44.1/48/96 kHz. Keep v9 4x PDC, compare higher oversampling only as a separate experiment.
- Lookahead limiter/peak candidates: compare original dual recovery against v9 using independent 16x true peak and held-out songs.
- M/S spatial processing: test width as optional creative control; at 100% retain reference baseline.

Targets (not results): full-song Clip35 abs loudness gap <=0.2 LU, band MAE <=0.08 dB, 20ms envelope MAE <=0.35 dB, 50ms <=0.30 dB, side/mid error <=0.05 dB. Never trade true peak safety or held-out quality for training numbers. Keep V9 and previous private reference files unchanged. Independent songs and real CPU measurement remain mandatory. Four user-mentioned source implementations not located yet; establish source authorization/provenance before integration.
