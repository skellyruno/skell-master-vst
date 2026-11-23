# Skell Master — Codex engineering handoff (2026-11-22)

## Continuation on 2026-11-23

Read `docs/WINDOWS_REFERENCE_FOLLOWUP_20261123.md` for the verified Windows build at `9ac29ec`, the DEFAULT transient fix, and new real Windows full-song comparisons. Existing factory5 remains the baseline. A measured listening candidate uses Input +5 dB, Glue 20%, Sauce 12%, with all other factory5 controls retained. It improves whole-song band/envelope/crest errors with a small held-out ending envelope regression. Private WAVs and listening previews stay outside the repository. The local stationary-knob pixel assertion failure and FL Studio save/reopen remain open.

> **Start from the existing project. Do not recreate it.** This is an active, **draft/unmerged** 4×-oversampled JUCE mastering plugin. The user wants **measured sound matching** to their own five-plugin FL Studio master, additional DSP/error fixes, and a verified Windows VST3. Do not publish their WAVs, FST, or song excerpts to GitHub.

## Entry points — read these before editing

- Repository: `skellyruno/skell-master-vst`
- Active branch: `tune/master-reference-postlimit-clip-20261122`
- Current audio source head at handoff: **`b211203a7943996b5e4b005d9b70c0e777160f48`**
- Existing **draft PR #35**: https://github.com/skellyruno/skell-master-vst/pull/35 (base `fix/master-preset-reapply-label-20261121`). Continue this work; don't open a parallel DSP PR, force-push, merge or close it.
- Rules: repository `AGENTS.md`; shared `skellyruno/SkellWorld/AGENTS.md`.
- Source: `Source/DSP/MasteringEngine.h` (`MasteringEngine`, CLIP35, lookahead limiter, `ReferenceToneCorrection`), `Source/PluginProcessor.cpp` (4× JUCE/guard routing/factory5 EQ), `Source/DSP/FiveBandColorPrototype.h`, `Source/PluginEditor.cpp`.
- QA: `tests/test_mastering.cpp`, `tests/test_ui.cpp`, `tests/test_vst3_host_audio.cpp`, `.github/workflows/build.yml`, `docs/MASTER_35PCT_REAL_SONG_AUDIT_20261122.md`, `docs/SKELL_MASTER_REFERENCE_CHAIN.md`.
- **New portable evaluation tool:** `tools/reference_compare.py` (no private audio or rendered audio in GitHub). The tool requires Python packages `numpy scipy soundfile pyloudnorm`.

## User reference — correct identity and staging

The owner's **actual** mastering path is **API-2500 → Waves Vitamin → SIR StandardCLIP → Waves L2 → API-550A**. The source, their master *without* StandardCLIP, and their master *with* StandardCLIP are three **separate 99.648-second 44.1kHz stereo 24-bit PCM WAVs from the same performance**. The user-supplied ZIP is named `master test preset.zip`; extract **locally** before comparison:

- `master test.wav` — **premaster input** to render with the Skell VST3.
- `master test preset.wav` — **owner four-plugin reference (no StandardCLIP)**.
- `master test w clip.wav` — **owner five-plugin reference (including StandardCLIP)**.

All original recordings are private. They are available to the owner in the original ChatGPT attachment or must be passed explicitly to Codex as files. Codex **cannot assume the original WAVs exist in the GitHub checkout**. The original files were CRC-validated and compared locally; **do not upload or commit them**. The reference exports have an observed **31-sample** alignment offset; the hosted Linux VST3 has reported **105-sample** latency at 44.1kHz. Align before every signal or envelope comparison; do not independently pick arbitrary lag per candidate.

Snapshot includes StandardCLIP **Soft Clip Pro, clip -1.20dB, 11% saturation, 8× oversampling**; L2 threshold **−2.8dB**, ceiling **−1.1dB**, ARC; API-550A final 5kHz presence boost. Skell uses a **native approximation at 4×**, not copied/proprietary SIR or Waves algorithms. Never equate the owner's +dBTP file with a safe output-ceiling certification.

## Last confirmed measurements — REAL compiled VST3 and owner's full song

Independent `pyloudnorm`, Welch five-band relative spectrum at matched LUFS, true-peak 16× Kaiser polyphase, and 20/50ms window-envelope comparisons. Full-song analysis used matching offsets and the same 0.25–98.75s section. Results are **hosted Linux VST3 output**, not offline EQ predictions.

| Engine / exact source head | LUFS-I | Crest dB | Side/Mid dB | Five-band MAE (dB) | 20ms env MAE (dB) | 16× TP (dBTP) |
|---|---:|---:|---:|---:|---:|---:|
| **Owner five-plugin target** | **−5.493** | **5.438** | **−13.648** | — | — | **~+2.59** (unsafe) |
| Baseline `f606ffd` (CLIP35, no new tone EQ) | −6.233 | 5.449 | −13.656 | 0.178 | 0.821 | −0.386 |
| `2f14809` (two-band EQ; too wide) | −6.261 | 5.588 | −13.374 | **0.103** | **0.804** | −0.426 |
| **`b211203` (two-band + side trim + makeup)** | **−6.253** | **5.564** | **−13.619** | **0.115** | **0.807** | **−0.423** |

