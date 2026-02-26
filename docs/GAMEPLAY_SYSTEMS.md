# Gameplay Systems

All gameplay systems are initialized by `CL_InitGameSystems()` in `cl_gameframe.c` and ticked every frame by `CL_GameFrame(frametime)`.

## AI Director (`g_director.h/c`)

Adaptive pacing engine with per-player intensity tracking.

**Phases:** BUILDUP → SUSTAIN → PEAK → RESPITE → RELAX (automatic transitions)

**Key functions:**
- `Director_UpdatePlayer(clientNum, pos, health, ammo, alive)` -- feed player state
- `Director_GetGlobalIntensity()` -- returns 0.0-1.0
- `Director_GetPhase()` -- current pacing phase
- `Director_AddSpawnType(name, maxActive, cooldown, minIntensity, maxIntensity, weight)` -- register enemy type
- `Director_PickSpawnType()` -- weighted random selection respecting budgets
- `Director_AddZone(name, mins, maxs, threat, budgetMult)` -- map zones with threat levels
- `Director_TriggerWave(intensityBoost)` -- force a wave

**Cvars:** 14 configurable parameters via `dirConfig_t`.

## GOAP (`g_goap.h/c`)

Goal-Oriented Action Planning for AI decision-making. A* search over action space.

**Setup:**
```c
int hunt = GOAP_RegisterAction("Hunt", 3.0);
GOAP_SetActionPrecondition(hunt, PROP_HAS_TARGET, 1);
GOAP_SetActionEffect(hunt, PROP_NEAR_TARGET, 1);

int goal = GOAP_RegisterGoal("Kill", 10.0);
GOAP_SetGoalState(goal, PROP_TARGET_DEAD, 1);

goapAgentHandle_t agent = GOAP_CreateAgent();
GOAP_AddAgentAction(agent, hunt);
GOAP_AutoPlan(agent);  // A* finds cheapest action sequence
```

## Horde AI (`g_horde.h/c`)

512-agent swarm with 4-tier LOD: FULL (every frame) → MEDIUM (100ms) → LOW (500ms) → DORMANT (no updates).

**Features:** Boids flocking (cohesion/separation/alignment), group management, state machine (idle/wander/chase/attack/flee/dead).

## Response Rules (`g_response.h/c`)

Context-aware NPC dialogue. 14 criteria types (health, ammo, combat, isolation, Director intensity/phase, zone, random chance). Weighted random response selection with per-rule cooldowns.

## Choreography (`g_choreography.h/c`)

Timeline-based scripted scenes. 32 scenes, 64 events, 8 actors. Event types: speak, animate, camera cut/lerp, sound, effect, move-to, look-at, wait, callback.

## Facial Animation (`g_facial.h/c`)

33 flex controllers, 25 phonemes, 11 expression presets. Keyframed lip sync, auto-blink, expression blending.

## Dismemberment (`g_dismember.h/c`)

16 body parts with parent-child hierarchy, 8 wound types, per-limb health, physics-driven gibs (128 pool), 5 gore levels.

## Background Map (`cl_map_background.h/c`)

Non-playable 3D maps behind menus. 64-point spline camera with hermite smoothing, per-map atmosphere (fog, ambient, time of day, music). Cvars: `cl_bgmap`, `cl_bgmap_speed`, `cl_bgmap_fov`.

## Dynamic Window Title (`cl_window_title.h/c`)

Format string with tokens: `{game} {map} {score} {player} {event} {fps} {custom}`. Timed events auto-clear. Cvars: `cl_dynamicTitle`, `cl_titleFormat`, `cl_titleUpdateRate`.
