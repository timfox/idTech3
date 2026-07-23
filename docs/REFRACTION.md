# Refraction (Phase 2.6)

Sorted refractive path after WBOIT. Materials with screenMap / refract / portal semantics **must not** enter ordinary WBOIT accumulation.

## Resources

```text
ResolvedWboitHDR → RefractiveSceneInput (copy/ping-pong) → sorted draws → RefractedSceneHDR
```

Format: `SCENE_LINEAR_HDR`. No same-image read/write feedback.

## Material

```c
typedef struct refractiveMaterial_s {
    float indexOfRefraction;
    float thickness;
    float roughness;
    float normalScale;
    float absorptionDistance;
    float absorptionColor[3];
    uint32_t reflectionMode;
    uint32_t refractionFlags;
} refractiveMaterial_t;
```

## Quality

| `r_refractionQuality` | Behavior |
|-----------------------|----------|
| 0 | Single sample |
| 1 | Roughness-selected mip |
| 2 | Bounded multi-sample |

No post-composition full-screen blur.

## Commands / debug

`refraction_status`, `material_refraction_status`, `r_refractionDebug` 1–10.
