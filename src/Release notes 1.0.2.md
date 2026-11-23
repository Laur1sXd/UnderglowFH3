# Underglow 1.0.2 — Garage Editions

## New

- Save-specific equipped setups and Previous Underglows. Offline and FH3MP careers use separate Underglow data, even when their cars share the same garage IDs.

## Fixed

- Fresh saves no longer inherit an equipped Underglow from an older save. Every player car starts unequipped until you apply a setup in Designs and Paints → UNDERGLOW.
- Previous Underglows keeps each save's own history, including the colors, effects, parts, Brightness and Effect Speed saved with those previous setups.
- Cached equipped/preview state is blocked when the active save identity changes.

## Changed

- All four Garage editions now register as 1.0.2 in game, with matching install/source ZIP names. GitHub update checking remains enabled.
- The save identity is stored in native career data so a normal FH3MP cloud copy can retain it. The identity marker itself equips nothing.
- Older unassigned entries in Underglow.ini are preserved but never automatically equipped. Reapply desired old setups once after updating.

The 1.0.2 packages include the save-isolation fix just completed in the updated local 1.0.1 build; the version bump adds no further gameplay changes.

## Updating

Close Azimuth and replace fh3-mods/Underglow.dll using one matching edition. Keep Underglow.ini and your FH3 saves. The DLL and INI names remain Underglow.dll and Underglow.ini.

Validation: all four DLLs rebuilt and their registered 1.0.2/update-check metadata verified. Source gameplay logic is unchanged from the preceding save-isolation build, whose save and MODS/F9 fixtures passed in all four configurations. Live FH3/FH3MP verification of the save fix is still pending. The installed game was not modified, and no GitHub release was uploaded.
