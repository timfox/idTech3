# GPU golden — Tier B capture

Tier A (`gpu_golden_compare` ctest) validates manifest + placeholders without a GPU.

## Tier B (manual / CI with GAME_BASE)

Requirements:

- Display + Vulkan capable GPU
- Game data in `GAME_BASE` (compatible retail install or minimal bootstrap + demo pk3)
- Built client: `release/idtech3`
- Optional stub maps: `docs/renderer_validation/devdata/rtest_base/` (see [OPTIONAL_GAME_ASSETS.txt](samples/renderer_regression/OPTIONAL_GAME_ASSETS.txt))

### Capture

```bash
export GAME_BASE=/path/to/base
./scripts/gpu_golden_capture.sh --capture
```

Uses `+exec gpu_golden_capture.cfg` (pins `r_filmGrain 0` / `r_chromaticAberration 0` / `r_hdr 2`) and collects `screenshots/renderer_golden.jpg` into `tests/data/golden/captures/`.

**Headless CI**: leave placeholders under `tests/data/golden/placeholder/`; operators with a display commit real PNGs when intentional.

### SP slice preset (hero TAA)

With `fs_game idtech3_demo` or your conversion mod:

```
set r_taa 1
set r_taaMotionVectors 1
set r_temporalCpuSkinPrev 1
set r_temporalCustomShaderMotion 1
set r_filmGrain 0
set r_chromaticAberration 0
```

Capture after loading a map with glTF hero + `misc_decal` props.

### Store results

Commit PNGs only when intentional visual change; attach `com_speeds` GPU ms in PR notes.

See [tests/data/golden/placeholder/README.txt](../tests/data/golden/placeholder/README.txt).
