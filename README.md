# Underglow for FH3

This is the original **Underglow 1.0.0** source, matching the ExtraOptions edition distributed as `Underglow_1.0.0.zip` in the `v1.0.0` release.

Add neon underglow to your cars in Forza Horizon 3 on Azimuth. Preview and equip a different setup for each car through **Garage → Designs and Paints → Underglow**.

[Effect catalog](docs/EFFECTS.md) · [Configuration](docs/CONFIGURATION.md) · [Original documentation](docs/ORIGINAL-README.md) · [Build from source](#build-from-source)

## Install

1. Use the FH3 Xbox One build **1.0.125.0** offline in Azimuth with FH3 Mod Platform.
2. Close Azimuth and extract the install ZIP beside `azimuth.exe`. It installs `fh3-mods/Underglow.dll`.
3. Keep an existing `Underglow.ini` when updating, and move older Underglow DLLs with different names out of `fh3-mods`.
4. Open **Designs and Paints → Underglow**. Highlight a color or effect to preview it and press **A (Apply)** to equip it.

This historical ExtraOptions build targets **FH3 ExtraOptions 0.4.4**, including its shared ANNA menus, NoPause and Groove Music Radio support. Its requirements and behavior are those of version 1.0.0.

## In the game

| Where | What you can do |
| --- | --- |
| **Designs and Paints → Underglow** | Choose primary and secondary colors, visual effects and the parts that glow. |
| **X (Options)** | Preview and adjust brightness, plus effect speed for animated effects. |
| **Previous Underglows** | Recall earlier colors, effects, placement and saved slider values, or delete setups. |
| **ANNA → Car → Underglow → Audio Visualizer** | Select one of 15 effects that react to your car radio. |
| **ANNA → Car → Lights** | In the ExtraOptions build, switch the equipped glow off or on. |
| **MODS → Installed mods → Underglow** | Switch the front, rear, left and right parts individually. |

Placement follows the fitted bumpers and side skirts where their bounds are available. The source includes 46 visual effects and 15 radio-reactive effects. Colors, equipped setups and history are stored separately from paint and liveries in `fh3-mods/Underglow.ini`. See the original documentation for this version's controls, compatibility and configuration details.

## Build from source

Install Visual Studio Build Tools with **Desktop development with C++** and the Windows SDK. In Command Prompt, run:

```bat
build.cmd ExtraOptions
```

This builds the edition used by the release. Running `build.cmd` without an argument also builds ExtraOptions. The shared code can additionally be built with `build.cmd Standalone`. Each command writes `src/build/Underglow.dll`.

Source files are in [src](src), configuration examples are in `src/fh3-mods`, and the build workflow is in `.github/workflows/build.yml`. Both GitHub-generated source archives include these files and the existing license notices.

## Credits

- **FH2Underglow** by Bartholomew Ho provided the effects and palette adapted for this project.
- **FH3 Mod Platform** by Levilente provides the loader and SDK; bundled upstream license notices are retained.
- **FH3 ExtraOptions** by Bartholomew Ho provides the shared ANNA menus and add-on integration used by this edition.

See [CREDITS.md](CREDITS.md), [Licenses](Licenses) and the SDK license for the original terms and attribution.
