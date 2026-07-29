# Vulkan Render Graph

The Vulkan renderer now has a real render graph core in `renderers/vulkan/vk_render_graph.c`.
It is deliberately wired behind the existing Spine pass registry so the current renderer can
keep recording commands while the graph learns the frame and validates ordering.

## What It Does

- Declares each production pass with explicit read and write resource edges.
- Observes the passes that actually run in a frame.
- Imports known external frame resources such as swapchain color, depth, HDR color, history,
  Forward+/cluster light buffers, sun shadow, and probe grid.
- Derives producer-to-consumer dependencies from resource reads/writes.
- Adds write-after-write and read-before-write dependencies for shared resources.
- Topologically compiles the observed frame into a deterministic pass order.
- Detects unresolved resource reads and dependency cycles.
- Exposes `render_graph_status` for the compiled frame order, dependency edges, and violations.
- Provides `vk_render_graph_set_pass_executor` and `vk_render_graph_execute` for migrating passes
  from observation-only recording to graph-owned execution.

## Current Integration

`vk_pass_registry.c` remains the source of pass/resource declarations. During
`vk_spine_registry_init`, every declared Spine pass is registered with the render graph.

At runtime:

- `vk_spine_frame_begin` calls `vk_render_graph_begin_frame`.
- `vk_spine_pass_begin` calls `vk_render_graph_observe_pass`.
- `vk_spine_frame_end` calls `vk_render_graph_end_frame`, which compiles the observed graph.

This means current renderer behavior is preserved, but every frame now produces a compiled graph
that can be inspected and validated.

## Migration Path

The intended migration is incremental:

1. Keep pass declarations centralized in the Spine table.
2. Move isolated compute/post passes to `vk_render_graph_set_pass_executor`.
3. Let `vk_render_graph_execute` own those passes once their command recording is callback-safe.
4. Promote attachment allocation and transient image lifetime into graph resource descriptions.
5. Replace manual late-post ordering with graph execution for opaque, transparent, temporal, and
   present phases.

The important contract is that new Vulkan passes must declare resources before they are observed.
If a pass reads an unimported resource with no producer, the graph records an unresolved-read
violation instead of silently accepting the frame.

## Runtime

Console:

```text
render_graph_status
```

Useful companion commands:

```text
spine_status
pass_registry_status
```

Static regression:

```text
./tests/scripts/test_render_graph.sh
```
