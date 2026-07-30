# Native RTS Session GUI

The RTS GUI is a native session layer inspired by the useful boundary in 0 A.D.: UI asks a small simulation-owned interface for player state and posts commands back through it. It does not embed 0 A.D.'s SpiderMonkey page runtime or XML pages; the scriptable presentation path uses the engine's Duktape JavaScript runtime.

`modules/rts/rts_gui.cpp` owns deterministic selection and selected-unit orders. Its C-compatible contract is in `modules/rts/rts_public.h`; the engine-facing API stays C while the implementation uses the repository's C++20 baseline. The client shell draws the panel in `runtime/client/core/cl_rts_gui.c`.

## Session Surface

- `RTS_GuiSelectRect(player, minX, minY, maxX, maxY)` replaces the player's persistent selection.
- `RTS_GuiGetState(player, &state)` supplies resource, turn, selection, health, and command-queue data.
- `RTS_GuiIssueMoveSelected(player, x, y)` queues deterministic move commands for the next turn.
- `RTS_GuiClearSelection(player)` clears the persistent selection.

The client renders resource/turn state and a selected-unit panel while RTS entities exist. It is enabled by `cl_rtsHud 1` and uses only the normal 640x480 virtual screen primitives, so it works with the Vulkan renderer without a second widget renderer.

Useful console checks:

```text
rts_gui_select_all
rts_gui_status
rts_gui_move 256 192
rts_gui_clear
```

## JavaScript Native Script API

The JavaScript environment receives `idtech3.rts` when the Duktape bindings are registered:

```javascript
var status = idtech3.rts.state();
idtech3.rts.selectRect(1, -128, -128, 128, 128);
idtech3.rts.moveSelected(1, 256, 192);
```

`state([player])` returns `{ playerId, turn, entityCount, resources, selectedCount, primarySelection, primaryHitpoints, primaryResources, pendingCommands }`. `selectRect`, `clearSelection`, and `moveSelected` accept an explicit player ID; player 1 is the default for `state` and `clearSelection`.

`base/scripts/js/rts_hud.js` is a minimal JS consumer. The local 0 A.D. demo config loads it with:

```text
js_reload scripts/js/rts_hud.js
```

## Scope

This is the session-gameplay slice of the 0 A.D. GUI port: selection, top resource state, selected entity information, and commands. Lobby, pregame, diplomacy, production queues, tooltips, notifications, and the XML style system remain separate future ports. Keeping those out of the deterministic simulation lets native C++ state and JavaScript presentation evolve independently.
