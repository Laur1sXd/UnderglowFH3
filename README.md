# Underglow for FH3

Customize each car in **Garage → Designs and Paints → Underglow**. Highlight colors and effects to preview them, then press **A (Apply)** to equip them. **X (Options)** adjusts brightness and animation speed, and **Previous Underglows** recalls saved setups.

[Effect catalog](docs/EFFECTS.md) · [Configuration](docs/CONFIGURATION.md) · [Detailed compatibility notes](src/ANNA-1.0.2-Compatibility.txt)

## Install

Use FH3 Xbox One 1.0.125.0 offline in Azimuth with FH3 Mod Platform, compatible FH3 ExtraOptions and the separate official ANNA plugin. Close Azimuth and extract the install ZIP beside `azimuth.exe`, replacing `fh3-mods/Underglow.dll`. Keep your settings and saves.

The official ANNA 1.0.2 plugin/API supplies the audio-effects menu and automatic scrolling. Install its complete package, including `media/UI/Controls/HUDAnnaControl.xaml`, to display the scroll arrows. Underglow does not bundle or replace the platform, ExtraOptions or ANNA DLLs.

## Build from source

Install Visual Studio Build Tools with **Desktop development with C++** and the Windows SDK. In Command Prompt run:

```bat
build.cmd
```

The command builds the ExtraOptions-compatible native-radio edition and writes `src/build/Underglow.dll`. The implementation and reproducible verification scripts are in [src](src). `src/Build-Editions.cmd` retains the full development build matrix.

## Credits and licenses

- **FH2Underglow** by Bartholomew Ho provided the effects and palette adapted for this project.
- **FH3 Mod Platform** by Levilente provides the loader and SDK; bundled upstream notices are retained.
- **FH3 ExtraOptions** and **FH3-ANNA** by Bartholomew Ho provide the supported host/plugin APIs.

See [Licenses](Licenses), the original notices and `src/sdk/LICENSE` for the existing terms and attribution.
