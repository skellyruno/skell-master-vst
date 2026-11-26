# Consolidation audit — 2026-11-24

The owner authorized auditing, consolidating and merging the verified work on November 24, superseding the earlier draft-only instruction. PR35 is the consolidation vehicle. No force push or history rewrite is used.

## Included work

PR35 previously targeted PR34's branch. Its complete tree contains 371 commits beyond the old common ancestor with main, including the hardware editor, preset/state fixes, true-peak guard, compiled-host tests and current measured reference response. Consolidation targets main directly and uses one squash commit rather than merging every historical experimental stage individually.

The retained source is `b3574b754809a892d7d90f414a960dfcf5ae3c83`. Windows/Linux workflow [45324465511](https://github.com/skellyruno/skell-master-vst/actions/runs/45324465511) passed, including Windows 9/9 CTest, external-WAV validation, pluginval strictness 5 and Linux portable/hosted tests. Tested PR merge was `a58b88af7e8439972e17021789c1f543bb068abe`. Windows artifact ID 14889068449 has ZIP SHA256 `4848cacc0f24a8fa253b646f3becc3300514d03fc857880572a90d931765151c`.

Real local Quick4 images were rebuilt and inspected for default, selected preset, edited controls and supported alternate skin. The preset menu contains five core presets. Older hidden preset identities remain loadable. Automation IDs and latency stay unchanged. No private WAV/FLAC/FST/ZIP is tracked. Measured matching tradeoffs are preserved in the reference recovery audit; no claim of perfect matching or subjective improvement is made.

## Consolidation decisions

- Preserve main's policy: local visual inspection, disabled hosted screenshot workflow, no screenshot-artifact uploads, only PR runs supersede older PR runs, production/manual builds are preserved.
- Retain current Windows/Linux DSP, metadata, hosted audio and pluginval gates. Replace the long obsolete experimental-branch allowlist with PR/manual validation.
- Incorporate the useful parallel packaging fixes from PR4/5: read-only workflow permissions and fail-closed selection of exactly one correctly named AMD64 Release VST3 with valid PE and metadata identity. The offline host helper remains a development tool alongside the VST3, not an extra plugin format.
- Parallel PR6's named program and PR11/13/24's preset-recall intent are implemented and tested in the current source. Do not reintroduce their obsolete alternative state-field implementations.
- Older ancestor PRs are already included in PR35's final tree. Intermediate safety-margin/voicing proposals are superseded by the tested final settings; do not merge those branches individually afterward. Keep their branches/history for provenance.

The main integration changes workflows/documentation only. `Source/`, `tests/` and `CMakeLists.txt` must remain identical to the independently verified response while this audit is completed. The retargeted PR's exact integration checks must pass before merging.

## Remaining release limitations

FL Studio interactive save/reopen and owner listening remain unverified. CPU measurements were collected under variable local load and do not certify audio deadlines. These limitations must remain visible after source integration; merging this verified source does not constitute a signed production release or an assertion that these separate checks passed.
