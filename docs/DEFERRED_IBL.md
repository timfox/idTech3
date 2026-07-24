# Deferred IBL parity

Deferred uses the same irradiance cube, prefiltered environment, BRDF LUT,
roughness mip convention, and scene-linear HDR convention as Forward+.
Lightmap-owned static diffuse suppresses duplicate diffuse IBL where the
material policy requires it. The visible sky remains independent of reflection
mip selection.

Local-probe transition parity is not yet GPU-certified. `ibl_parity_validate`
remains an evidence gate, not a source-code checklist.
