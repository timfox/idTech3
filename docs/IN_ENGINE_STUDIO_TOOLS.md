# In-engine tools (id Studio–inspired)

The Vulkan + **Dear ImGui** overlay (`r_imgui`, toggle **F11** / `toggle_imgui`) provides a docked **inspector**: PostFX, volumetrics, physics, profiler, plus **scene-aware** panels backed by the last assembled `tr.refdef` batch:

- **Viewport** — refdef rect, FOV, view origin/axis, entity/dlight/surf counts  
- **Objects** — world BSP row, filtered ref-entity list (model path, reType), dynamic lights, first 96 registered `tr.models` slots  
- **Inspector** — details for selected world row, entity, dlight, or shader row (including morph channel snapshot and map shader counts)  
- **Shaders** — filterable clipper list of `tr.sortedShaders`, plus **`r_reloadShaders`** shortcut  
- **Profiler** — adds the same scene batch counts  

Scene snapshots are gathered through **`vk_imgui_scene.c`** (C only) because `tr_local.h` uses identifiers that are invalid in C++ (`or`, linkage clashes with ImGui TU).

This document describes the optional **Studio** layer: lightweight, in-game workflow helpers reminiscent of classic **id Studio** (session strip + command strip), not a full map editor.

## Toggle

| Cvar | Default | Meaning |
|------|---------|---------|
| `r_imgui` | `1` | Master switch for the ImGui overlay (inspector CPU work). |
| `r_studio_tools` | `0` | When `1`, enables **Studio** menu, **Studio / Session**, and **Studio / Console** panels. |

On startup with `r_studio_tools 1`, the Session and Console panels open automatically (with `r_imgui 1`). Enabling **Studio tools** from the inspector **Developer** menu at runtime also opens both panels.

Startup logs:

- `[VK][imgui] debug inspector r_imgui=…`
- `[VK][studio] r_studio_tools=…`

## Studio / Session

Read-only view of common session cvars (`mapname`, `fs_game`, `fs_basegame`, `sv_hostname`) plus **Quick commands** (`map_restart`, `disconnect`, `vid_restart`) and a small **dev** row (`noclip`, `god`, `notarget`) routed through the normal command buffer.

## Studio / Console

Single-line command entry (Enter or **Run**) that appends to the main **command buffer**—same as the drop-down console. A **local history** (last 48 lines) is shown above the input; it is **not** a mirror of the full engine log.

## Editor parity

Entity keys and conventions shared with **idTech3Radiant** are documented in **[EDITOR_BRIDGE.md](EDITOR_BRIDGE.md)**.

## Future work (non-goals for this pass)

- Live game view texture in the Viewport panel  
- Full scene graph / property grid wired to entities  
- Shader live hot-reload beyond `r_reloadShaders` / `vid_restart`  
- Two-way IPC with the external editor  

Those remain roadmap items; the Studio layer is intentionally small and safe to ship disabled by default.
