# Skell Master — release-candidate audit entry

Owner considers the sound ready and requests final menu polish, SKELL MAGIC in the upper-left and original Clip/Output/Ceiling positions. Release freeze: no changes to mastering algorithms, automation IDs, factory numbers, existing serialized DSP mode, or latency in this change. Retain approved V16 sound and V18 exact reference excerpt. V19 creative mode remains opt-in, not a new default. Release candidate source remains draft PR35. No merge/release tag before gates.

## This polish batch
- SKELL MAGIC label/tooltip; same internal mode ID and placement.
- Original utility positions exactly: Clip 371,685,178,68; Output 647,685,199,68; Ceiling 929,685,181,68 at design1200x760. Restore labels/dividers from V19. Left space engraved MASTER BUS identification only.
- Consistent dark hardware popup palette, legible highlight text, minimum widths and row height; quality section heading; factory and saved-preset commands separated. Keep factory IDs 1..5.
- Suppress duplicate/overlapping menu and chooser requests, safe async teardown, sanitize default filename from recalled display name. Do not change persisted preset contents or existing files.
- Parent fixes Windows MSVC C3493 explicit lambda captures. V20 Linux CI passed; Windows failed compile. V21 pending at start. This candidate needs its own exact-head build.

## Audit checklist / evidence required
1. Windows Release + Linux exact candidate build, pluginval and complete CTest, package SHA256 and ZIP integrity, correct VST3 manufacturer/class IDs and version metadata. Check license notices and asset provenance; imported LYNX AGPL source remains excluded.
2. Actual local native editor capture at 620/702/1200 and alternate skin, real preset/quality menus, SKELL MAGIC on/off and active meters. Inspect/fix/reinspect. Save, overwrite confirmation, cancel, browse, recall, asterisk, same factory reselect, popup dismiss and reopen. Local JUCE git retrieval DNS failed; use actual hosted compiled editor when available. No mockups.
3. Hosted real music A/B current reference vs V16/V18/V19, sample-aligned PCM null at default reference, explicit creative mode, safety at 2/4/8x across sample rates, repeat CPU and latency. No private recordings in Git.
4. FL Studio Windows real-time insert/remove, bypass, project save/reopen, old presets and automation, quality change on host prepare, render/export. Owner listening approval of creative mode optional, but must not alter current approved reference.
5. Verify all prior unresolved work: RMS MATCH commit was not advanced, not part of promised release; document deferred advanced per-band controls. Do not expose unfinished controls. Release notes/install instructions; Windows installer/bundle scan. Draft remains unmerged.

Current gates are not a release certification. Exact candidate CI, screenshot and FL Studio validation remain pending. No private audio uploaded.