- Target vs latest `b211203`: **~0.759 LU too quiet**, **~0.126dB too much crest**, **~0.029dB wider**, five-band error 0.115dB. Full-chain sample peak on Skell is limited to approximately **−0.600dBFS** by factory5's CEILING +3 setting; target is **0.0dBFS** with large intersample overs. Do **not** simply boost output or crush CLIP35 to match the unsafe target's LUFS.
- Latest level-matched band differences (Skell vs target, in order 20–90 / 90–500 / 500–2500 / 2500–9310 / 9310–20000Hz): **+0.072, −0.206, −0.083, +0.105, −0.110dB**.
- Latest 50ms envelope MAE **0.571dB**; latest full-song host CPU ~**5.68% of one core at 44.1kHz on CI Linux**, ~**105 samples PDC**. Shared CI runners are **not** a controlled before/after CPU benchmark.
- The score is not uniform across the song: with the new portable evaluator, `b211203` five-band MAE is approximately **0.387dB at 0.25–20s**, **0.101dB at 20–85s**, and **0.072dB at 85–98.75s**. A global high-shelf adjustment cannot fix the bright intro without harming later sections. **Hold out** the ending when fitting any new coefficients; avoid a static four-filter overfit.
- Real clipper aliasing at isolated CLIP35 was approximately **−118dBc** for 13kHz→5.1kHz third-harmonic foldback (native 4×); with whole factory5 chain the foldback was about **−98dBc**, reflecting nonlinear interaction upstream/downstream. Other tested 16/19kHz folded harmonics were also far below the fundamental; this **does not prove 8× equivalence**. Benchmark CPU, passband, latency and high-drive cases before considering an oversampling change.
- CLIP35 matches target crest/stereo extremely well *before the new EQ*: changing to CLIP65 brings LUFS closer but lowers crest too much. A/B **level matched**, don't blindly optimize for loudness. Factory5 `CLIP=35`, `SAUCE=20` are intentional current baselines.

## DSP state and what changed

- Internal L2-style lookahead: 1.5ms, two-stage release ~65/220ms and stereo-linked attenuation; final true-peak safety guard and separate legacy sample-peak path retain host latency.
- Fixed stale limiter gain when returning from long/hot **dry BYPASS**. At full dry, the engine clears internal limiter gain/hold; active wet mastering remains unchanged. A whole-song pre-fix vs post-fix hosted render nulled to **zero differing PCM samples** with bypass disabled. The windows fixture originally failed because it inherited BYPASS/CEILING from an earlier UI test; it now explicitly sets bypass OFF, Deuce ON, ceiling offset 0.
- Factory5 reference EQ at host rate: **95Hz low shelf −0.10dB**, **1350Hz bell +0.70dB/Q0.78**, after internal limiter, before final true-peak guard. Initial four-filter candidate was **discarded** because it overfit one 84s excerpt and failed the held-out ending.
- Latest `b211203` *experimental* side/makeup stage (also factory5-only): **−0.274dB mid/side side trim**, **+0.15dB pre-final-guard makeup**. It brought width near target while five-band MAE rose from 0.103 to 0.115. Exact-head Linux hosted VST3 real-song analysis **performed**. It is a **candidate, not owner-approved**.
- Other factory IDs and source APVTS parameter IDs remain numerically stable; old preset/host state safety and real FL Studio save/reopen still need verification. The owner explicitly said not to spend time retaining the *old DEFAULT sound*; modern fully zeroed DEFAULT is transparent, with host PDC maintained. **Do not interpret that as permission to rename/reorder automation IDs or overwrite FL Studio projects.**

## Build/QA status and evidence — separate exact heads

