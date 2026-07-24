# Deferred emissive ownership

Emissive is added exactly once by the selected lighting owner. Until a distinct
full-fidelity emissive attachment is production-ready, emissive opaque
materials route wholly through Forward+; additive materials route through the
additive/specialized transparency path. Raw albedo is never an emissive or
fullbright fallback.

`emissive_ownership_validate` must detect missing, doubled, LDR-clamped, and
wrong-exposure-state emissive contributions from scene-linear GPU captures.
