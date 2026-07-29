# Deferred many-lights demo

`demo_deferred_many_lights.cfg` demonstrates the real-time deferred clustered
lighting path with 64 deterministic renderer-owned dynamic lights.

Run:

```text
exec demo_deferred_many_lights.cfg
vid_restart
map <your test map>
deferred_many_lights_status
deferred_status
cluster_status
```

If a map is already loaded, execute the config and continue playing. The
injected lights are placed in front of the camera and animate when
`r_deferredLightDemoAnimate 1`.

Important cvars:

```text
r_deferredLightDemo 1
r_deferredLightDemoCount 64
r_deferredLightDemoRadius 420
r_renderMode 3
r_deferredLighting 1
r_forwardPlus 1
r_forwardPlusDebug 0.35
```

Proof points:

- `deferred_many_lights_status` reports requested and last-added demo lights.
- `cluster_status` reports packed lights, cluster entries, and overflow.
- `deferred_status` confirms mode 3 deferred lighting and lightmap mode.
- `r_forwardPlusDebug 0.35` draws the live cluster/tile light occupancy overlay.

The demo is default-off. Production profiles should leave `r_deferredLightDemo`
at `0`; the synthetic lights are intended for renderer validation and live
demonstration only.
