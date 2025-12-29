idTech3 Demo Content
====================

This is minimal demo content for testing the idTech3 engine.

Files:
- demo.map: Source map file
- maps/demo.bsp: Compiled map (placeholder - needs q3map2)
- scripts/demo.shader: Material definitions
- textures/demo/: Basic textures
- demo.cfg: Demo configuration

To compile the map:
1. Install GtkRadiant or q3map2
2. Run: q3map2 -fs_basepath . -game demo_content demo.map

To run the demo:
1. Place demo_content directory in your idtech3 base path
2. Launch engine with: +set fs_game demo_content +map demo

Note: This is very basic content for testing engine functionality.
For full game content, obtain official Quake 3 assets.
