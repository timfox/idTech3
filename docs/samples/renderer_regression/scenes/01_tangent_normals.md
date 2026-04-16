# Scene 01 - Tangent space / normal maps

## Goal

Catch **weld/tangent** bugs (e.g. MikkTSpace), **normal map orientation**, and **UV seam** lighting errors.

## Layout

- **3–4 quads or boxes** sharing edges with **split UVs** vs **welded UVs** (same position, different UV islands).
- One **mirrored UV** strip (negative scale or overlapping island) with a clear asymmetric normal pattern (e.g. embossed text or arrow).
- One **cylinder or sphere** with a tangent-space normal map (not object-space).

## Materials

- Tiling normal map with obvious directionality (bricks, noise with bias, or numeric labels in the normal texture).
- One flat surface with **no** normal map as reference.

## Pass criteria

- No obvious **lighting discontinuity** along intended-smooth edges (except where UV split is intentional).
- Mirrored UV region looks **symmetric** under moving light, not inverted or black.
- Cylinder/sphere: no **seam band** worse than reference engine build.

## Cvars / notes

- Toggle `r_normalMapping` if present; compare off/on.
- After changes under `mikktspace` or mesh tangent upload, **re-run this scene first**.
