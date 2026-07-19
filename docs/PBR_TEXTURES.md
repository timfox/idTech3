## PBR packed textures (project standard)

This fork’s Vulkan PBR path supports several packed “physical” map layouts to stay compatible with different pipelines. To reduce confusion, the **recommended project standard is `ORM`** (matches glTF 2.0 convention):

- **Suffix**: `*_orm` (e.g. `textures/foo/metalplate_orm.tga`)
- **Channels (file)**:
  - **R**: Ambient Occlusion (AO)
  - **G**: Roughness
  - **B**: Metalness
  - **A**: Specular scale (**recommended to keep at 1.0 / 255** for plain `ORM`)

If you want to drive the dielectric/specular intensity from texture alpha, use **`ORMS`**:

- **Suffix**: `*_orms`
- **Channels (file)**:
  - **R**: AO
  - **G**: Roughness
  - **B**: Metalness
  - **A**: Specular scale

## Shader keywords

You can specify the packed map explicitly in a `.shader` stage:

- `ormMap <path>` / `ormsMap <path>`
- `rmoMap <path>` / `rmosMap <path>`
- `moxrMap <path>` / `mosrMap <path>`

Or you can rely on **auto-discovery**: when a stage has a base `map`, the engine will try to find packed maps using the suffixes listed below.

## Supported packed layouts (auto-discovery + Vulkan channel swizzles)

Internally, the PBR shader expects the sampled packed map as:

- **R**: AO
- **G**: Roughness
- **B**: Metalness
- **A**: Specular scale (optional)

The Vulkan renderer uses texture view swizzles to remap file channels into that layout.

### `ORM` / `ORMS` (recommended)
- **Suffix**: `*_orm` / `*_orms`
- **File**: `R=AO, G=Roughness, B=Metalness, A=Specular(optional)`
- **Swizzle**: identity

### `RMO` / `RMOS`
- **Suffix**: `*_rmo` / `*_rmos`
- **File**: `R=Roughness, G=Metalness, B=AO, A=Specular(optional)`
- **Swizzle**: output = `{ R=B, G=R, B=G, A=1|A }`

### `MOXR` / `MOSR`
- **Suffix**: `*_moxr` / `*_mosr`
- **File (`MOXR`)**: `R=Metalness, G=AO, B=unused, A=Roughness`
- **File (`MOSR`)**: `R=Metalness, G=AO, B=Specular, A=Roughness`
- **Swizzle**: output = `{ R=G, G=A, B=R, A=1|B }`

## Notes

- **BaseColor/Albedo** is treated as **sRGB** by the Vulkan PBR path; packed maps (AO/Rough/Metal/etc.) are treated as **linear**.
- If you use plain `*_orm` (no “S”), keep **alpha = 1.0** in the file if possible, since alpha can influence dielectric specular intensity in the current shader.
- **Ultra-black / optical blacks:** a very low albedo + high roughness is only an approximate game proxy. Measured angular scattering (TIS, grazing specular) differs strongly across Vantablack, Musou, velvet, and matte coatings — see **[HOWDARK.md](HOWDARK.md)** (Filip & Vávra arXiv:2601.05094 research scaffold).
- **Analytic GGX capacity:** shipping PBR uses compact GGX + Schlick F + Smith G. Fitting measured BRDFs can need hybrid neural enhancement of bottleneck terms (F, G, 1/E) — see **[NEBRDF.md](NEBRDF.md)** (Shen et al. arXiv:2604.24081 research scaffold; no weights in-engine yet).

## Multi-material height-blend

Vertex RGBA weights + optional height from `normalHeightMap` alpha — see **[MATERIAL_BLEND.md](MATERIAL_BLEND.md)** (`materialBlend vertex`, `layerMap`, `r_materialBlend`).

## Parallax occlusion mapping (Vulkan PBR)

When a **normal map** and a packed **ORM/physical** map are both bound on the default PBR path, the fragment shader can **ray-march height** from the physical map’s **occlusion (R)** channel (height lives there for POM; `parallaxDepth` / `parallaxBias` still apply). Toggle and tuning: **`r_pom`**, **`r_pomSteps`**, **`r_pomScale`**, **`r_pomShadow`**, **`r_pomShadowSteps`** - see `tr_init.c` and [RENDERERS.md](RENDERERS.md). POM is disabled on some multi-UV / lightmapped PBR variants.

## Clearcoat, sheen, anisotropy (Vulkan)

- **Clearcoat** (`clearcoatMap` + `clearcoatScale <strength> <roughness>`): direct lighting uses a second GGX lobe; the base color is dimmed by approximate energy conservation before the coat is added.
- **Sheen** (`sheenMap` + `sheenScale <rgb>` or `sheenScale <r> <g> <b> [<roughness>]`): optional fourth value is **sheen roughness** (default `0.5`). The fragment shader uses a **Charlie** distribution with Smith visibility (not the old flat `NL * 0.05` term).
- **Anisotropy map**: direct specular uses **anisotropic GGX** NDF and **anisotropic visibility** when `r_pbr_anisotropicSpecular` is 1. **IBL** can stretch roughness from the same map when `r_pbr_iblAnisoStretch` is greater than 0 (default 1).

