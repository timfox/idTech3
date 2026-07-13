# Box3D Example Scenes

The upstream Box3D sample application is a standalone renderer/UI testbed. These
idTech3 examples mirror its sample families through engine-native configs,
console commands, debug draw, and the demo mod package.

Run a playable map from your own base data, then:

```cfg
exec box3d_examples/menu.cfg
```

## Scene Map

| idTech3 scene | Upstream sample family mirrored | Engine systems exercised |
|---------------|----------------------------------|--------------------------|
| `box3d_examples/stacking_jenga.cfg` | Stacking, Box Stack, Sphere Stack, Dominoes, Jenga | dynamic boxes/spheres, sleep, impulses |
| `box3d_examples/joints_bridge.cfg` | Joints, Distance Joint, Prismatic, Bridge, Ball and Chain | distance joints, prismatic motor, rope/bridge layouts |
| `box3d_examples/events_sensors.cfg` | Events, Sensor Visit, Hit, Move, Persistent Contact | sensors, hit threshold, contact/event bus, buoyancy volume |
| `box3d_examples/continuous_bullets.cfg` | Continuous, Thin Wall, Bullet vs Stack, Mesh Drop | CCD, heightfield, projectile-style impulse stress |
| `box3d_examples/compound_village.cfg` | Compound, Simple, Tile Floor, Mesh Tile, Village | static compound hull clusters, terrain, falling props |
| `box3d_examples/character_mover.cfg` | Character, Mover, body cast, kinematic control | shadow body, `phys_pmove`, step height, sensor interactions |
| `box3d_examples/softbody_fluid.cfg` | Soft-body companion examples around Box3D collision | cloth, rope, particles, soft blob, SPH fluid, solvers |
| `box3d_examples/replay_determinism.cfg` | Replay, Determinism, Falling Ragdolls | recording, replay, ragdoll pile, deterministic stress |

## Notes

- These are not ports of the upstream sample app renderer. They are game-engine
  scenes built from `phys_*` commands so they run in the idTech3 client.
- The repo does not ship commercial game maps. Load any compatible map from your
  base data first, then execute a scene.
- Use `phys_debug` or `set phys_debugDraw 1` while learning a scene. Turn debug
  draw off for performance captures or normal playtests.
- `phys_clear_props` resets the current scene props without changing maps.
