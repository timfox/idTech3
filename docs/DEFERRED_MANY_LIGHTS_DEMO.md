# Deferred many-lights demo

`demo_deferred_many_lights.cfg` demonstrates the real-time deferred clustered
lighting path with 64 deterministic renderer-owned dynamic lights.

Run:

```text
map <your test map>
exec demo_deferred_many_lights.cfg
deferred_many_lights_status
deferred_status
cluster_status
```

If a map is already loaded, execute the config and continue playing. The
injected lights are placed in front of the camera and animate when
`r_deferredLightDemoAnimate 1`.

The light field uses `r_deferredLightDemoDistance` (default `520`) to control
its forward placement. Lower it for compact maps when the colored lights are
outside the current room; the supported range is `160` to `1024` world units.
`r_deferredLightDemoEnergy` controls color contribution independently of
radius/culling; the demo uses `1.25` to keep saturated colors readable in HDR.

Important cvars:

```text
r_deferredLightDemo 1
r_deferredLightDemoCount 64
r_deferredLightDemoRadius 420
r_renderMode 3
r_classicLighting 0
cl_autoGraphicsProfile 0
r_deferredLighting 1
r_forwardPlus 1
r_forwardPlusDebug 0
```

Proof points:

- `deferred_many_lights_status` reports requested and last-added demo lights.
- `r_classicLighting 0` is required because OpenArena's compatibility profile
  otherwise restores classic lighting and makes opaque surfaces ineligible.
- `cl_autoGraphicsProfile 0` prevents the QVM compatibility profile from
  reapplying those classic defaults during renderer restart.
- `cluster_status` reports packed lights, cluster entries, and overflow.
- `deferred_status` confirms mode 3 deferred lighting and lightmap mode.
- `r_forwardPlusDebug 0.35` draws the live cluster/tile light occupancy overlay
  when explicitly enabled; the demo defaults to a clean lighting view.

Capture evidence after the scene has rendered for several frames. For stable
evidence, use a fresh client session for each view and capture at 640x480:

```text
set r_clusterDebug -1
set r_forwardPlusDebug 0
set r_deferredGBufferDebug 0
wait 30
screenshotJPEG dm4ish_clustered_lighting

set r_forwardPlusDebug 0.35
wait 30
screenshotJPEG dm4ish_forwardplus_occupancy

set r_forwardPlusDebug 0
set r_deferredGBufferDebug 1
wait 30
screenshotJPEG dm4ish_deferred_gbuffer
```

Vulkan screenshot readback waits for the submitted frame fence with a finite
timeout. A failed WSI submission skips that capture instead of blocking the
renderer indefinitely.

The demo cfg waits out the post-map restart debounce, applies its latched mode
with `vid_restart keep_window`, and waits for the rebuilt renderer before
status/capture commands. No separate manual restart is required.

The demo is default-off. Production profiles should leave `r_deferredLightDemo`
at `0`; the synthetic lights are intended for renderer validation and live
demonstration only.

The demo pins `r_textureMode` to `GL_LINEAR_MIPMAP_LINEAR` so oblique stair and
wall textures do not show abrupt mip-level bands during captures.
