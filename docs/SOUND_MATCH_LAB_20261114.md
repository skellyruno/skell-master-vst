## Integration status — 2026-11-19

The initial experiment below describes the two processors as isolated prototypes. On the current Deuce Match branch they are now connected to the production audio path, but remain opt-in:

- `FiveBandColorPrototype` runs in the existing 4x oversampled path only when the saved/automatable `deuceMatch` parameter is enabled.
- `PostLimiterPresencePrototype` runs after downsampling and before the final true-peak guard, under the same opt-in parameter.
- The original four factory presets and migrated legacy sessions keep Deuce Match OFF. Only the appended `SKELL MASTER` factory enables it.
- The output guard now uses a 257-tap / 4-phase Kaiser detector with a 32-sample lookahead, checked against the independent full-band reconstruction regression. The 0.8 dB margin remains additional safety headroom, not a claim of exact mastering match.

undefined