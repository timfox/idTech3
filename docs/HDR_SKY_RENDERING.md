# HDR sky rendering

**Status:** Visible HDR sky uses scene-linear RGBA32F outerbox faces into SceneHDR.  
**Commands:** `sky_hdr_status`, `sky_hdr_validate`, `sky_hdr_capture`  
**Debug:** `r_skyHdrDebug`, `r_skyLod`, `r_skyExposureEV`, `r_skyLuminanceScale`

## First flattening stage (fixed)

Before this change, EXR radiance was crushed by **Reinhard + gamma into RGBA8 UNORM**
in `SkyboxHDR_BuildDisplayFaces` (`SKY_HDR_VALUES_CLAMPED` / `SKY_RENDERED_TO_LDR_TARGET`).
That was `FIRST_STAGE_FLATTENING_SKY`.

Visible sky now:

```text
EXR/HDR decode (float)
→ equirect sample (linear, no IBL exposure bake)
→ × exp2(r_skyExposureEV) × r_skyLuminanceScale
→ RGBA32F six-face outerbox (mip 0 only)
→ skybox_pipeline → float SceneHDR color target
→ shared exposure → tonemap → display
```

Specular prefilter / irradiance cubemaps remain **IBL-only** and are not used for the visible background.

## Color-space contract

| Stage | Space |
|-------|--------|
| OpenEXR load | float linear (tinyexr) |
| Visible faces | `SKY_SCENE_LINEAR_HDR` (RGBA32F) |
| IBL intensity | `r_skyboxHDR_exposure` × `r_skyboxHDR_intensity` on cubemap bake |
| Exposure / tonemap | shared SceneHDR spine (`docs/COLOR_PIPELINE.md`) |

Do **not** multiply the final LDR sky as a fix. Use `r_skyExposureEV`.

## Resolution

Visible faces are **not** the 512 IBL cubemap. Auto size is ≈ `equirectWidth/2`
(clamped 512–2048). A 4096×2048 EXR therefore bakes **2048²** faces. Override with
`r_skyFaceSize` (e.g. `1024` for memory, `2048` for max detail). Old 512² bakes
looked heavily pixelated on 4K panoramas.

## Ownership

`r_skyOwner 2` (HDR): classic shells suppressed unless HDR outerbox faces exist.
`RB_StageIteratorSky` notes `SCENE_HDR_SKY_ATMOSPHERE` when drawing HDR faces.

## Eye adaptation

Enabled automatically for HDR sky maps (`r_exposure_auto 1`). Looking toward a
bright sky lowers exposure; looking away raises it gradually. See
[SKY_EXPOSURE.md](SKY_EXPOSURE.md).

## Related

- [BSP30_FORMAT_SUPPORT.md](BSP30_FORMAT_SUPPORT.md) — map sidecar / worldspawn
- [COLOR_PIPELINE.md](COLOR_PIPELINE.md) — pass order
- [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) — `skybox_hdr*` keys
