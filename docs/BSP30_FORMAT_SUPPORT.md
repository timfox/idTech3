# BSP30 and WAD3 format support

The engine's BSP30 and WAD3 readers are clean-room file-format parsers. They
are implemented in the engine's existing GPL-licensed code and do not include,
link to, or depend on the Half-Life SDK, its headers, or its libraries.

Format compatibility does not grant rights to game content. BSP maps, WAD
textures, sounds, models, and other assets retain their respective copyrights
and licenses. The engine repository does not bundle Valve or Counter-Strike
WAD files.

## Lighting

BSP30 faces carry `styles[]` + `lightofs` into `BSP30_LUMP_LIGHTING` (RGB luxels,
16-unit GoldSrc grid). The renderer samples style 0 into **vertex colors**
(`LIGHTMAP_BY_VERTEX`) so ordinary surfaces are not fullbright albedo. Special /
unlit faces remain white. See `docs/GHOST_FULLBRIGHT.md`.

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

## Sky and HDR environment

BSP30 sky brushes use the GoldSrc texture name `sky` (or `sky*`). The loader
assigns those faces a sky shader (`RB_StageIteratorSky`) instead of a diffuse
WAD material.

Worldspawn keys:

| Key | Description |
|-----|-------------|
| `skyname` | Classic `gfx/env/<name>{rt,bk,lf,ft,up,dn}` faces (HL naming; Q3 `_rt` also tried) |
| `skybox_hdr` | Path to an OpenEXR (`.exr`) or Radiance (`.hdr`) equirectangular panorama |
| `skybox_hdr_exposure` | Exposure multiplier |
| `skybox_hdr_rotation` | Yaw degrees |
| `skybox_hdr_intensity` | IBL intensity |
| `skybox_hdr_projection` | `0` equirect (default; auto for ~2:1), `1` cubemap faces, … |

When `skybox_hdr` is set (or `r_skyboxHDR`), the engine loads the panorama via
**tinyexr** (OpenEXR), converts it to a cubemap for IBL, and builds **scene-linear
RGBA32F** outerbox faces (values may exceed 1.0) so the sky writes into SceneHDR
with `r_skyOwner 2`. Visible radiance uses `r_skyExposureEV` / `r_skyLuminanceScale`
(not Reinhard→RGBA8). See [HDR_SKY_RENDERING.md](HDR_SKY_RENDERING.md).

Cvars: `r_skyboxHDR`, `r_skyboxHDR_exposure`, `r_skyboxHDR_rotation`,
`r_skyboxHDR_intensity`, `r_skyboxHDR_projection`, `r_skyOwner`,
`r_skyExposureEV`, `r_skyLuminanceScale`.

Per-map sidecar (no BSP edit required): `maps/<map>.skybox_hdr` — first token is
the panorama path; optional `exposure` / `rotation` / `intensity` / `projection`
tokens follow. Example: `maps/surf_aztec.skybox_hdr` → `env/aarfontein_dirt_road_4k.exr`.
Maps without worldspawn/`skybox_hdr` sidecar clear `r_skyboxHDR` so panoramas do
not leak between levels.

See also [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) for the full key list.

## Collision

Point queries use the BSP30 render-node tree. Player and box sweeps use the
map's precomputed clipnode hull and add only the residual idTech3 box extent.
This preserves surf ramps and edges without importing an external physics or
SDK implementation.

## Face triangulation and plane winding

BSP30 faces are edge-walked rings that may be **non-convex**. The renderer
triangulates each face with **ear clipping** (`R_Bsp30_TriangulateFace` in
`renderers/common/tr_bsp30_triangulate.c`). When ear-clip fails the centroid-
inside test, every vertex is tried as a **fan hub** and the first triangulation
whose triangle centroids lie inside the polygon is kept. A vertex-0 fan is only
the last resort (and is what produced the original AZ letter wedges).

After loading vertices, the face plane is **aligned to the Newell normal** of
the vertex ring. A mismatched `face.side` / surfedge winding left
`plane.normal` anti-parallel to the geometry on `surf_aztec` letter brushes;
`R_CullSurface` then dropped front-facing faces while neighbors remained,
producing shredded “AZ” letters and hard black wedges. This is a static
geometry / cull bug — not temporal AA.

Regression: `ctest -R unit_bsp30_triangulate`,
`tests/scripts/test_geometry_corruption_regression.sh`.
See also [EXPLODING_GEOMETRY.md](EXPLODING_GEOMETRY.md).

## Surface identity (surf_aztec “AZ”)

| Property | Value |
|---|---|
| Type | BSP30 brush faces (not MD3, decal, sprite, or glyph mesh) |
| Entity | `func_illusionary` `"model" "*17"` |
| Texture | `black` (WAD `de_aztec.wad`; checkerboard fallback if WAD absent) |
| Shader | auto `*bsp30/black` |
| Approx. bbox | x −2744…−2382, y 256…263, z 1958…2320 |
