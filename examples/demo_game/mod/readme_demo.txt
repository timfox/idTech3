idtech3_demo — example mod for idTech3 engine
================================================

This pk3 contains configuration + scripts/js (no maps, textures, or qagame).

Contents:
  - demo_features.cfg — renderer cvar toggles (PBR, volumetric fog, etc.)
  - scripts/js/demo_hooks.js — Duktape hooks (map_load, frame, HUD text)
  - demo_gameplay.cfg — hints for buildnavmesh / cl_* subsystem cvars

Requirements:
  - A full game base directory (e.g. base or baseq3) with stock pk3s and VMs.
  - Engine with Vulkan (for volumetric/PBR demos) and USE_DUKTAPE for JS hooks.

See examples/demo_game/README.md in the engine repository.
