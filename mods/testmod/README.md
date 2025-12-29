# testmod Mod

This is a custom mod for idTech3.

## Structure

- `scripts/` - Shader files (.shader)
- `maps/` - BSP map files (.bsp)
- `textures/` - Texture files (.tga, .jpg)
- `sounds/` - Sound files (.wav)
- `models/` - Model files (.md3, .md2)

## Usage

1. Place this mod in the `mods/` directory
2. Launch the engine with: `+set fs_game testmod`
3. Or use the launcher: `./idtech3_launcher +set fs_game testmod`

## Packaging

To create a .pk3 file from this mod:

```bash
cd /home/tim/Desktop/idtech3/mods/testmod
zip -r ../testmod.pk3 .
```

Then place the .pk3 file in the `base/` directory.
