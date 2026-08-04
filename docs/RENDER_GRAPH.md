# Vulkan Render Graph

The Vulkan renderer now has a real render graph core in `renderers/vulkan/vk_render_graph.c`.
It is wired behind the existing Spine pass registry. The current renderer still records
commands in its established functions, but entry/exit and known-image ownership are now
centralized at the graph boundary.

## What It Does

- Declares each production pass with explicit read and write resource edges.
- Observes the passes that actually run in a frame.
- Imports known external frame resources such as swapchain color, depth, HDR color, history,
  Forward+/cluster light buffers, sun shadow, and probe grid.
- Derives producer-to-consumer dependencies from resource reads/writes.
- Adds write-after-write and read-before-write dependencies for shared resources.
- Topologically compiles the observed frame into a deterministic pass order.
- Detects unresolved resource reads and dependency cycles.
- Exposes `render_graph_status` for the compiled frame order, dependency edges, per-frame
  violations, and cumulative violations.
- Exposes `render_graph_dot` for Graphviz-compatible dependency output with resource/access labels.
- Provides `vk_render_graph_set_pass_executor` and `vk_render_graph_execute` for migrating passes
  from observation-only recording to graph-owned execution.
- `r_spineAuthoritative 1` (default) makes `vk_spine_pass_begin/end` enter and leave graph-owned
  pass scopes. The single `record_image_layout_transition` helper reports every known renderer
  image transition to the owning Spine resource, including old-layout mismatches when validation
  is enabled.

## Current Integration

`vk_pass_registry.c` remains the source of pass/resource declarations. During
`vk_spine_registry_init`, every declared Spine pass is registered with the render graph.

At runtime:

- `vk_spine_frame_begin` calls `vk_render_graph_begin_frame`.
- `vk_spine_pass_begin/end` call `vk_render_graph_enter_pass/leave_pass`; undeclared or mismatched
  scopes are rejected and reported.
- `vk_spine_frame_end` calls `vk_render_graph_end_frame`, which compiles the observed graph.

This preserves command recording order while making the graph the ownership boundary for pass
scopes and known image layouts. Every frame still produces a compiled graph that can be inspected
and validated.

`renderer_status` prints a compact `graph` row, and `renderer_subsystems` prints a fuller
`rendergraph` row with observed pass count, compiled pass count, dependency count, per-frame
violations, and total violations.

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
render_graph_dot
```

Useful companion commands:

```text
spine_status
pass_registry_status
renderer_status
renderer_subsystems
```

Static regression:

```text
./tests/scripts/test_render_graph.sh
```
