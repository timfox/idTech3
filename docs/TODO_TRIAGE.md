# TODO / FIXME Triage

**Date**: 2026-03-03  
**Scope**: Project code in `src/` (excluding `external/` third-party libraries)

---

## Summary

| Category | Count | Status |
|----------|-------|--------|
| net_sdr.c TODOs | 7 | Resolved: full Steam SDR implementation (Steamworks SDK) |
| be_aas_reach.c FIXME | 1 | Resolved: reworded to Legacy (dead code) |
| False positives (XXX in names, patterns) | 4 | Documented; no action |
| External/third-party | 150+ | Out of scope |

---

## Project TODOs (Actionable)

### net_sdr.c — Steam Networking Sockets (SDR)

**Resolution**: Full implementation complete. Uses Steamworks SDK (ISteamNetworkingSockets) for P2P SDR. Enable with `USE_STEAM_NETWORKING=ON` and set `STEAMWORKS_SDK` to SDK path. Cvar `net_sdr 1` enables SDR transport. Connect via `connect steam:STEAMID`. See `docs/ROADMAP.md` and `docs/DEVELOPMENT_SETUP.md`.

---

### be_aas_reach.c — Jump pad velocity

| Line | Item | Priority | Notes |
|------|------|----------|-------|
| 3595 | 1.1 overshoot factor | P4 | Reworded FIXME → Legacy; in commented-out block. Dead code. |

**Resolution**: Comment updated to "Legacy: 1.1 overshoot factor for trigger_push (commented block)". No FIXME remaining.

---

## False Positives (Not TODOs)

| File | Pattern | Reason |
|------|---------|--------|
| vm_armv7l.c:36 | `XXX` in pragma | Compiler warning ID (signed/unsigned mismatch) |
| vm_aarch64.c:29 | `XXX` in pragma | Same |
| vm_x86.c:3819 | `OP_XXX` | Opcode name in comment |
| tr_image.c (Vulkan, OpenGL) | `lm_XXXX` | Lightmap texture naming pattern (e.g. lm_0001) |

---

## External Libraries (Out of Scope)

TODOs/FIXMEs in `src/external/` are from third-party code (duktape, zstd, cjson, flac, libpng, opus, etc.). These are not triaged; upstream fixes apply.

---

## Incomplete / Stub Items (Documented)

| Item | Location | Status |
|------|----------|--------|
| RB_ColorMask (Vulkan) | tr_backend.c | Partial: `vk_set_color_write_mask()` exists, but the VK_EXT_extended_dynamic_state3 path is currently disabled due to validation/driver issues; Vulkan falls back to full color writes. |
| r_renderMode 1/2 | tr_init.c | Deferred/Forward+ placeholders; need G-buffers, light culling. Documented in cvar description. |
| r_hdr 3 64-bit output | vk.c | Infrastructure in place (vk_hdr64_active, _hdr64 modules, pipeline selection). glslangValidator rejects dvec4/f64vec4 fragment shader outputs. Falls back to RGBA32F. When glslang adds support, compile HDR64 variants and return RGBA64F from get_hdr_format. |
| Vegetation wind draw | vk.c | Compute runs; wind-modified buffer not yet used for rendering. Future enhancement. |
| Vulkan RTX | vk.c | Extensions enabled when USE_VULKAN_RTX=ON and GPU supports them. Pipeline (BLAS/TLAS, shaders) not yet implemented. See docs/RENDERERS_FUTURE.md. |

## Recommendations

1. **net_sdr.c**: Full SDR implementation; requires `USE_STEAM_NETWORKING=ON` and Steamworks SDK.
2. **be_aas_reach.c**: No action; dead code.
3. **USE_VULKAN_RTX**: Build with `-DUSE_VULKAN_RTX=ON` to request ray tracing extensions on RT-capable GPUs.
4. Re-run triage after major refactors or when adding new features.
