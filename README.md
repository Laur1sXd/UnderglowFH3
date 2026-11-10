# Underglow for FH3

Add neon underglow to your cars in Forza Horizon 3 on Azimuth. The mod uses the game's lights to illuminate the road, and the Garage lets you preview and save a different setup for each car.

[Effect catalog](docs/EFFECTS.md) · [Configuration](docs/CONFIGURATION.md) · [Build from source](#build-from-source)

## Install

1. Use the FH3 Xbox One build **1.0.125.0** offline in Azimuth. Install FH3 Mod Platform first.
2. Close Azimuth and extract one install ZIP into the folder containing `azimuth.exe`. The mod goes in `fh3-mods/Underglow.dll`.
3. If upgrading, keep your existing `fh3-mods/Underglow.ini`. Move any older Underglow DLL with a different filename out of `fh3-mods`.
4. Start Azimuth and open **Garage → Designs and Paints → Underglow**.

## In the game

| Where | What you can do |
| --- | --- |
| **Designs and Paints → Underglow** | Pick primary and secondary colors from 18 presets and 10 custom slots. Browse 46 visual effects and apply them to all parts or one part. |
| **X (Options)** | Adjust brightness and, for animated effects, effect speed. The car previews the changes while you edit. |
| **Previous Underglows** | Recall a setup with its colors, effect, placement, brightness, and speed. Press **X (Delete)** to manage equipped and previous setups. |
| **ANNA → Car → Underglow → Audio Visualizer** | Choose from 15 effects that react to the car radio. Equip a setup in the Garage first. |
| **ANNA → Car → Lights** | In the ExtraOptions edition, switch the equipped glow off or back on. |
| **MODS → Installed mods → Underglow** | Switch the front, rear, left, and right lights individually. |

Highlight a swatch to preview it, then press **A (Apply)** to equip it. A fresh install starts with each car's glow off. Select **No Underglow** to clear a color. To remove an equipped setup while keeping older ones, choose **Delete Equipped Underglow** from the history tab.

## How it works

Underglow attaches lights to the local player's car. Placement follows the fitted bumpers and side skirts when their bounds are available. The mod saves each car's appearance and history separately from its paint and livery. Audio effects read the active car-radio signal, and the ExtraOptions edition also supports Groove Music Radio.

## Build from source

Install Visual Studio Build Tools with **Desktop development with C++** and the Windows SDK, then run these commands from the repository folder in Command Prompt.

```bat
build.cmd Standalone
build.cmd ExtraOptions
```

Each command writes `src/build/Underglow.dll`. Both editions share the code in [src](src). The edition switch selects the ExtraOptions integrations at compile time. See [the build workflow](.github/workflows/build.yml) for the same two builds on GitHub Actions.

## Credits

- **FH2Underglow** by Bartholomew Ho provided the effects and palette adapted for this project.
- **FH3 Mod Platform** by Levilente provides the loader and SDK. The bundled SDK retains Trinculo54's upstream MIT notice.
- **FH3 ExtraOptions** by Bartholomew Ho provides the shared ANNA menus used by the ExtraOptions edition.

For a bug report, include your edition, FH3 Mod Platform version, and steps to reproduce it. [Open an issue](https://github.com/Laur1sXd/UnderglowFH3/issues/new)
