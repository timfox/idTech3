# Transparent Shadows (Phase 2.6)

| Route | Policy |
|-------|--------|
| MASKED | Alpha-tested cast + receive |
| WBOIT | Receive only; no translucent cast by default |
| ADDITIVE | No cast |
| REFRACTIVE | Receive opaque shadows |
| SPECIAL_BLEND | Compatibility |
| WEAPON_OPTIC | Weapon-specific receive |

Flags: `TRANSPARENT_SHADOW_RECEIVE`, `TRANSPARENT_SHADOW_CAST_MASKED`, `TRANSPARENT_SHADOW_CAST_APPROXIMATE`, `TRANSPARENT_SHADOW_NONE`.

No general colored translucent shadow casting in this phase.

Commands: `transparent_shadow_status`, `r_transparentShadowDebug`.
