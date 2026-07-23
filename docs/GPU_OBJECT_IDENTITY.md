# GPU Object Identity

**Status:** Milestone 1 — handle + objectGeneration + scene generation.  
**Related:** [GPU_SCENE.md](GPU_SCENE.md) · [GPU_FRAME_OWNERSHIP.md](GPU_FRAME_OWNERSHIP.md)

## Ownership

`gpuSceneObject_t` / `GpuSceneObject` is the sole identity for temporal / visibility / draws.

## Data flow

Register (spawn) → update transform → invalidate on model/material/teleport → unregister (despawn increments generation).

## Buffer formats

`objectId`, `objectGeneration`, `generation`, `sourceKind`/`sourceRef`, frame stamps.

## Lifecycle

Free-list reuse increments `objectGeneration`. Stale generation cannot match new entities.

## Fallback behavior

Capacity full → refuse register + count `fallbackObj` (direct draw).

## Debug commands

`gpu_scene_object_status <handle>`, `gpu_scene_inspect`.

## Performance cost

O(n) handle lookup (dense table).

## Known limitations

Not yet hashed; fine for ≤4k instances.

## Next milestone hooks

Stable hash map if instance counts grow.
