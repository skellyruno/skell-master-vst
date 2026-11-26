# SKELL MASTER — photographic asset-kit integration

This branch keeps the existing **1200×760 design grid**, compact default size,
10 APVTS rotary controls, two buttons, live meter needles, and unchanged
`PluginProcessor`/DSP.

## Asset-kit location

The reference art is delivered separately as
`SKELL_Master_Professional_Asset_Kit.zip`. Copy these three **PNG files**
from the kit into `Source/UI/Assets/` using these exact destinations:

| In supplied kit | Repository target |
|---|---|
| `Assets/Panels/header_nameplate_green_blank.png` | `Source/UI/Assets/header_nameplate_green_blank.png` |
| `Assets/Logos/logo_skull_header_left.png` | `Source/UI/Assets/logo_skull_header_left.png` |
| `Assets/Logos/logo_skull_header_right.png` | `Source/UI/Assets/logo_skull_header_right.png` |

The first artwork is the **emerald-green** reconstruction so it matches the
current SKELL MASTER design. The original red artwork remains in the asset
kit but is not used. Source code does not duplicate or flatten the whole UI.

CMake detects whether **all three** images are present and, if so, embeds
them through `juce_add_binary_data`. If any are absent, existing procedural
artwork is retained and the build remains functional. No runtime filesystem
dependency is introduced.

## White readouts

All **numeric** live readout text is now near-white:
- the 10 APVTS slider values;
- analog meter dBFS numeric overlays;
- gain-reduction number and its dB units.

The gain-reduction bars and active engine-status LED remain green, and the
analog meter face remains warm amber. This is UI-only.

## Build/verify

```sh
cmake -S . -B build-ui -G Ninja -DCMAKE_BUILD_TYPE=Release -DSKELLY_MASTER_BUILD_UI_RENDERER=ON
cmake --build build-ui --target SkellyMasterUIRenderer --parallel 3
xvfb-run -a ./build-ui/SkellyMasterUIRenderer --output ui-screenshots
ctest --test-dir build-ui --output-on-failure
```

On Windows, the repository's Windows workflow builds and tests the VST3.
For asset-specific screenshots, first copy the three PNGs so CMake enables
`SKELLY_MASTER_HEADER_ASSETS`. The Windows UI test then checks PNG decode,
dimensions, and transparent skull corners. Verify the actual editor renders
at 620×393, 702×445, 1200×760, and 1800×1140 using SkellCore.

The 40 other art kit files are reusable stock, not blindly stacked over the
working editor; live needles and APVTS widgets must remain interactive.

Do not infer production sound quality from visual changes. Confirm DAW
behavior and VST3 artifact before release.
