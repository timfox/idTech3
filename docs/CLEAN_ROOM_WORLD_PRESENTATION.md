# Clean-Room World Presentation Features

This document defines the independent protocol for implementing observable
world-presentation behaviors in this Vulkan renderer.

## Protocol

1. Specify observable behavior before coding.
2. Derive mathematics from public graphics literature or original analysis.
3. Implement under generic engine-facing names.
4. Keep optional format import separate from runtime ownership.
5. Record provenance for every feature.
6. Do not use proprietary source, leaked code, disassembly, or reverse-engineered
   undocumented constants.

## Header rule

New feature units include:

```c
/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 */
```

## Feature registry

Runtime ownership lives in `vk_world_presentation.c` with flags
`worldPresentationFeature_t`. Commands:

- `world_features_status`
- `world_features_validate`
- `world_feature_status <feature>`
- `world_feature_enable <feature>`
- `world_feature_disable <feature>`
- `world_perf_status`
- `world_memory_status`

## Feature index

See [WORLD_PRESENTATION_BEHAVIOR_SPEC.md](WORLD_PRESENTATION_BEHAVIOR_SPEC.md)
and [WORLD_PRESENTATION_PROVENANCE.md](WORLD_PRESENTATION_PROVENANCE.md).

## Hard constraints

- Preserve scene-linear HDR and single exposure application.
- Preserve reversed-Z and WBOIT contracts.
- Unknown materials must not silently become fullbright.
- Do not introduce temporal ghosting to imitate legacy looks.
- Do not resume unrelated GPU-driven visibility work in this milestone.
