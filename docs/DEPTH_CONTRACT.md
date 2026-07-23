# Depth Contract (Phase 2.3.1 Freeze + 2.3.2 View-Depth)

**Status:** Frozen production depth policy for Vulkan scene rendering.  
**Code:** `vk_depth_contract.h` / `vk_depth_contract.c` · GLSL `depth_view.glsl`  
**Commands:** `depth_contract_status` (alias `depth_status`), `depth_contract_validate`  
**Version:** `DEPTH_CONTRACT_VERSION` **1** + `contractHash`

Parent color order: [COLOR_PIPELINE.md](COLOR_PIPELINE.md). Fog-through-layers: [WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md). Hi-Z: [HIZ_OCCLUSION.md](HIZ_OCCLUSION.md).

**Do not** change reversed-Z / clear / compare / clip conventions without bumping the version and re-certifying fog + OIT weighting.

---

## Frozen `depthContract_t`

| Field | Production value |
|-------|------------------|
| `reversedZ` | **true** |
| `zeroToOneClipDepth` | **true** (Vulkan) |
| `infiniteFarPlane` | **false** (finite `viewParms.zFar`) |
| `projectionMode` | `DEPTH_PROJECTION_PERSPECTIVE` |
| `farPlaneMode` | `DEPTH_FAR_FINITE` |
| `positiveViewDepthMode` | `VIEW_DEPTH_NEG_VIEW_Z` (**certified**; WBOIT fog + weight) |
| `nearPlane` | default **8** (`r_znear`) |
| `farPlane` | **dynamic** (`viewParms.zFar`) |
| `clearDepth` | **0** |
| `depthCompareOp` | `GREATER_OR_EQUAL` |
| current depth ownership | scene / opaque + depth prepass |
| previous depth ownership | temporal history (`temporal_prev_depth`) |

---

## Positive view-depth

```text
certified:  viewDepth = -viewSpace.z     # meters along camera forward (Q3 axis[0])
            = dot(worldPos - viewOrg, normalize(axis[0]))
fallback:   Depth_LinearizeReversedZ(deviceZ, zNear, zFar)
forbidden:  raw device depth as fog/weight metric
legacy:     |worldPos - viewOrg|         # diagnostic only (r_oitFogDebug 8)
```

Device depth remains the hardware buffer (reversed-Z). Fog, soft particles, and OIT **weighting** reconstruct positive view-depth via `depth_view.glsl` / `vk_depth_*` helpers — not `gl_FragCoord.z` alone.

Forward+ SSBO uploads `fp_view_forward` (`viewParms.or.axis[0]`, `w=1`) at param floats 32–35.

---

## Equations (device ↔ view)

Vulkan reversed-Z projection (`R_SetupProjectionZ`): near maps toward **1**, far toward **0**, clear **0**.

Linearize (matches TAA / postfx):

```text
viewDepth = (zNear * zFar) / (zNear + deviceZ * (zFar - zNear))
```

OIT accum depth test: `GREATER_OR_EQUAL`, no depth write ([WBOIT_CONTRACT.md](WBOIT_CONTRACT.md)). McGuire weight curve coefficients are unchanged; only the depth **input** is certified view-depth mapped to traditional [0,1].

---

## Print / validate

```text
depth_contract_status
depth_contract_validate
oit_fog_status   # depthApprox=certified positive view-depth
```

Static gates: `tests/scripts/test_depth_contract.sh`, `tests/scripts/test_oit_view_depth.sh`.  
Unit: `unit_depth_view` (`tests/unit/test_depth_view.c`).
