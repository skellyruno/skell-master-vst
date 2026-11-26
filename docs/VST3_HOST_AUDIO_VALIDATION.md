# Windows VST3 host audio validation

This developer-only harness loads the compiled VST3 through JUCE's VST3 host, rather than linking the processor directly. It applies the existing SKELL MASTER public parameters and renders through AudioPluginInstance::processBlock with the real JUCE oversampler and post-downsample peak guard.

## Automated synthetic validation
CTest launches SkellyVST3HostAudioTests from the Windows build, renders 9 seconds of deterministic kicks, 61Hz bass, 550Hz body and 9.5kHz air, and records host latency, stereo width, RMS, crest factor, sample peak, reconstructed true peak and processing-time ratio. It checks finite audio and the current sample-peak ceiling. This is a diagnostic only; it does not certify a reconstructed true-peak ceiling.

## Optional private audio render
On a Windows machine after building, invoke SkellyVST3HostAudioTests.exe with three arguments: (1) built Skelly Master.vst3 bundle path, (2) a stereo input WAV path, and (3) an output WAV path. The output is 24-bit PCM and includes the latency and tail. The executable is usually under build/SkellyVST3HostAudioTests_artefacts/Release/.

Compare the output to the user-provided mastered WAV after alignment and equal integrated loudness, including band balance, 50ms/400ms envelopes, crest, stereo width, reconstructed peak, bass pumping, aliasing and latency. The CI version does NOT upload private user WAVs or preset files.

Do not confuse a green synthetic VST3-host test with an approved musical A/B, and do not enable experimental coloration or post-limit tone in old projects by default. The previous presets, APVTS IDs and approved UI are unchanged.

Routine GitHub screenshot workflows stay manual-only; no screenshot build is necessary for sound work.

## Host-state audio equivalence

The hosted VST3 regression creates a fresh plugin instance, restores the saved SKELL MASTER factory state, prepares both instances at the same rate, then renders the same 1.25-second deterministic stereo transient/bass/body/air signal through both. It compares finite output, sample peak and maximum absolute difference. The test repeats after restoring bypass state. This catches hidden mode/state drift that a parameter-value-only comparison can miss. It is a JUCE-hosted VST3 test, not an FL Studio session test.

## Linux local-reference renderer

PR #17 adds a Linux VST3 host target and a CI artifact containing the Linux VST3 bundle plus render_deuce_master_reference.sh. After its first CI build passes, use the packaged script to render a local stereo WAV through the compiled VST3. The input WAV is never uploaded to GitHub; the workflow only builds and tests with generated synthetic audio. The output is 24-bit PCM and includes the plugin's reported latency and safety tail. This is a Linux JUCE-hosted VST3 render, not a Windows or FL Studio render. The Windows build remains the Windows packaging gate.