- Latest source at handoff: **`b211203a7943996b5e4b005d9b70c0e777160f48`**, unmerged draft.
- **Linux VST3 and hosted test suite PASSED** at source head `b211203`, [workflow 45082535781](https://github.com/skellyruno/skell-master-vst/actions/runs/45082535781). The downloadable Linux reference renderer artifact from that workflow is an actual JUCE VST3 plus local host helper; local full-song `master test.wav` render was analyzed. It is **not a Windows VST3 release**.
- **Windows job was still compiling when this handoff was written** (check that workflow live; do not assume pass/fail). Prior `f606ffd` Windows failed a real `SkellyUITests` ceiling assertion after previous tests left settings modified; code now resets test state, but full exact-head Windows CTest/pluginval **not yet verified** in this handoff.
- Quick4/SkellCore: actual JUCE editor renders are required if UI changes; do not generate illustrative mockups. No UI redesign requested. Windows 620×393, 702×445, 1200×760 screenshots from *earlier* heads are not visual certification of `b211203`.
- Outstanding: full Windows VST3 CTest (9/9), strictness-5 pluginval, real Windows GUI screenshot inspection if UI changes, FL Studio save/reopen, real user A/B listening, cross-rate truepeak/alias/CPU. Preserve main/production workflows and don't merge before passing.

## Reproduction on an actual hosted Linux VST3

Retrieve the matching artifact from run 45082535781, or compile the branch with pinned JUCE/SkellCore. Inside the artifact's extracted folder:

```bash
./render_deuce_master_reference.sh './Skelly Master.vst3' \
  '/private/master test.wav' '/private/skell_b211.wav'
python -m pip install numpy scipy soundfile pyloudnorm
python tools/reference_compare.py \
  --reference '/private/master test w clip.wav' \
  --candidate '/private/skell_b211.wav' \
  --reference-delay 31 --candidate-delay 105 --true-peak \
  --out '/private/match_b211.json'
```

The GitHub source checkout owns `tools/reference_compare.py`; copy that script beside your local render workspace or run with an absolute path. The host runner writes 24-bit WAV and includes latency/tail. It refuses to overwrite existing files. **Never upload those WAVs to Actions or public GitHub.** Target the *four-plugin* original separately by passing `master test preset.wav`; never compare five-plugin candidate to wrong-stage original.

## Codex implementation plan — work, measure, decide

1. **Validate exact head first.** Check Windows CI for `b211203`; fix actual failing compiler/DSP/UI/pluginval errors without relaxing safety thresholds. If Windows succeeded, save the verified Windows VST3 artifact details. Keep draft PR.
2. **Make experiment measurement reproducible** with `tools/reference_compare.py` and the owner's private same-take WAVs. Retest `f606ffd`, `2f14809`, and `b211203`; confirm the table and per-section profiles. Record the precise renderer SHA, offsets, PDC, LUFS/crest/bands/envelope/stereo and independent TP; use a saved deterministic fixed fixture even when private media isn't present.
3. **Optimize against the entire song with multiobjective constraints**, not just five-band MAE. Track train/holdout/intro separately; maintain stereo side/mid within ±0.1dB and crest within ±0.15dB if possible, without materially increasing 20/50ms envelope error or aliasing. Preserve wide-band dynamics and guard stereo coherence.
4. **Investigate root causes**, not a pile of new EQ bands: inspect pre-L2 API2500-like compression, attack/release, five-band Vitamin modeling and post-L2 API550A tone. Inspect whether target/reference files contain nontrivial 24-bit dither/clipping or gain staging. The remaining ~0.76 LU deficit is partly a peak-ceiling disparity; document the achievable sound match under a *safe* dBTP constraint rather than forcing unsafe peaks.
5. **Try one bounded DSP hypothesis at a time**, re-render with the *compiled VST3*, measure 44.1/48/96kHz, verify no CPU spikes/realtime allocations, peaks, bypass, preset state. If it doesn't beat both `f606ffd` and `b211203` in the chosen tradeoff objective, revert it. Avoid changing 4× oversampling to 8× without clear alias improvement vs CPU/latency cost.
6. **Automate local A/B**: make lossless same-section same-take previews, trim PDC and level match to −14 LUFS, keep owner four/five-plugin originals outside voting. Don't publish or include private audio in repo. Ask owner to listen only once meaningfully distinct candidates are available; autonomous development must continue without listening feedback.
7. **Document the change** in this handoff/audit + PR description; commit related changes together, let PR concurrency cancel superseded CI while preserving `main`/manual/production, no force push, no merge of unverified code. Provide direct tested Windows VST3 ZIP and **real JUCE screenshots** when available, never invented links or mocks.

## Do not accidentally destroy progress

- `main` and 12+ previous branches/PRs are **not** your scratchpad. Do not mass-delete branches or merge the PR chain to “clean up.”
- No new APVTS IDs, renaming existing controls, GUI redesign, or changed host plugin class IDs without explicit compatibility verification.
- Never treat CTest success as proof of improved sound, or a screenshot's existence as proof of a good interface; do the actual A/B and screenshot inspection.
- Never upload the private recordings/FL Studio preset into the public repo or CI.
- Do not claim proprietary StandardCLIP/L2/Waves bit-exact emulation.

**Immediate assignment:** confirm Windows CI for the current branch, add the matching reproducible private-audio measurements locally, find the *root DSP reason* the five-plugin reference differs by ~0.76 LU without sacrificing crest/true-peak safety, and make one measured, regression-tested improvement (or report why the safer target cannot exactly match the unsafe source). Update draft PR35; don't merge.
