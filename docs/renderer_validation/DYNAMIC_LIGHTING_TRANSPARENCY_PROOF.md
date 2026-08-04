# Dynamic lighting and transparency proof

## Executed checks

```text
bash tests/scripts/test_cluster_contract.sh
bash scripts/test_forward_plus_tier_b.sh
GAME_BASE=docs/renderer_validation/devdata/rtest_base \
  ./scripts/renderer_regression_maps.sh
```

Results: clustered ABI/list ownership checks passed, Forward+ Tier-B wiring
passed, and all six authored regression maps loaded successfully.

## Vulkan runtime evidence

The client was launched with:

```text
r_forwardPlus 1
r_renderMode 3
r_oit 1
r_oitForwardPlus 1
map open_void
```

The Vulkan log reported:

```text
GPU: Discrete NVIDIA RTX PRO 6000 Blackwell Workstation Edition
[VK][deferred] r_deferredLighting=1 (G-buffer diffuse + Forward+ tiles; point+spot)
[VK] OIT implementation: WBOIT
[VK] OIT lit translucent path: Forward+ clustered
[VK][Forward+] light grid: 64x48 tiles 16 Z-slices (49152 clusters)
[VK][Forward+] ... device-local light SSBO + staging ... (tile cull + PBR read VRAM)
[VK][oit] Phase 2.1/2.5 WBOIT contract frozen v2
[init][server] ready map=open_void
```

This proves initialization, resource ownership, light-grid allocation, and
the selected opaque/deferred plus transparent/WBOIT execution routes.

## OpenArena framebuffer proof

Using the installed OpenArena `baseoa` data and the `aggressor` map, the same
Vulkan path completed a 1920x1080 capture:

```text
Wrote screenshots/oa_dynamic_transparency.jpg
454536 bytes
SHA-256 55333ec91b6c0c4a50b06266700d33deb0ad1e127e5e96812f00fb7b8d843297
```

Capture: [openarena_dynamic_transparency.jpg](../../tests/data/golden/captures/openarena_dynamic_transparency.jpg)

### Same-scene transparency A/B

With the same `aggressor` camera and deferred/G-buffer setup:

| Route | Bytes | SHA-256 |
| --- | ---: | --- |
| Forward+ transparency | 343133 | `bd6b546fc6bbfdb3a268c96bc164becb3c879c9aded632d8f745c526eb5e4fb1` |
| WBOIT transparency | 454484 | `7a82bffbc9ba1661fb680415e6c1c54848517ad8014e99fcae056a9760385cc5` |

Captures: [Forward+](../../tests/data/golden/captures/openarena_forwardplus.jpg), [WBOIT](../../tests/data/golden/captures/openarena_wboit.jpg).
The distinct hashes, while sharing the same deferred light-grid setup, prove
that the selected transparent route affects the presented frame.

The minimal validation pack still lacks `cgame.qvm`, but OpenArena supplies
the complete gameplay VM and now provides the end-to-end framebuffer proof.
