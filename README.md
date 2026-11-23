# Underglow for FH3

This is the **Underglow 1.0.2** source prepared for the `v1.0.2` draft release. The final install file is named `Underglow_v1.0.2.zip` and uses the ExtraOptions-compatible build.

Customize each car in **Garage → Designs and Paints → Underglow**. Highlight colors and effects to preview them, then press **A (Apply)** to equip them. **X (Options)** adjusts brightness and animation speed, and **Previous Underglows** recalls saved setups.

[Effect catalog](docs/EFFECTS.md) · [Configuration](docs/CONFIGURATION.md) · [Detailed compatibility notes](src/ANNA-1.0.2-Compatibility.txt) · [Auto Drive fix](src/Auto-Drive-Fix.txt)

## Install

Use FH3 Xbox One 1.0.125.0 offline in Azimuth with FH3 Mod Platform, compatible FH3 ExtraOptions and the separate official ANNA plugin. Close Azimuth and extract the install ZIP beside `azimuth.exe`, replacing `fh3-mods/Underglow.dll`. Keep your settings and saves.

The official ANNA 1.0.2 plugin/API supplies the audio-effects menu and automatic scrolling. Install its complete package, including `media/UI/Controls/HUDAnnaControl.xaml`, to display the scroll arrows. Underglow does not bundle or replace the platform, ExtraOptions or ANNA DLLs.

## Version 1.0.2

- Equipped setups and history are isolated by save. New offline/FH3MP careers start unequipped, including when a new car reuses an earlier garage ID.
- Save tracking leaves the native SQL function available to Mod Platform's custom-upgrade hook.
- Audio controls use the separate official ANNA API, with ANNA 1.0.2 scrolling long submenus.
- The Auto Drive correction recognizes the owned local car in manual and AI-control modes and refreshes its moving presentation while human-control ticks are inactive.

Colors, visual and audio effects, part selection, brightness, speed and No Underglow behavior are retained. Source and native-memory regression tests passed, and the Auto Drive fix has been confirmed working in game with the fixed DLL.

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
