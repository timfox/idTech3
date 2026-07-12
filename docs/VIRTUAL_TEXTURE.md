# Virtual texture scaffold

Chocolate **MegaTexture-class lite**: physical page atlas + CPU page table. Not sparse `VkImage` residency and not a full BSP UV rewrite.

## Enable

```
set r_vt 1
vid_restart
exec demo_vt.cfg
set r_vtDebug 1     // PiP atlas overlay
set r_vtSample 1    // brush-top bsp_stream faces sample the atlas
vt_status
```

Demo pages: `examples/demo_game/bootstrap_media/vt/page0..2.png`.

## Cvars / commands

| | |
|--|--|
| `r_vt` | Latched master (default 0) |
| `r_vtDebug` | Atlas PiP overlay |
| `r_vtSample` | Use atlas shader on `r_bspStream` brush-top fallback faces |
| `vt_status` / `vt_load` / `vt_flush` | Status, decode PNG/TGA/JPG into a page, clear table |

## Limits (v1)

- No GPU feedback / world UV rewrite yet
- Sample consumer is brush-top overlay + PiP (not full MegaTexture UV space)
