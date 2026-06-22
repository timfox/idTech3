# Client runtime layout (2026)

Domain folders mirror the target `runtime/client/` tree ([REPOSITORY_LAYOUT_2026.md](../../docs/core/REPOSITORY_LAYOUT_2026.md)).

| Folder | Responsibility |
|--------|----------------|
| `core/` | `CL_Init`, `CL_Frame`, connect, cmds, gameframe, parse |
| `world/` | Open world, districts, procedural scatter (`USE_OPEN_WORLD`) |
| `media/` | Demo, download, cinematics |
| `platform/` | cURL, Steam, VoIP, WebSocket, Mumble |
| `*.c` (root) | Console, UI, HUD, engine sprites/decals (shell) |

Generative ML client code lives under `src/extensions/generative/` (not here).

CMake: `cmake/client/ClientSources.cmake` + `ClientExtensionSources.cmake`.
