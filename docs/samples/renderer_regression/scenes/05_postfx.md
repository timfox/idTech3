# Scene 05 — PostFX toggles

## Goal

Catch **MSAA, SMAA, SSAO, bloom** ordering and resolution mismatches.

## Layout

- **High-contrast edges** (geometry silhouette against skybox).
- **Contact geometry** (objects near ground) for SSAO.
- **Small bright highlights** for bloom.

## Pass criteria

- Each mode: **on vs off** does not change aspect ratio, viewport, or produce black buffer.
- SMAA/MSAA: edges stable; no **double image** or broken resolve.
- SSAO: contact shadowing appears near contact; no **screen-wide** darkening bug.

## Cvars / notes

- List your project cvars for MSAA, SMAA, SSAO, bloom; run toggles in one session and record order.
