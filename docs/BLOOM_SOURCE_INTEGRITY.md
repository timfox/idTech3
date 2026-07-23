# Bloom Source Integrity

**Code:** `vk_bloom_source_contract.c`  
**Commands:** `bloom_source_status` · `bloom_source_validate`

Bloom must sample complete post-weapon `SCENE_LINEAR_HDR` (`vk.color_image`). Contributors are inferred from SceneHDR ownership (opaque, GI, WBOIT, special, weapon, volumetric). Forbidden: OIT accum/reveal, UI, tonemap/display-encoded, stale pre-weapon HDR.

See [BLOOM_FIREFLY_CONTROL.md](BLOOM_FIREFLY_CONTROL.md), [COLOR_PIPELINE.md](COLOR_PIPELINE.md).
