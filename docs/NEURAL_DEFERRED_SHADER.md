# Convolutional neural deferred shading

The renderer has an opt-in integration boundary for He et al., “A
Convolutional Neural Deferred Shader for Physics Based Rendering”
(PBNDS+, arXiv:2512.19522).

Enable the contract with:

```text
exec vulkan_overlay_neural_deferred.cfg
neural_deferred_status
```

## Ownership

PBNDS+ consumes the authoritative mode-3 G-buffer and incident-light/HDR
features. It does not own primary visibility, material decoding, cluster
construction, shadow pages, or the legacy shader translation seam. Classical
clustered deferred lighting remains the fallback and comparison reference.

The initial output owner is `compare_only`. A future validated model may add a
bounded SceneHDR advisory blend, but it must remain gated by the same G-buffer
generation and cluster generation and must never silently replace lighting.

## Feature contract

The 14-channel contract is: linear albedo RGB, encoded normal RGB, specular
RGB, roughness, depth, and incident-light RGB. The model is expected to use
the paper’s sampled incoming-light formulation and convolutional feature-map
layout; weight packing, convolution dispatch, and model manifests are not yet
implemented.

`neural_energy_guard.comp` is the first executable proof. It applies the
paper’s dark-environment regularization at the inference boundary, preventing
nonzero learned output when incident energy is zero or negligible.

Current status: ownership/input contract plus energy-guard shader. No trained
weights, CNN inference, relighting asset format, or golden neural-vs-GGX
capture is claimed yet.

