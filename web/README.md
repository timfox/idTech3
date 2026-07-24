# Surf web / WASM shell

Browser shell adapted from Quake3e-style Emscripten loaders. Native desktop already has TVD/TVL (`cl_tv.c`); the browser path feeds live bytes via `CL_TV_FeedBytes` / `CL_TV_GetPlayerList`.

## Status

idtech3 ships a **Vulkan** renderer for desktop. A playable browser build needs a WebGL/WebGPU backend (see `docs/WEBGPU_ROADMAP.md`). This directory provides:

- `client.html.in` / `loader.js.in` — Surf-branded page + JS loader (`SurfEngine` export name)
- `client-config.json` — default cvars / asset fetch map for `fs_game` `surf`
- Export symbols already present in `cl_tv.c` for when an Emscripten client links

Discord Rich Presence is stripped under `EMSCRIPTEN` in CMake.

## Build (when a web renderer exists)

```bash
# Requires Emscripten SDK on PATH
emcmake cmake -S . -B build-web -DIDTECH3_WEB=ON
emmake cmake --build build-web -j
```

Until WebGPU/OpenGL ES lands, use desktop:

```bash
# Record
tvrecord
# Playback
demo mymatch.tvd
# Live tap (loopback TVL1 on net_port)
set sv_tvLive 1
```

## Live TV in the browser (planned)

1. Dedicated / listen server with `sv_tvLive 1` (TCP `127.0.0.1:net_port`)
2. External relay → HTTP(S) byte stream
3. Loader calls `_CL_TV_FeedBytes` with TVL1 segments
