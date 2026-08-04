Material parity golden fixture

Tier A validates that this fixture and its capture contract are present. A
Tier B GPU job should replace this file with five captures named:

  material_parity_forward
  material_parity_deferred
  material_parity_wboit
  material_parity_ssr
  material_parity_rtx

All five captures use the same rtest_parity material scene. Keep exposure,
tonemapping, TAA, film grain, chromatic aberration, and compact GBuffer mode
fixed when generating replacements.
