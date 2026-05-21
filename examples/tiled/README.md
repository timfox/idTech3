# Tiled Map Editor (optional)

Reference workflow for **2D tile maps** alongside Quake III–style **`.map` / BSP`** levels. The engine does not load `.tmx` files yet; use Tiled for planning, overlays, or exports your game mod converts.

## Setup

From the engine repo root:

```bash
./scripts/init_optional_submodules.sh --tiled
```

Build and run Tiled from `tools/tiled/` per [docs/TILED.md](../../docs/TILED.md) (Qt, GPL-2.0).

## Reference map in this folder

| File | Purpose |
|------|---------|
| [`minimal_demo.tmx`](minimal_demo.tmx) | Tiny orthogonal map (4×4) you can open in Tiled after init |

## Typical pipeline

1. Author tiles and layers in Tiled (tilesets, object layers, custom properties).
2. Export for your mod:
   - **CSV / JSON** — scripts or tools convert to collision, spawn points, or UI layouts.
   - **Lua tables** — generate from Tiled object layers for `scripts/lua/`.
   - **Images** — render tile layers to PNG for 2D UI or minimaps.
3. Keep **BSP / `.map`** as the 3D world source of truth unless you add a dedicated loader.

## Engine maps (unchanged)

- 3D levels: Quake `.map` → BSP → `.bsp` in your `base/` pk3.
- Bootstrap data: [docs/MINIMAL_GAME_SHELL.md](../../docs/MINIMAL_GAME_SHELL.md).

## License

Tiled itself is GPL-2.0 in `tools/tiled/`. Files in **`examples/tiled/`** are engine-repo documentation/samples (same license as this repository).
