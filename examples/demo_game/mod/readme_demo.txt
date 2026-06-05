# idtech3_demo - example mod for Gopex idTech3 engine
================================================

This pk3 contains configuration + scripts + bootstrap renderer assets (no maps or game logic modules).

Contents:
  - gameinfo.txt - window title "Gopex Engine Demo"
  - scripts/demo_bootstrap.shader + gfx/* + fonts/Inter_28pt-Regular.ttf (repo fonts/) - HUD / init without retail pak0
  - demo_features.cfg - renderer cvar toggles (PBR, volumetric fog, etc.)
  - demo_physics.cfg - Bullet middleware + active ragdoll motor layer
  - scripts/js/demo_hooks.js - Duktape hooks (map_load, frame, HUD text)
  - demo_gameplay.cfg - hints for buildnavmesh / cl_* subsystem cvars

Requirements:
  - base/ with at least one .pk3 (use repo examples/demo_skeleton/base/z_minimal_bootstrap.pk3 for an empty tree).
  - Your compatible base pk3s when you want stock maps, qagame, and menus.
  - Engine with Vulkan (for volumetric/PBR demos) and USE_DUKTAPE for JS hooks.

See examples/demo_game/README.md and DEMO_IDENTITY.md in the engine repository.
