# GPU Indirect Draws

**Status:** Milestone 1 — validated host-packed `VkDrawIndexedIndirectCommand` + published draw count.  
**Related:** [GPU_SCENE.md](GPU_SCENE.md) · `vk_gpu_scene.c`

## Ownership

| List | Path |
|------|------|
| deferred opaque | `VK_GPU_PATH_DEFERRED` |
| Forward+ opaque | fallback |
| alpha-tested / shadow / transparent / weapon | separate counters |

## Data flow

Clear buffer → validate mesh/generation → pack cmds → publish exact count → (optional) merge.

## Buffer formats

`vkGpuSceneDrawCmd_t` = Vulkan indexed indirect layout. Draw-count buffer: `uint32` published count.

## Lifecycle

Each frame zeros unused slots (`instanceCount=0`). Never execute full capacity.

## Fallback behavior

Capacity overflow → direct path for excess (`fallbackObj` counter). Fault inject rejects bad cmds.

## Debug commands

`gpu_draw_status`, `r_gpuDrawForce*`, `r_gpuDrawCompare` 1–9.

## Performance cost

Pack <0.1 ms. MDI savings only when consumers issue `vkCmdDrawIndexedIndirect`.

## Known limitations

`DrawIndirectCount` not required yet — CPU-published count is authoritative.

## Next milestone hooks

Wire production world draw consumer for pilots; optional IndirectCount.
