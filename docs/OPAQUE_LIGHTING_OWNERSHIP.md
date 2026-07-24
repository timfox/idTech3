# Opaque lighting ownership

Every opaque surface is assigned exactly one logical owner:

```c
OPAQUE_OWNER_INVALID
OPAQUE_OWNER_DEFERRED
OPAQUE_OWNER_FORWARD_PLUS
OPAQUE_OWNER_LIGHTMAP_ONLY
OPAQUE_OWNER_EXPLICIT_FULLBRIGHT
OPAQUE_OWNER_SPECIALIZED
```

Deferred ownership is allowed only when the complete material is representable
by the active G-buffer contract. Unsupported multi-stage, destination-blend,
screen-map, portal, mirror, distortion, deformation, transmission, cinematic,
and unsupported animated materials route completely through Forward+ or a
specialized pass.

The full-fidelity path stores an explicit Deferred-owned marker in
`GBufferSurfaceData.a`; it does not infer ownership from roughness, lightmap,
normal, material ID, or a bias-encoded value. Non-owned pixels preserve the
valid Forward+ result during owner-based composition.

Debug colors are green (Deferred), blue (Forward+), yellow (lightmap-only),
white (explicit fullbright), purple (specialized), and red (invalid).
Production and strict validation require zero invalid and zero double-owned
pixels.

Raw albedo is never a recovery path for an invalid ordinary opaque owner.
