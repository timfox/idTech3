# GPU Frame Ownership

**Status:** Milestone 1 Phase 1 — host frame IDs + persistent object SSBO upload.  
**Related:** [GPU_OBJECT_IDENTITY.md](GPU_OBJECT_IDENTITY.md) · [GPU_SCENE.md](GPU_SCENE.md)

## Ownership

| ID | Meaning |
|----|---------|
| sceneUpdateFrame | begin_frame |
| objectTransformFrame | last transform write |
| visibilityFrame | cull pass |
| indirectCommandFrame | command publish |
| objectUploadFrame | object SSBO memcpy |

## Data flow

begin_frame → cull/build → end_frame advances previous transforms (rendered frames only).

## Buffer formats

Host frame counters; not swapchain-image indexed.

## Lifecycle

Map/vid restart bumps scene generation and clears draws.

## Fallback behavior

Partial updates never consumed — cull skips mismatched generation.

## Debug commands

`gpu_frame_ownership_status`.

## Performance cost

Negligible.

## Known limitations

Host cull remains authoritative; object SSBO is host-visible (Phase 1). Multi-buffered device-local object buffers land with GPU cull.

## Next milestone hooks

Frames-in-flight device buffers after GPU cull lands.
