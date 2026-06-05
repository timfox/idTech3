# GPU golden — Tier B capture

Tier A (`gpu_golden_compare` ctest) validates manifest + placeholders without a GPU.

## Tier B (manual / CI with GAME_BASE)

Requirements:

- Display + Vulkan RTX/GL capable GPU
- Game data in `GAME_BASE` (compatible retail install or minimal bootstrap + demo pk3)
- Built client: `release/idtech3`

### Capture

```bash
export GAME_BASE=/path/to/base
./scripts/gpu_golden_capture.sh --capture
```

Uses `+exec gpu_golden_capture.cfg` then compares PNGs under `tests/data/golden/`.

### SP slice preset (hero TAA)

With `fs_game idtech3_demo` or your conversion mod:

```
set r_taa 1
set r_taaMotionVectors 1
set r_temporalCpuSkinPrev 1
set r_temporalCustomShaderMotion 1
```

Capture after loading a map with glTF hero + `misc_decal` props.

### Store results

Commit PNGs only when intentional visual change; attach `com_speeds` GPU ms in PR notes.

See [tests/data/golden/placeholder/README.txt](../tests/data/golden/placeholder/README.txt).
