idtech3_demo - example mod for idTech3 engine
================================================

This pk3 contains configuration + scripts/js + bootstrap renderer assets (no maps or qagame).

Contents:
  - gameinfo.txt - window title "idTech3 Demo"
  - scripts/demo_bootstrap.shader + gfx/* + fonts/Inter-Regular.ttf - HUD / init without retail pak0
  - demo_features.cfg - renderer cvar toggles (PBR, volumetric fog, etc.)
  - scripts/js/demo_hooks.js - Duktape hooks (map_load, frame, HUD text)
  - demo_gameplay.cfg - hints for buildnavmesh / cl_* subsystem cvars

Requirements:
  - base/ with at least one .pk3 (use repo examples/demo_skeleton/base/z_minimal_bootstrap.pk3 for an empty tree).
  - Retail/base pk3s when you want stock maps, qagame, and menus.
  - Engine with Vulkan (for volumetric/PBR demos) and USE_DUKTAPE for JS hooks.

See examples/demo_game/README.md in the engine repository.
