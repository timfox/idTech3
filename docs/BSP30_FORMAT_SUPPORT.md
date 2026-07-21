# BSP30 and WAD3 format support

The engine's BSP30 and WAD3 readers are clean-room file-format parsers. They
are implemented in the engine's existing GPL-licensed code and do not include,
link to, or depend on the Half-Life SDK, its headers, or its libraries.

Format compatibility does not grant rights to game content. BSP maps, WAD
textures, sounds, models, and other assets retain their respective copyrights
and licenses. The engine repository does not bundle Valve or Counter-Strike
WAD files.

## Textures

BSP30 maps may either embed each indexed texture and palette or store only a
texture name and dimensions. For names-only textures, the renderer reads the
worldspawn `wad` key, discards its historical absolute directories, and looks
only for each basename under the active game's `wads/` directory. For example:

```text
surf/wads/de_aztec.wad
surf/wads/cstrike.wad
```

Only uncompressed WAD3 mip-texture entries are decoded. Users must supply any
external WADs themselves and are responsible for having permission to use
them. If a referenced WAD or texture is absent, the renderer generates a
clearly visible, name-stable checker material so the map remains navigable
without proprietary assets.

## Collision

Point queries use the BSP30 render-node tree. Player and box sweeps use the
map's precomputed clipnode hull and add only the residual idTech3 box extent.
This preserves surf ramps and edges without importing an external physics or
SDK implementation.
