idTech3 Box3D example scenes
============================

These configs mirror the Box3D sample-app families using engine-native
commands. They are intentionally small "example games": load a map from your
base data, then exec a scene and interact with it through the console.

Quick start:

  exec box3d_examples/menu.cfg
  exec box3d_examples/stacking_jenga.cfg
  exec box3d_examples/joints_bridge.cfg
  exec box3d_examples/events_sensors.cfg
  exec box3d_examples/continuous_bullets.cfg
  exec box3d_examples/compound_village.cfg
  exec box3d_examples/character_mover.cfg
  exec box3d_examples/softbody_fluid.cfg
  exec box3d_examples/replay_determinism.cfg

The examples use the shared physics middleware, not the upstream sample app
renderer. Keep phys_debugDraw enabled when learning; turn it off for normal
playtests.
