# Underglow 1.0.0 for Forza Horizon 3

This edition of Underglow adds an **UNDERGLOW** tile to **Garage → Designs and Paints**. The tile opens **UNDERGLOW SELECT** with the full-car **Paint Body** camera. The mod targets FH3 Xbox One **1.0.125.0** running offline in Azimuth with FH3 Mod Platform. Its MODS settings recognize platform **1.0.0 (API 15)** and **1.2.1 (API 16)**.

This Garage edition supports Extra Options 0.4.4 and its NoPause and Groove Music Radio add-ons. Underglow appears in the shared ANNA Car and Lights menus. For Groove, choose a playlist in **Audio Options**, tune to **Groove Music**, and apply an **Audio Visualizer** effect in ANNA.

This edition supports a separate Horizon 3 Extended installation. The visualizer uses the native FH3 radio path.

## Install

1. Close Azimuth and move any existing `Underglow.dll` or `UnderglowGarage.dll` out of `fh3-mods`.
2. Extract this ZIP into the Azimuth folder containing `azimuth.exe`. Keep your existing FH3 Mod Platform loader. The release ZIP installs `fh3-mods/Underglow.dll`. Keep any existing `Underglow.ini`. The mod uses built-in defaults on a fresh install.
3. Open **Designs and Paints → UNDERGLOW**. Apply a color or effect to start a setup for the current car. All choices are free.
4. **No Underglow** is the first swatch in both color tabs. Highlight it to preview clearing that color and press **A (Apply)** to keep the change. Clearing the last color turns the glow off and saves the earlier setup in **PREVIOUS UNDERGLOWS** so you can reapply it later.

Underglow starts with Mod Platform's **Hot Reload** enabled or disabled. Restart Azimuth after replacing the DLL.

## Colors and effects

- Use **LB/RB** to switch between **PRIMARY UNDERGLOW**, **SECONDARY UNDERGLOW**, **EFFECTS WITH 1 COLOR**, **EFFECTS WITH 2 COLORS**, **MULTICOLOR EFFECTS**, and **PREVIOUS UNDERGLOWS**. Highlight a swatch to preview it on the Paint Body camera. Back out of Underglow Select to restore the equipped setup.
- The Primary and Secondary tabs sort presets by hue and list custom colors in slot order from **Custom 1** through **Custom 10**. Opening either color tab highlights its applied color or **No Underglow** if that channel has no applied color. Press **A** to apply the color to the current tab's channel. Set custom colors and names under `[CustomColors]` in `Underglow.ini` and restart Azimuth to load them.
- The three effect tabs contain 46 visual patterns. **EFFECTS WITH 1 COLOR** requires an applied primary color. **EFFECTS WITH 2 COLORS** requires both an applied primary and secondary color. Press **A (OK)** on each lock message. The selector jumps to the next available tab in the direction you pressed. Returning left from **MULTICOLOR EFFECTS** lands on **EFFECTS WITH 1 COLOR** if Primary is applied, or **SECONDARY UNDERGLOW** if Primary is unset. Clearing a color switches any effect that requires it to Static on the same parts. Two-color effects use a contrasting accent when both colors match. Choose **All** to apply an effect across the car. Choosing **Front**, **Rear**, **Left Side**, or **Right Side** lights only that part.
- **PREVIOUS UNDERGLOWS** keeps earlier setups for the current car without a fixed entry limit. Completing a new setup archives the earlier equipped setup. Reapplying a previous setup equips it, discards the setup it replaces, and returns to Primary with the recalled glow visible. Each swatch label shows **Primary / Secondary / Effect / Placement**. Previewing or applying a saved setup also uses its saved Brightness and Effect Speed. Older entries created before slider snapshots use the current slider values. Press **X (Delete)** on a previous setup. Choose **Delete Equipped Underglow** to turn off the current setup and keep your previous setups, or **Delete This Previous Underglow** to remove the highlighted entry and keep the current setup equipped. Selecting an empty history tab or deleting its final entry opens an **A (OK)** popup.
- With underglow enabled for the current car, open **ANNA → Car → Underglow → Audio Visualizer** in free roam to select one of 15 stock radio-reactive effects. Applying a garage effect to **All** enables only **All** in ANNA. Applying it to an individual part enables only that part. A color-only setup allows every target. Gray targets cannot be selected. The visualizer uses the car's radio and the colors from Underglow Select. **Off** restores the saved visual setup for the selected target. Applying a visual effect in Underglow Select clears existing audio effects on that car so the chosen placement takes effect. The ANNA row is gray until a garage color or effect is applied.
- **Underglow** is the last row in **ANNA → Car**. The Extra Options editions also add an **Underglow** switch under **Car → Lights** that keeps the car's saved setup when switched off. The switch is gray until a setup is applied. In Standalone editions, choose **No Underglow** in the garage to remove the equipped setup.
- In **UNDERGLOW SELECT**, highlight a color or effect and press **X (Options)** to open the Fine Tune-style sliders. Primary, Secondary, and Static offer **Brightness**. Animated effects in all three effect tabs also offer **Effect Speed**. The selected animation fills the track at its current pace. The car previews the adjustments. Press **A (Apply)** to save or **B (Back)** to restore the saved values.
- **MODS → Installed mods → Underglow** exposes the Front, Rear, Left, and Right switches. On verified layouts, the switches preview while selected. Press **A (Accept)** to keep the change. Moving to another row or pressing **B** restores the saved value. On other layouts that support the Mod Platform settings API, highlight **Apply Underglow Settings** after editing. Press **Right** to apply or **Left** to discard.

The front and rear strips follow the fitted bumpers, and the side strips follow the fitted side skirts. When those bounds are unavailable, the renderer uses the active body or mounted lights. In AutoFit, the bumper strips use `FrontInset` and `RearInset` as edge clearance, capped at a quarter of the body length on short cars. The side strips use 15% of `SideTrim` at each skirt end and 85% when using the fitted body or light bounds. The defaults are `Brightness=0.5`, `Height=0.01`, `FrontInset=0.5`, `RearInset=0.7`, `SideOffset=-0.2`, `SideTrim=0.8`, `Range=1.0`, and `Power=4.0`.

The strips form straight rows below the car. With the default `SideOffset`, the side strips sit 20 cm inside the fitted skirts or body edge. The same footprint and range apply across the Brightness slider. Audio effects vary the light output with the radio.

## Save files and updates

`fh3-mods\Underglow.ini` stores settings, custom colors, and per-car setups and history. Underglow uses its own save data and hides the View livery-save action throughout its selector. Keep the INI with the corresponding Azimuth save and replace only the DLL when updating.

When upgrading from the old Garage build, keep `UnderglowGarageSettings.ini` and `UnderglowGarage.ini` in `fh3-mods` for the first launch. Install the new DLL alone if you want to import the old Garage settings. The mod copies them when `Underglow.ini` is absent and imports the old history when `Underglow.ini` has no Garage records. If `Underglow.ini` already exists, its settings take precedence. You can move the old files out after the import.

## Build

Run `build.cmd` with Visual Studio Build Tools and the Windows SDK installed. The DLL is written to `build/Underglow.dll`.

The effects and palette are adapted from [FH2Underglow](https://github.com/qiangqiang101/FH2Underglow) by Bartholomew Ho. See `LICENSE-FH2Underglow` for the MIT terms.
