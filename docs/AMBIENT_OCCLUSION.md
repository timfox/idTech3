# Ambient Occlusion Ownership

The renderer has two different AO families and they must not be presented as
the same algorithm.

## HBAO

The legacy post effect supports horizon-based screen-space AO through:

```text
r_fbo 1
r_ssao 1
r_ssaoMethod 1
r_hbaoDirections 8
r_hbaoSteps 8
```

`renderers/vulkan/shaders/glsl/hbao.frag` reconstructs view position from the
camera depth buffer, marches directional screen-space slices, retains the
maximum elevation angle, and integrates the unoccluded horizon. Increasing
`r_hbaoDirections` and `r_hbaoSteps` improves angular and radial coverage at a
linear texture-sample cost. The bilateral blur and SSAO combine remain the
owner of the post-process output.

HBAO is mutually exclusive with Ambient Visibility modes 2–5. When
`r_ambientVisibilityMode >= 2`, Ambient Visibility owns AO and the legacy post
SSAO pass is suppressed. This prevents double-darkening and keeps deferred,
Forward+, and transparent paths consistent.

## Ambient Occlusion Volumes (AOV)

AOV is a separate geometry-owned feature, not another name for HBAO/GTAO.
Analytic volume integration needs explicit occluder data that survives camera
motion and LOD changes. The planned contract is:

```text
volume primitive: sphere | capsule | box | custom convex
transform:        world transform and inverse bounds
falloff:          inner radius, outer radius, strength
material class:   opaque-only / transmission-aware
residency:        persistent GPU volume buffer with frame budget
```

The future AOV pass will consume that persistent volume buffer and write the
same ambient-visibility attachment used by deferred lighting. It will not read
the screen depth buffer as its source of occluders. Until that buffer and its
LOD/residency ownership exist, `r_ambientVisibilityMode` deliberately does not
claim an AOV mode; modes 2–5 are GTAO, RTAO, hybrid, and reference AO.

Validation should use the same Sponza camera with: HBAO, GTAO, and the eventual
AOV volume bake. Compare flat stone, arch contacts, thin railings, and LOD
transitions separately; screen-space AO is expected to lose information at
off-screen and disoccluded boundaries while AOV is expected to remain stable.
