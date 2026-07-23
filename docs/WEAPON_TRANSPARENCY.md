# Weapon Transparency (Phase 2.6)

## Classes

```text
WEAPON_TRANSPARENT_OPTIC
WEAPON_REFRACTIVE_OPTIC
WEAPON_EMISSIVE_RETICLE
WEAPON_MUZZLE_SMOKE
WEAPON_MUZZLE_FLASH
```

## Order

```text
world WBOIT → world refraction/special → weapon opaque → optic → emissive → bloom
```

Optics sample current world `SCENE_LINEAR_HDR`; no world revealage writes; reticles separate from lens opacity.

Commands: `weapon_transparency_status`, `r_weaponTransparencyDebug` 1–6.
