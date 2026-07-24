# World Presentation — Behavior Specification

Observable behaviors targeted by Milestone S1. Implementations use generic names
and the existing Vulkan / SceneHDR architecture.

## HDR eye adaptation

- Dark interiors: exposure rises gradually.
- Bright exteriors: exposure lowers more quickly.
- Direct sun: tiny core does not dominate the histogram.
- Looking away from sky: dark geometry becomes readable over time.
- Camera cut: documented reset or fast convergence.

## Exposure controller volumes

- Map-authored EV ranges blend smoothly across trigger volumes.
- One exposure convention for sky, world, weapon, and transparency.

## Sky environment (3D horizon)

- Reduced-scale secondary environment extends the horizon.
- Independent origin and scale; shares camera orientation.
- No collision/gameplay ownership; correct depth and exposure.

## Environment reflection probes

- Spherical/box influence, priority, two-probe blend, sky fallback.
- Parallax-corrected box projection; prefiltered specular; irradiance.
- Capture excludes UI/weapon; deterministic exposure convention.

## Reflective materials

- Mask, tint, Fresnel, roughness-aware probe sampling.
- Physical and artistic-compatibility modes remain scene-linear.

## Water

- Dedicated transparent route (not ordinary WBOIT).
- Fresnel split, dual normals, absorption, foam, refraction quality tiers.

## Projected lights

- Perspective cookie, depth shadow, clustered integration, flashlight policy.

## Decals / overlays

- Geometry and deferred routes; depth-aware; no sky/weapon bleed.

## Detail layers

- Bounded blend modes; distance fade; no shimmer from bad LOD bias.

## Lightstyles

- GPU table indexed by surface metadata; no per-frame CPU lightmap rewrite.

## Local fog volumes

- Box/sphere/height shapes; priority blend; shared fog ownership with WBOIT.

## Color correction volumes

- LUT/parameter grades after exposure+tonemap; HUD excluded.

## Material parameter drivers

- Bounded deterministic ops (time, sine, scroll, lightstyle, …).

## Visibility portals

- Area connectivity open/closed; not a PVS substitute.

## Terrain patches

- Height displacement, LOD, collision, crack-safe edges; original format.

## Viewmodel lighting

- Near-plane policy; world exposure/probe; no world TAA history contamination.
