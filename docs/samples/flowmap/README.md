# Water Flowmap Sample

Sample assets for the water flowmap feature. Flowmap textures drive per-pixel UV offset for water surfaces (rivers, pools, wakes).

## Setup

1. **Copy the flowmap texture** into your game's `base/` directory:
   ```
   base/textures/flowmap/flowmap.png
   ```

2. **Copy the shader** (must be `.shaderx` for extended features):
   ```
   base/scripts/water_flowmap.shaderx
   ```

3. **Water texture**: The sample shader uses `textures/water/water`. Either:
   - Add your own water texture at `base/textures/water/water.png` (or .tga), or
   - Edit the shader and change `clampmap textures/water/water` to point to an existing texture from your game (e.g. from a pk3).

## Usage in a map

In your map editor, assign the shader `textures/water/flowmap_water` to water brush surfaces. The flowmap will animate the water texture based on the flow vectors in `flowmap.png`.

## Flowmap texture format

- **R channel**: Flow direction X (0.5 = no flow, &lt;0.5 = left, &gt;0.5 = right)
- **G channel**: Flow direction Y (0.5 = no flow, &lt;0.5 = down, &gt;0.5 = up)
- **B channel**: Unused (can be 0.5 for neutral)

Create custom flowmaps in an image editor by painting R/G values. The included `flowmap.png` is a simple left-to-right gradient for testing.

## Shader parameters

- `flowmapTex <path>` — Path to the flowmap texture
- `flowSpeed <value>` — Flow intensity (e.g. 0.05 for subtle, 0.15 for strong)
