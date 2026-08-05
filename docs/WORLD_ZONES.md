# Engine spatial zones

Spatial zones are an engine-original residency and visibility hint layer. They
are inspired by the useful idea of dividing a map into bounded regions, but do
not consume or reproduce any legacy map compiler, SDK, BSP extension, or
third-party source implementation.

## Contract

Each zone contains:

- authored AABB bounds;
- load and unload radii with hysteresis;
- a priority score;
- a residency mask for district, texture, and shadow resources;
- optional adjacency indices for future portal/stream prediction;
- a resident/pending state.

`WorldZone_UpdateView` selects the highest-scoring zones under
`r_worldZoneBudget`. Resident zones remain alive through the unload radius,
which prevents thrashing at boundaries. Load/unload callbacks let district,
texture, collision, and GPU page systems own their resources explicitly.

## Current integration

The zone manager initializes with the open-world subsystem and updates before
sector residency. USDA district assemblies populate one zone per district.
Optional prim custom data keys are `zoneLoadRadius`, `zoneUnloadRadius`,
`zonePriority`, and integer `residencyMask` (the default is all three owners).
Districts remain the payload owner, while zone transitions trigger proxy
district residency. `WorldZone_IsLayerResidentAtPoint` exposes the shared
texture/shadow residency decision to renderer consumers.
