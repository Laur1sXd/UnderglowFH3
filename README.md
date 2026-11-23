# Underglow for FH3

Customize each car in **Garage → Designs and Paints → Underglow**. Highlight colors and effects to preview them, then press **A (Apply)** to equip them. **X (Options)** adjusts brightness and animation speed, and **Previous Underglows** recalls saved setups.

[Effect catalog](docs/EFFECTS.md) · [Configuration](docs/CONFIGURATION.md) · [Detailed compatibility notes](src/ANNA-1.0.2-Compatibility.txt)

## Install

Use FH3 Xbox One 1.0.125.0 offline in Azimuth with FH3 Mod Platform, compatible FH3 ExtraOptions and the separate official ANNA plugin. Close Azimuth and extract the install ZIP beside `azimuth.exe`, replacing `fh3-mods/Underglow.dll`. Keep your settings and saves.

The official ANNA 1.0.2 plugin/API supplies the ANNA menu and automatic scrolling: **Car > Underglow** has Primary Color and Secondary Color lists, and an Effect list: pick all parts or one, then a group (Audio Visualizer, 1 Color, 2 Colors, Multicolor) and an effect, as in the Garage. The Garage's UNDERGLOW page has an Audio Visualizer tab for the radio-reactive effects. Audio effects read the car radio, and Groove Music Radio too when it is installed (every edition). Install its complete package, including `media/UI/Controls/HUDAnnaControl.xaml`, to display the scroll arrows. Underglow does not bundle or replace the platform, ExtraOptions or ANNA DLLs.

## Build from source

Install Visual Studio Build Tools with **Desktop development with C++** and the Windows SDK. In Command Prompt run:

```bat
build.cmd
```

The command builds the one `Underglow.dll` the package ships (native radio) and writes it to `src/build/Underglow.dll`. It uses the official ANNA plugin when it finds it and works without it (Garage only, or the game's own ANNA menu). The implementation and reproducible verification scripts are in [src](src).

## Credits and licenses

- **FH2Underglow** by Bartholomew Ho provided the effects and palette adapted for this project.
- **FH3 Mod Platform** by Levilente provides the loader and SDK; bundled upstream notices are retained.
- **FH3 ExtraOptions** and **FH3-ANNA** by Bartholomew Ho provide the supported host/plugin APIs.

See [Licenses](Licenses), the original notices and `src/sdk/LICENSE` for the existing terms and attribution.
