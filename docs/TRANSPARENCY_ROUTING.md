# Transparency Routing (Phase 2.6)

## Pass order

```text
fog_scene
→ WBOIT resolve (+ additive composite)
→ sorted refractive draws
→ specialized classic blends
→ weapon opaque / optic / emissive
→ bloom (SCENE_LINEAR_HDR contributors only)
```

## Classes

| Class | Route |
|-------|--------|
| Ordinary glass/smoke | WBOIT (`r_oit 1`) |
| Additive ONE/ONE | Additive bucket (no revealage) |
| Refractive / distortion / screenMap | Sorted post-WBOIT |
| Multiply / filter / DST_COLOR | Specialized blend queue |
| Portal / mirror / screenMap | Explicit scene-source routes |
| Weapon optic / reticle | Weapon transparency classes |
| UI / decal | Excluded from OIT |

## Commands

`transparency_route_status`, `transparency_lab_status`, `special_transparency_status`, `refraction_status`, `portal_transparency_status`, `weapon_transparency_status`, `transparent_shadow_status`, `transparency_resource_status`

## Cvars

`r_transparencyReference`, `r_transparencyFreeze`, `r_transparencyCompare`, `r_transparencyCompareStage`, `r_refraction`, `r_refractionQuality`, `r_refractiveExcludeOit`
