# Full-fidelity G-buffer

`r_gbufferQuality 2` is the Deferred production/IQ reference. Its logical
surface data is base color/opacity, world normal/perceptual roughness,
metallic/AO/clearcoat, lightmap irradiance, explicit lighting ownership,
depth, and motion where required. The physical Vulkan layout currently uses
the albedo, normal, material, and `GBufferSurfaceData` MRTs; the decoded
representation is `SurfaceMaterial`.

Ownership is `SurfaceData.a`, not a biased normal or roughness value.
Lightmap irradiance is `SurfaceData.rgb`. Compact octahedral normal packing is
quality 0 only and cannot provide IQ-reference evidence.

Opaque emissive, sheen, transmission, and other data without an explicit
full-fidelity channel route completely to Forward+/specialized rendering.
They must not partially enter Deferred.

Commands: `gbuffer_status`, `gbuffer_quality_status`,
`gbuffer_attachment_status`, and `gbuffer_decode_validate`.
