# Configuration

The mod stores its settings and per-car setups in `fh3-mods/Underglow.ini` next to the other FH3 mods. Keep that file when updating. The shipped DLL has defaults for a fresh install, and the [sample INI](../src/fh3-mods/Underglow.ini) shows the available settings.

## Custom colors

Close Azimuth before editing the INI, then restart it to load your changes. The ten custom slots use `Color1` through `Color10` and the corresponding `Name1` through `Name10`. Leave the name blank to keep the default slot label. Colors accept hex, RGB channels from 0 to 255, or normalized RGB channels from 0 to 1.

```ini
[CustomColors]
Color1=#FF69B4
Name1=Hot Pink
Color2=30 180 255
Name2=Ice Blue
```

## Placement

| Setting | Default | Purpose |
| --- | ---: | --- |
| `AutoFit` | `1` | Follow the car's fitted bodywork. Set to `0` to use fixed dimensions. |
| `Height` | `0.01` | Height of the strips above the road, in meters. |
| `FrontInset` | `0.5` | Front clearance from the fitted bumper. |
| `RearInset` | `0.7` | Rear clearance from the fitted bumper. |
| `SideOffset` | `-0.2` | Move the side strips inward or outward. |
| `SideTrim` | `0.8` | Shorten the side strips at their ends. |
| `Range` | `1.0` | Distance each light reaches. |
| `Power` | `4.0` | Light power. |

Use **X (Options)** in Underglow Select for brightness and effect speed. **A (Apply)** saves the adjustment and **B (Back)** restores the saved value.
