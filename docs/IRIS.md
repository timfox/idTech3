# Iris — Digital pathology WSI rendering (experimental)

**Iris Core** (*A Next Generation Digital Pathology Rendering Engine*, Landvater & Balis, [J Pathol Inform 16 (2025) 100414](https://doi.org/10.1016/j.jpi.2024.100414)) is a Vulkan-based whole-slide imaging (WSI) renderer using 256×256 tiles, dual LR/HR scope passes, rapid tile buffering (RTBS), and single-pass downsampling (SPD) with Laplacian reduction-enhancement.

This engine ships a **Vulkan compute scaffold** aligned with Iris Core architecture (tile atlas, atomic tile state, SPD on HR tiles → `tile_mip`, LR compose from mip with bilinear upsample, HR overlay from atlas). Full upstream Iris (OpenSlide / proprietary codec, multi-thread loaders, MoltenVK iOS builds) is not embedded; see [IrisExamples](https://github.com/ryanlandvater/IrisExamples).

## Paper metrics (model)

| Metric | Iris Codec | OpenSlide |
|--------|------------|-----------|
| LR TeFOV (new FOV, no overlap) | **10 ms** | 56 ms |
| HR TeFOV | **25 ms** | 124 ms |
| LR TPT | **0.16 ms** / tile | 1.32 ms |
| HR TPT | **0.10 ms** / tile | 0.60 ms |
| Buffer-rate (median) | **1.36 GiB/s** | — |
| Sustained FPS (iPad Pro) | **120** | — |

`iris_compare` reports speedup vs DziTileSource (357 ms), FlexTileSource (240 ms), Schüffler et al. (164 ms).

## Build

```bash
./scripts/compile_engine.sh vulkan
```

## Runtime

```
r_iris 1
vid_restart
iris_pan 4
iris_save atlas.iris
iris_load atlas.iris
iris_status
r_iris_overlay 1
r_iris_bilinear 1
```

With `r_iris_overlay 1`, the scope is alpha-blended onto the main color buffer after the scene pass (requires FBO mode). Mode `0` is bottom-left PiP; mode `1` is fullscreen.

## `.iris` file format

| Version | Contents |
|---------|----------|
| v1 | Header + RGBA8 atlas |
| v2 | Header + RGBA8 atlas + `tile_state` (16×16 × uint32) |

Magic `IRIS`, tile grid **16×16 @ 256 px**. v1 files load with zeroed tile state.

## Cvars
|------|---------|------|
| `r_iris` | `0` | Master toggle (latched; `vid_restart`) |
| `r_iris_width` / `r_iris_height` | `1024` / `768` | Scope viewport |
| `r_iris_decoder` | `0` | `0` Iris Codec model, `1` OpenSlide model |
| `r_iris_sharpen` | `0.35` | SPD Laplacian sharpen scale |
| `r_iris_bilinear` | `1` | Bilinear `tile_mip` upsample in LR compose |
| `r_iris_overlay` | `0` | Alpha-blend scope onto main framebuffer |
| `r_iris_overlay_alpha` | `0.92` | Overlay blend strength |
| `r_iris_overlay_scale` | `0.4` | PiP width fraction (mode 0) |
| `r_iris_overlay_mode` | `0` | `0` PiP, `1` fullscreen |
| `r_iris_debug` | `0` | Developer logging |
| `cl_iris_model` | `1` | Paper benchmark commands |

## Console — model

```
iris_model_status
iris_api
iris_model
iris_teFOV [codec|openslide] [lr|hr]
iris_compare
```

## Console — renderer

```
iris_status
iris_pan [steps]
iris_load <path.iris>
iris_save <path.iris>
iris_spd_step
iris_reset
```

## Architecture (scaffold vs paper)

| Paper (Fig. 2–6) | Engine v4 |
|------------------|-----------|
| Render / buffer / loader threads | Simulated RTBS in `iris_pan` |
| VRAM wrapper recycling + atomics | `tile_state` image (FREE/LR/HR) |
| SPD floating-point mipmaps | `iris_spd.comp` → `tile_mip`; bilinear LR compose |
| Separate transfer/compute/render queues | Single compute queue |
| OpenSlide / `.iris` decode | Engine `.iris` v1/v2 atlas I/O; OpenSlide via RTBS model only |

## v4 limitations

- No OpenSlide / SVS file decode (`r_iris_decoder` adjusts RTBS HR promotion only)
- No stencil pass or triple queue-family sync

## References

- Paper: [doi:10.1016/j.jpi.2024.100414](https://doi.org/10.1016/j.jpi.2024.100414)
- Patent: [US20230334621A1](https://patents.google.com/patent/US20230334621A1)
- OpenSlide: [openslide.org](https://openslide.org/)
