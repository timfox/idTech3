# Client runtime layout (2026)

Domain folders mirror the target `runtime/client/` tree ([REPOSITORY_LAYOUT_2026.md](../../docs/core/REPOSITORY_LAYOUT_2026.md)).

| Folder | Responsibility |
|--------|----------------|
| `core/` | `CL_Init`, `CL_Frame`, connect, cmds, gameframe, parse |
| `world/` | Open world, districts, procedural scatter (`USE_OPEN_WORLD`) |
| `media/` | Demo, download, cinematics |
| `platform/` | cURL, Steam, VoIP, WebSocket, Mumble |
| `shell/` | Console, UI, HUD, fonts, engine sprites/decals, particles |
| root | Shared API headers only (`client.h`, `keys.h`, `keycodes.h`) |

Generative ML client code lives under `extensions/generative/` (not here). Thin client headers for FLUX/TRELLIS/USD live under `shell/`.

CMake: `cmake/client/ClientSources.cmake` + `ClientExtensionSources.cmake`.
