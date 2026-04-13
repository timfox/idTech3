# Scene 03 — Emissive

## Goal

Catch **emissive** masking bugs, HDR bloom interaction, and tone-map crushing.

## Layout

- **Dark room** (low ambient): one emissive-only quad (no base color contribution).
- One quad with **base color + emissive** (different hues).
- One **very bright** emissive small source (stress test for bloom / firefly handling).

## Materials

- Emissive mask and strength per [PBR_TEXTURES.md](../../../PBR_TEXTURES.md).

## Pass criteria

- Emissive-only surfaces read **correct hue** in darkness.
- Combined surface shows both contributions without doubling or losing emissive.
- Extreme emissive does not **persistently white-out** the frame after look-away (unless intentional bloom).

## Cvars / notes

- Toggle bloom / exposure-related cvars if you have them; note baseline screenshots.
