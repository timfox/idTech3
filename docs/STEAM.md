# Steam API

Client Steamworks integration (`USE_STEAM`) for achievements, overlay, rich presence, Steam Deck detection, and Steam Input status. Peer networking over Steam Datagram Relay is separate — see [P2P_NETWORKING.md](P2P_NETWORKING.md) (`USE_STEAM_NETWORKING`).

## Build

`USE_STEAM` defaults to **ON**. When the Steamworks SDK is found, the full API is linked; without an SDK the client still builds using the stub path.

Requires the [Steamworks SDK](https://partner.steamgames.com/doc/sdk). This machine’s default path is `/home/tim/SteamWorks/sdk` (auto-detected when present, or set via `STEAMWORKS_SDK` / `-DSTEAMWORKS_SDK=`).

```bash
# Default build already enables Steam when the SDK is available
./scripts/compile_engine.sh vulkan

# Explicit (same as default)
./scripts/compile_engine.sh vulkan steam

# Disable Steam
./scripts/compile_engine.sh vulkan no-steam

# Override SDK root
STEAMWORKS_SDK=/path/to/sdk ./scripts/compile_engine.sh vulkan

# Or configure CMake directly
cmake -S . -B build-vk-Release \
  -DUSE_STEAM=ON \
  -DSTEAMWORKS_SDK=/home/tim/SteamWorks/sdk \
  ...
```

CMake resolves `STEAMWORKS_SDK` from the cache variable first, then `$ENV{STEAMWORKS_SDK}`, then `/home/tim/SteamWorks/sdk` when that tree exists. When the SDK is found it:

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

Steam Input this pass initializes Steam Input and reports connected controller
count in `steam_status`. Full Steamworks action-set remapping / glyph APIs are
not wired yet; Deck play uses **SDL3 gamepad → `PAD0_*` keys** with binds from
`steamdeck.cfg`.

## Steam Deck controls

When `IsSteamRunningOnSteamDeck()` (or `SteamDeck=1` / Gamescope env) is true,
the client sets `in_steamDeck 1` and `exec steamdeck.cfg`:

| Control | Action |
|---------|--------|
| Left stick | Move (analog) |
| Right stick | Look (analog freelook) |
| A | Jump |
| B | Crouch |
| X | Scores |
| Y | Screenshot |
| RT / RB | Attack |
| LT | Crouch (alt) |
| LB / L3 | Walk (hold; `cl_run 1`) |
| START / BACK | Pause menu (same as ESC) |
| D-pad | Menu navigation (unbound in-game) |

Surf JS overlays (`ui/surf/menu.js`, `mapselect.js`, `leaderboard.js`) accept
`PAD0_DPAD_*`, `PAD0_A` / `PAD0_B` via `ui/surf/pad.js`.

Dual-stick look requires `in_joystickUseAnalog 1` plus the stick binds in
`steamdeck.cfg`, and `CL_JoystickMove` consuming `AXIS_YAW` / `AXIS_PITCH`.

## Related

- [P2P_NETWORKING.md](P2P_NETWORKING.md) — `net_p2p` / Steam SDR
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) — CMake feature flags
- [PLATFORM_GATED.md](PLATFORM_GATED.md) — product SKU gating for store release
- [config/steamdeck.cfg](../config/steamdeck.cfg) — Deck defaults (gamepad binds, fullscreen, 60 FPS, Vulkan)
