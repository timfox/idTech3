# Scene 07 — glTF GPU tangent mode 2 (topology)

**Tier**: B (GPU + curated glTF asset)  
**Goal**: Validate **`r_gltfGpuTangentFix 2`** (topology-weighted tangent) vs **`1`** (Gram–Schmidt only) on a **skinned** glTF with a **normal map**, where tangent quality is visually sensitive.

## Preconditions

- Self-hosted runner with `GAME_BASE` and Vulkan client (see `docs/renderer_validation/SELF_HOSTED_TIER_B.md`).
- Asset: skinned glTF with **TEXCOORD_0** and **normalTexture** (external images OK), moderate triangle count (so **8** incident triangles per vertex cap in `tr_gltf_topo.h` is unlikely to clip badly).

## Procedure

1. Baseline: `r_gltfGpu 1`, `r_gltfGpuTangentFix 1`, `vid_restart`, capture reference stills / video.
2. Candidate: `r_gltfGpuTangentFix 2`, `vid_restart`, same pose and lighting.
3. Compare tangent-dependent shading: normal-map detail, specular streaks along UV seams, and silhouette stability under animation.

## Pass / fail

- **Pass**: No obvious seam inversion or “swimming” tangents worse than mode `1`; any regression is documented with captures and model path.
- **Fail**: Clear shading regression (inverted binormal behavior, worse UV seam lighting) attributable to mode `2` — file an engine issue with asset + `r_*` settings.

## Notes

- Mode `2` is **MikkT-inspired**, not bit-exact MikkTSpace; CPU **`r_gltfCpuQtangent`** on OpenGL remains the reference for full MikkT on tessellated geometry.
