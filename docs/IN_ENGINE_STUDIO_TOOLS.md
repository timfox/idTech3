# In-engine tools (id Studio–inspired)

The Vulkan + **Dear ImGui** overlay (`r_imgui`, toggle **F11** / `toggle_imgui`) provides a docked **inspector**: PostFX, volumetrics, physics, profiler, plus **scene-aware** panels backed by the last assembled `tr.refdef` batch:

- **Viewport** — refdef rect, FOV, view origin/axis, entity/dlight/surf counts  
- **Objects** — world BSP row, filtered ref-entity list (model path, reType), dynamic lights, first 96 registered `tr.models` slots  
- **Inspector** — details for selected world row, entity, dlight, or shader row (including morph channel snapshot and map shader counts)  
- **Shaders** — filterable clipper list of `tr.sortedShaders`, plus **`r_reloadShaders`** shortcut  
- **Profiler** — adds the same scene batch counts  

Scene snapshots are gathered through **`vk_imgui_scene.cpp`** (`-fno-operator-names` on GCC/Clang so `tr_local.h` field names like `or` remain valid).

This document describes the optional **Studio** layer: lightweight, in-game workflow helpers reminiscent of classic **id Studio** / id Tech 7 tool strips — not a full map editor (BSP stays in [idTech3Radiant](RADIANT.md)).

## Toggle

| Cvar | Default | Meaning |
|------|---------|---------|
| `r_imgui` | `1` | Master switch for the ImGui overlay (inspector CPU work). |
| `r_studio_tools` | `0` | When `1`, enables the **Studio** menu and the five Studio panels below. |

On startup with `r_studio_tools 1` (and `r_imgui 1`), all five Studio panels open. Enabling **Studio tools** from the inspector **Developer** menu at runtime also opens all five. **Window → Reset workspace layout** re-docks them along the bottom strip with Session / Console / Entities / Paint / Animation.

Menus when Studio is on:

- **Studio** — Session, Console, Entities, Paint, Animation  
- **Window** — same five under a Studio separator  

Startup logs:

- `[VK][imgui] debug inspector r_imgui=…`
- `[VK][studio] r_studio_tools=…`

## Studio panels

| Panel | Role |
|-------|------|
| **Studio / Session** | Read-only session cvars (`mapname`, `fs_game`, `fs_basegame`, `sv_hostname`) plus quick commands (`map_restart`, `disconnect`, `vid_restart`) and cheat-class buttons (`noclip`, `god`, `notarget`). |
| **Studio / Console** | Single-line command entry (Enter or **Run**) into the main command buffer. Local history (last 48 lines); not a full engine log mirror. |
| **Studio / Entities** | Lists `misc_billboard` / flipbook / imposter / `misc_decal` from the loaded BSP entity lump. Export writes Radiant-style snippets to `studio_exportents.cfg`. See [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) / [RADIANT.md](RADIANT.md). |
| **Studio / Paint** | Material-weight paint (`r_materialPaint`) — brush radius/strength/channels, NDC stroke, `paint_save` / `paint_load` / `paint_status`. Sidecar `maps/<map>.paint`. See [MATERIAL_BLEND.md](MATERIAL_BLEND.md). |
| **Studio / Animation** | **Stub strip:** toggle `g_animgraph` and path echo. Does not yet call `G_AnimGraph_Load` / status — see [ANIMGRAPH.md](ANIMGRAPH.md). Closable via its window `open` flag like the other Studio panels. |

## Editor parity

Entity keys and conventions shared with **idTech3Radiant** are documented in **[EDITOR_BRIDGE.md](EDITOR_BRIDGE.md)**.

## Two-way editor sync (Radiant)

Export from **Studio / Entities** writes `studio_exportents.cfg`. In idTech3Radiant, run `Editor/bridge_tools.py` (from [examples/radiant/Editor/](../examples/radiant/Editor/)) to paste the block into your `.map`. See [RADIANT.md](RADIANT.md).

## Future work

- Live game view texture in the Viewport panel  
- Viewport click-to-select / property grid wired to entities  
- Shader live hot-reload beyond `r_reloadShaders` / `vid_restart`  
- Wire Studio / Animation to real `G_AnimGraph_*` load/status  
- Native IPC with the external editor (file bridge is the current approach)
