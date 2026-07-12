# Virtual texture scaffold

Chocolate **MegaTexture-class lite**: physical page atlas + CPU page table. Not sparse `VkImage` residency and not a full BSP UV rewrite.

## Enable

```
set r_vt 1
vid_restart
exec demo_vt.cfg   // loads vt/page0..2.png + dirt into the atlas
set r_vtDebug 1    // PiP atlas overlay (debug consumer)
vt_status
vt_load textures/demo/dirt.png   // decodes PNG/TGA/JPG; missing files get a procedural page
vt_flush
```

Demo pages ship under `examples/demo_game/bootstrap_media/vt/` (`page0.png` … `page2.png`) and pack into `idtech3_demo.pk3` when built with `demo`.

## Cvars / commands

| | |
|--|--|
| `r_vt` | Latched master (default 0) |
| `r_vtDebug` | Draw atlas PiP overlay (default 0) |
| `vt_status` | Atlas size, slot use, hit/miss, real vs procedural loads |
| `vt_load <path>` | Decode PNG/TGA/JPG into one atlas page |
| `vt_flush` | Clear page table |

Atlas: 8×8 pages of 128×128 RGBA (`*vt_atlas`). Uploads use `vk_upload_image_data`.

## Relation to NDGI

Neural Dynamic GI’s `r_ndgi_vt` is **lightmap dirty-page decode**, not this albedo page cache. See [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md).

## Limits (v1)

- No GPU feedback / indirection UVs in world shaders yet
- Demo consumer: `vt_load` + optional `r_vtDebug` PiP overlay
