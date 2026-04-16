# Scene 02 - Metallic vs roughness

## Goal

Validate **metalness/roughness** response and energy sanity (specular/diffuse balance).

## Layout

- **Row of spheres or panels**: fully dielectric (metalness 0) stepped **roughness** 0.05 → 1.0.
- **Row**: high metalness, same roughness steps.
- One **chrome or mirror-like** ball (roughness near 0, metalness 1) with an HDR or busy environment visible.

## Materials

- Use project PBR convention ([PBR_TEXTURES.md](../../../PBR_TEXTURES.md)).
- Optional: single panel with **clearcoat** if your content pipeline supports it.

## Pass criteria

- Roughness increases **blur** specular consistently; no surfaces that blow out to white or go black unexpectedly.
- Dielectric row shows **F0** behavior; metal row shows colored reflections where appropriate.
- Optional: cycle `r_pbr_debug` modes and confirm sane visualization.

## Cvars / notes

- `r_pbr` (or project equivalent) on/off sanity check.
