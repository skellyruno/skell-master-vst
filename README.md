# Skell Master — M-01

Skell Master is a JUCE 8 / C++17 mastering effect with native 4x oversampling, stereo-linked dynamics, saturation, clipping, output safety and a resizable hardware-style editor. Windows VST3 is the packaged plugin format. Standalone and the local editor renderer are development tools.

## Current presets and controls

The menu contains **DEFAULT**, **WARM MASTER**, **PUNCH MASTER**, **LOUD MASTER** and **SKELL MASTER**. Edited settings display **CUSTOM**. Selecting a preset again restores its factory settings. Historical extra preset identities remain supported in saved states but are hidden from the menu. There is no separate reapply action or plus-preset menu.

The eleven controls are Input, Drive, Glue, Dynamics, Lows, Highs, Sauce, Transients, Clip, Output and Ceiling. Existing automation IDs and preset identities are preserved. A fully zeroed DEFAULT uses the exact delayed input, including its original peaks. Its transparent path does not provide peak protection.

Output changes gain before the clipper/limiter. Ceiling adjusts the limiting target: displayed zero corresponds to -1 dB, with rate-dependent safety margins in true-peak-protected modes. A ceiling setting is not the same as the measured final true peak. No width control has been added.

## Reference response and compatibility

SKELL MASTER uses Input +5 dB, Drive 8%, Glue 20%, Dynamics 11%, Lows -1 dB, Highs +3 dB, Sauce 12%, Transients 0%, Clip 35%, Output 0 dB and Ceiling offset +3. The saved Deuce Match mode activates reference-chain coloration and final true-peak protection.

The v9 factory 5 response adds measured limiter recovery and a causal Clip-dependent phase profile. Existing v8 states retain their previous calibration. Select SKELL MASTER explicitly to use the new response. Other historical clip topologies remain versioned in saved state.

Actual compiled Windows VST3 full-song comparisons used separate no-Clip and five-plugin references, fixed alignment and held-out sections. At Clip 35, 20 ms envelope error improves from 0.764 to 0.530 dB, while five-band tonal error rises from 0.098 to 0.127 dB. Safe output remains approximately 0.66 LU quieter than the reference. Clip zero improves whole-song crest, loudness and band/envelope errors. These measurements do not define a percentage of subjective similarity or proprietary algorithm equivalence.

- [Measurements and regressions](docs/REFERENCE_RECOVERY_PHASE_20261123.md)
- [Consolidation audit and verification provenance](docs/CONSOLIDATION_AUDIT_20261124.md)
- [Engineering handoff and historical evidence](CODEX_HANDOFF.md)
- [Private-audio comparison tool](tools/reference_compare.py)

Private recordings and listening previews stay outside GitHub. FL Studio interactive save/reopen and owner listening remain unverified release checks. CPU figures were collected under variable machine load. Source integration is not a signed production release or a claim of universal host compatibility.

## Build and validation

The build fetches pinned JUCE 8.0.12 and SkellCore sources. On Windows with Visual Studio C++ and CMake installed:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

An existing JUCE source checkout can be supplied with `-DJUCE_PATH=C:/JUCE`. Windows checks include portable DSP, state migration, actual editor behavior and compiled VST3 hosting at 44.1/48/96 kHz. CI also validates metadata, external WAV rendering, strictness-5 pluginval and package identity. Windows packages contain the complete `Skelly Master.vst3` directory, build provenance and an optional offline render helper. Install the whole VST3 bundle into the host's VST3 location and rescan; the helper executable is not the plugin.

For current UI inspection, enable `-DSKELLY_MASTER_BUILD_UI_RENDERER=ON`, build the `SkellyMasterUIRenderer` target, and run `build/Release/SkellyMasterUIRenderer.exe --output ui-screenshots`. Four scenes render the actual compiled editor. Add `--full` for the responsive audit. Hosted screenshot jobs and screenshot uploads remain disabled; DSP and release checks run independently. See [AGENTS.md](AGENTS.md) for the source-of-truth policy. Review JUCE and project licensing before commercial distribution.
