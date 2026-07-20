# Scene Visual Quality — oa_rpg3dm2 (OpenArena)

**Map:** `oa_rpg3dm2`  
**Screenshot cause:** Selective hybrid launch was wiped by `classic_openarena_native.cfg` (`r_fbo 0`, `r_pbr 0`, `r_classicLighting 1`).

## Reproduce (modern path)

```bash
cd release
./idtech3 +set fs_game openarena \
  +set cl_preferModernGraphics 1 \
  +exec vulkan_overlay_oa_rpg3dm2_visual.cfg \
  +map oa_rpg3dm2
```

Recovery to classic OA look:

```text
exec classic_openarena_native.cfg
vid_restart
```

## Lighting ownership (enforced)

| Term | Owner | Notes |
|------|-------|-------|
| Lightmap | Static diffuse (primary) | Deluxe splits ambient residual |
| Clustered / Forward+ lights | Dynamic direct only | Not multiplied by sun shadow |
| SH / IBL diffuse | Missing ambient / dynamic | Material AO × IBL diffuse only |
| IBL specular | Env probe | Not × scalar AO; SHR may suppress gen_frag IBL |
| GTAO / AV | Approximate contact grounding | Soft floor + luminance-weighted; never emissive/transmission |
| Emissive | Additive pre-tonemap | Not × AV |
| SSR | Post opaque | Demoted when SHR RT owns |

## Debug

| Cvar | Modes |
|------|-------|
| `r_pbr_debug` | 1 direct, 2 IBL spec, 3 irradiance, 9 albedo, 10 normal, 11 roughness, 12 metal, 13 ambient, 14 AO, 15 emissive, 16 pre-tonemap sum |
| `r_rtaoDebug` | AV / GTAO views |
| `r_ambientVisibilityFloor` | Default **0.28**; modern OA uses **0.32** |

## Root causes (screenshot)

1. **OA auto-profile** forced classic (no FBO/PBR/SSR/SH).
2. **GTAO × full HDR** crushed lightmaps (fixed: floor + indirect weight).
3. **Default roughness 0.99** on non-physical PBR (fixed: 0.72 dielectric).
4. **Tonemap whitePoint 1.5** clipped emissives under HDR (modern OA: 4.0).
5. **Water** still classic blend / OIT milky — depth-aware liquid path not yet certified.

## Remaining (ranked)

1. Depth-aware water (reflection/transmission/absorption/soft edge)
2. Legacy material class heuristics beyond default roughness
3. Contact-shadow near-field pass
4. Runtime IBL cubemap capture for indoor probes
5. GPU soak metrics on oa_rpg3dm2
