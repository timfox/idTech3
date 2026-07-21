# Steam API

Client Steamworks integration (`USE_STEAM`) for achievements, overlay, rich presence, Steam Deck detection, and Steam Input status. Peer networking over Steam Datagram Relay is separate — see [P2P_NETWORKING.md](P2P_NETWORKING.md) (`USE_STEAM_NETWORKING`).

## Build

Requires the [Steamworks SDK](https://partner.steamgames.com/doc/sdk). This machine’s default path is `/home/tim/SteamWorks/sdk`.

```bash
# Convenience (sets USE_STEAM + STEAMWORKS_SDK)
./scripts/compile_engine.sh vulkan steam

# Override SDK root
STEAMWORKS_SDK=/path/to/sdk ./scripts/compile_engine.sh vulkan steam

# Or configure CMake directly
cmake -S . -B build-vk-Release \
  -DUSE_STEAM=ON \
  -DSTEAMWORKS_SDK=/home/tim/SteamWorks/sdk \
  ...
```

CMake resolves `STEAMWORKS_SDK` from the cache variable first, then `$ENV{STEAMWORKS_SDK}`. When the SDK is found it:

- Compiles `steam_shared.c` / `cl_steam.c` as C++ (Steamworks headers are C++) with a C ABI
- Copies `libsteam_api.so` and `steam_appid.txt` into the build directory
- `compile_engine.sh` also copies them into `release/` next to the client binary (`$ORIGIN` rpath)

Without the SDK, the client still builds: `cl_steam.c` uses the stub branch (`Steam: not compiled`), and Deck can still be detected via `SteamDeck=1` / Gamescope env.

## Runtime files

| File | Purpose |
|------|---------|
| `steam_appid.txt` | Next to the executable. Repo ships placeholder **AppID 480** (Spacewar) for local testing. Replace with your real AppID before shipping. |
| `libsteam_api.so` | Steamworks redistributable (linux64). Must sit next to `idtech3`. |
| `base/steamdeck.cfg` | Auto-`exec` when Deck is detected (`in_steamDeck 1`). |

Steam client must be running for `SteamAPI_Init` to succeed. Failure is non-fatal: the engine continues without Steam.

## Cvars

| Cvar | Default | Notes |
|------|---------|-------|
| `in_steamDeck` | `0` | ROM when Steam API detects Deck; archive when stub/env path |
| `cl_steamOverlay` | `1` | Honor overlay active state (pause / `Steam_IsOverlayActive`) |
| `cl_steamRichPresence` | `1` | Publish `status` rich presence from `CL_Frame` |
| `cl_steamPauseOnOverlay` | `1` | Set `cl_paused` while the overlay is open; clears only if Steam set the pause |

## Console commands

| Command | Effect |
|---------|--------|
| `steam_status` | Init state, persona, SteamID, Deck, overlay, Input controller count |
| `steam_achievement <api_name>` | `SetAchievement` + `StoreStats` |
| `steam_achievement_clear <api_name>` | `ClearAchievement` + `StoreStats` |

## Code ownership

| Module | Owns |
|--------|------|
| [`engine/core/steam_shared.c`](../engine/core/steam_shared.c) | `SteamAPI_Init` / `Shutdown` / `RunCallbacks` (client + dedicated when networking is on) |
| [`runtime/client/platform/cl_steam.c`](../runtime/client/platform/cl_steam.c) | Overlay callback, pause-on-overlay, achievements, Steam Input status, console cmds |
| `CL_Frame` | `Steam_Frame()` then rich presence |
| `CL_Shutdown` | `Steam_Shutdown()` (client teardown only; shared shutdown stays in `Com_Shutdown`) |

Steam Input this pass is **status-only** (`Init` + connected controller count in `steam_status`). Full action-set remapping is not wired yet.

## Related

- [P2P_NETWORKING.md](P2P_NETWORKING.md) — `net_p2p` / Steam SDR
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) — CMake feature flags
- [PLATFORM_GATED.md](PLATFORM_GATED.md) — product SKU gating for store release
- [config/steamdeck.cfg](../config/steamdeck.cfg) — Deck defaults (gamepad, fullscreen, 60 FPS, Vulkan)
