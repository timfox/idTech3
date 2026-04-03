# Scene 04 — Volumetric fog

Package as **`maps/rtest_volumetric.bsp`** (see pack README).

## Goal

Exercise **froxel volumetrics**, sun/moon shaft stability, and interaction with shadows.

## Layout

- **Corridor or box** with a strong **directional light** and visible beam (dusty air).
- Optional: **local light** inside fog volume.
- Optional: integrate [fluid/fog](../../../RENDERERS.md) emitters if you use them.

## Pass criteria

- Fog **density** responds to cvars (`r_volumetricFog*`, project names) without NaN colors or full black.
- Temporal stability: minimal **swimming** when standing still; acceptable when moving.
- Shadowed regions **darken** fog believably vs lit fog.

## Cvars / notes

- Document baseline: `r_volumetricFog`, quality preset, grid dims.
- Compare one **screenshot** after fog shader or froxel changes.
