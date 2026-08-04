# GPU vector brushes

The renderer has an opt-in vector-brush sidecar based on Ciao et al.,
“Ciallo: GPU-Accelerated Rendering of Vector Brush Strokes” (SIGGRAPH 2024).
It is intended for Studio/canvas/HUD overlays and is not a replacement for
primary raster visibility, deferred lighting, WBOIT, or the render graph.

Enable the contract with:

```text
exec vulkan_overlay_vector_brush.cfg
vector_brush_status
```

## Ownership

The sidecar owns editable polyline points, variable radii/opacity, brush
style, layer order, stamp resampling, and its eventual overlay composite.
It targets the overlay canvas. It does not write SceneHDR, the G-buffer, or
cluster light lists. If a future world-space stroke mode is added, it must
declare a separate pass/resource owner rather than silently joining the
opaque or transparent scene paths.

## Paper mapping

The first GPU proof is `vector_brush_resample.comp`: it expands each polyline
edge into deterministic, approximately equidistant stamp records. This is
the shared foundation for vanilla capsules and texture stamps. Airbrushes
should use Ciallo’s continuous alpha-density form (`1-exp(-integral density)`)
instead of sampling an excessive number of footprints. Prefix arc length,
footprint mip selection, programmable blending, and planar-map fill markers
remain follow-up stages.

The contract deliberately distinguishes editable strokes from raster brush
footprints. Stamp brushes are therefore not currently resolution independent;
procedural/vector stamps and mip-aware footprints are required for that claim.

Current status: contract plus compiled resampling proof; no live stroke import,
GPU buffer upload, final raster blend, or vector-fill implementation yet.

