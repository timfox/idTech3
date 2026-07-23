# Classic Blend Compatibility (Phase 2.6)

Destination-dependent classic blends cannot use WBOIT.

## Routes

```text
SPECIAL_BLEND_MULTIPLY
SPECIAL_BLEND_FILTER
SPECIAL_BLEND_DST_COLOR
SPECIAL_BLEND_INVERSE
SPECIAL_BLEND_MULTISTAGE
```

Policy: preserve original blend factors where practical; render in `SCENE_LINEAR_HDR`; sort back-to-front when needed; run after WBOIT and before weapon/bloom; never write WBOIT accum/revealage.

Commands: `special_transparency_status`, `material_blend_status`, `r_specialTransparencyDebug`.
