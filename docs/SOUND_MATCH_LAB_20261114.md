## Integration status — 2026-11-19

The initial experiment below describes the two processors as isolated prototypes. On the current Deuce Match branch they are now connected to the production audio path, but remain opt-in:

- `FiveBandColorPrototype` runs in the existing 4x oversampled path only when the saved/automatable `deuceMatch` parameter is enabled.
- `PostLimiterPresencePrototype` runs after downsampling and before the final true-peak guard, under the same opt-in parameter.
- The original four factory presets and migrated legacy sessions keep Deuce Match OFF. Only the appended `SKELL MASTER` factory enables it.
- The independent full-band true-peak oracle is being strengthened; do not treat the current 0.8 dB margin as final until the independent Kaiser test passes.

undefined