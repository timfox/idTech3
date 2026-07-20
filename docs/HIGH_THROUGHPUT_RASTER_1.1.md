# High-Throughput Raster Engine 1.1

**Candidate profile:** `config/modern_high_throughput_animation.cfg` — **NOT the boot default.**  
**Recovery:** `exec modern_vulkan.cfg; vid_restart`  
**Preserves:** `modern_vulkan.cfg`, `gfx_safe.cfg`, mode 2 Forward+ fallback, mode 3 deferred opaque ownership, Forward+ transparent/weapon, classic BSP/MD3, QVM, OpenGL fallback, dedicated-server builds, pass/resource registry, HT 1.0 indices.

Ray tracing / path tracing / frame generation remain optional (locked off under this candidate).

## Slice status

| Slice | Scope | Status |
|-------|--------|--------|
| **A — Skeletal throughput** | Canonical records, clip compression, pose decode (CPU), VS GPU skin, prev bone palette, anim LOD | **shipped (opt-in)** |
| B — Tangent + morph fidelity | Compact tangent frames, authored normals, morph parity | not started |
| C — Geometry caches | Compiler, prediction, streaming, shared instances | not started |
| D — Surface interaction | Decals on deforming geo, destruction, cloth caches | not started |
| E — Certification | Stress, soak, promotion | not started |

HT 1.0 Slice A (GPU throughput) remains the foundation — see `docs/HIGH_THROUGHPUT_RASTER_1.0.md`.

## Production ownership (Slice A)

| Concern | Owner |
|---------|--------|
| Pose / clip time | Animation system (`vk_ht_animation` records + game frame/oldframe) |
| Deformed vertices | GPU VS skinning (IQM/glTF SSBO path) when enabled |
| Previous deformation | Prev bone palette in IQM/glTF skin SSBO; motion validity tracked per instance |
| Material shading | Existing shader / PBR path |
| Shadows | Same deformation buffers as main view when GPU skin is used for the draw |
| Collision | Gameplay / physics (unchanged) |

Composition order (morph + skin): **source → morph deltas → skeletal skin → normalize normals** (matches existing IQM GPU VS path).

## Slice A — what shipped

### Canonical records (`vk_ht_animation`)

Stable IDs (slot indices, never raw file pointers) for:

- `AnimationSkeleton`, `AnimationClip`, `AnimationPose`, `AnimationInstance`, `DeformationOutput`
- Asset generation, topology signature, sample rate, compression format, streaming state, debug name

Commands: `animation_status`, `animation_memory`, `animation_profile`, `deformation_status`.

### Clip compression

- Constant-track-aware quantized translation / compact quaternion / compressed scale
- Per-bone importance tolerances (critical / weapon / facial / helper vs default)
- Measured max/avg angular error, max positional, root, end-effector error, size ratio
- **Full-precision fallback** when tolerance fails (`VK_HT_CLIP_COMPRESS_FAILED_FALLBACK`)
- Runs at IQM load when `r_htAnimation 1` (developer log of measured metrics only)

### GPU skinning (certified path)

- **IQM:** `r_iqmGpu 1` enables VS GPU skin **without** requiring morph targets (same SSBO layout as morph+skin)
- **glTF:** existing `r_gltfGpu` (unchanged)
- MDR/MD3 remain CPU / vertex-frame paths
- Prev skin/normal matrices written for motion vectors when GPU path is active

### Animation LOD

- Tiers: full → reduced rate → reduced skeleton (rate proxy) → impostor
- Distance + visibility inputs with **hysteresis** (`r_animLodHysteresis`)
- Pose update skips counted; does not change boot MD3 behavior when hub is off

## Enable

```
exec modern_high_throughput_animation.cfg
vid_restart
```

## Explicit non-goals this slice

- Compact tangent frames (`r_compactTangentFrames`) — Slice B
- Geometry-cache compiler / streaming / shared instances — Slice C
- Decals attached to deforming surfaces / destruction hooks — Slice D
- Dual-quaternion skin, compute pre-skin, async compute pose decode — experimental later
- Invented CPU/GPU millisecond timings or soak results — Slice E

## Promotion decision (Slice A)

| Capability | Class |
|------------|--------|
| Canonical anim records + diagnostics | quality opt-in |
| Quantized clip compression + measured fallback | quality opt-in |
| IQM VS GPU skin (`r_iqmGpu`) | quality opt-in |
| Prev-frame bone palette (GPU path) | quality opt-in (parity with morph path) |
| Animation LOD (update rate / hysteresis) | quality opt-in |
| glTF GPU skin | already shipping (`r_gltfGpu`) |
| Compact tangents | experimental (not started) |
| Geometry caches | experimental (not started) |
| Boot `modern_vulkan.cfg` | unchanged certified fallback |

## Highest-impact next failure to fix

Before Slice B: measure skinned vertex / normal angular error on a reference IQM vs CPU skin ground truth on device, and confirm shadow draws reuse the same GPU skin batch (no second CPU skin). Then start compact tangent validation — not geometry caches.
