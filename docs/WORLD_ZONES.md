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
- optional adjacency indices for future portal/stream prediction;
- a resident/pending state.

`WorldZone_UpdateView` selects the highest-scoring zones under
`r_worldZoneBudget`. Resident zones remain alive through the unload radius,
which prevents thrashing at boundaries. Load/unload callbacks let district,
texture, collision, and GPU page systems own their resources explicitly.

## Current integration

The zone manager initializes with the open-world subsystem and updates before
sector residency. It is currently an API-level layer; USDA manifest parsing
will populate zones in the next step. Districts remain the payload owner.
