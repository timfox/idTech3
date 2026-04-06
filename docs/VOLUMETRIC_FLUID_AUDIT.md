# Volumetric Fog & Fluid Simulation Quality Audit

**Date**: 2026-02-28  
**Scope**: `vk_volumetric_params.c`, `vk_fluidsim.c`, `vk.c` (volumetric/fluid paths), compute shaders

## Architecture Summary

### Volumetric Fog
- **Froxel grid**: Configurable 3D grid (default 160×90×64) for ray marching
- **Pipeline**: Compute pass → composite pass; temporal reprojection for stability
- **Parameters**: Density, height falloff, scatter, anisotropy, noise, wind, fog color
- **Integration**: Uses `r_volumetricFog*` and `r_fog*` cvars

### Fluid Simulation (vk_fluidsim)
- **Algorithm**: Stable Fluids (Jos Stam 1999) – Semi-Lagrangian advection + Jacobi pressure solve
- **Grid**: 2D (fluid_width × fluid_height), derived from froxel resolution
- **Emitters**: Up to 16; position, radius, density, temperature, velocity
- **Parameters**: Viscosity, diffusion, dissipation, buoyancy, vorticity, wind
- **Integration**: Fog fluid uses `r_fogFluid*` cvars; emitters from `FluidSim_*` API

### Fluid integration
- **r_fogFluid** – Fog-integrated fluid (`r_fogFluid*` cvars, `r_fogFluidPressureIterations` for Jacobi iterations)
- Emitters use `FluidSim_AddEmitter` / `FluidSim_GetEmitter` (API in `vk_fluidsim.c`).

## Audit Findings

### ✅ Strengths
- **Parameter validation**: Extensive clamping in `vk_update_volumetric_params` (fog_steps, depth_mode, quality, fluid params, etc.)
- **Bounds checks**: `vk_fluid_simulation_pass` validates pressure_iterations (1–64), delta_time, active dimensions
- **Telemetry**: Shaders increment counters for NaN/Inf, velocity/density clamp, temporal reject
- **Null safety**: `FluidSim_GetEmitter` returns NULL for out-of-range index
- **Descriptor guards**: `vk_update_volumetric_descriptors` checks all required views before updating

### 🔧 Improvements Applied

1. **Fog world AABB validation** (`vk.c`): If `r_volumetricFogWorldMax` ≤ `r_volumetricFogWorldMin` + 1 on any axis, clamp to valid extent to avoid zero/negative fluidWorldMap scale and division issues.

2. **Emitter radius validation** (`vk_fluidsim.c`): Clamp negative radius to 0 in `FluidSim_AddEmitter` (shader already uses `max(radius, 0.001)`).

3. **Jacobi iterations range** (`tr_init.c`): `Cvar_CheckRange(r_fogFluidPressureIterations, "1", "64", CV_INTEGER)` to prevent invalid iteration counts.

### 📋 Design Notes
- **Emitter position**: Shader uses `position.xy` for 2D world mapping; `fluidWorldMap` maps UV [0,1] to world (fog_min + uv × size).
- **fluidEmitterData layout**: `[density, temperature, vel.x, vel.y]` per emitter; matches shader usage.

### ⚠️ Recommendations
- RENDERERS.md states "64³ grid" for fluid; actual implementation is 2D (fluid_width × fluid_height). Consider doc update.
