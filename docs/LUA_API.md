# Lua Scripting API

All engine systems are exposed to Lua via the `Engine` global table. Register with `LuaBindings_RegisterAll(luaState)`.

## Engine.Director (16 functions)

```lua
Engine.Director.init()
Engine.Director.update(dt)
Engine.Director.getPhase()                    -- returns phase enum
Engine.Director.forcePhase(phase)
Engine.Director.getIntensity()                -- returns 0.0-1.0
Engine.Director.getPlayerIntensity(clientNum)
Engine.Director.getPlayerStress(clientNum)
Engine.Director.triggerWave(intensityBoost)
Engine.Director.updatePlayer(cn, x, y, z, health, ammo, alive)
Engine.Director.playerKill(clientNum)
Engine.Director.playerDeath(clientNum)
Engine.Director.playerDamage(clientNum, amount)
Engine.Director.addSpawnType(name, maxActive, cooldown, minIntensity, maxIntensity, weight)
Engine.Director.shouldSpawn(typeId)           -- returns boolean
Engine.Director.pickSpawnType()               -- returns typeId or -1
Engine.Director.addZone(name, mnX, mnY, mnZ, mxX, mxY, mxZ, threat, budgetMult)
```

## Engine.Nav (4 functions)

```lua
Engine.Nav.init()
Engine.Nav.buildFromBSP(mapName)              -- returns meshHandle
Engine.Nav.findPath(mesh, sX, sY, sZ, eX, eY, eZ) -- returns {{x,y,z},...} or nil
Engine.Nav.addAgent(x, y, z)                  -- returns agentHandle
```

## Engine.Physics

Box3D Soft Step rigid bodies, companion solvers, ragdoll bind, and the script-facing event poll. See [PHYSICS.md](PHYSICS.md) for architecture, cvars, and console commands.

### Core / queries

```lua
Engine.Physics.init()                         -- returns boolean
Engine.Physics.step(dt)
Engine.Physics.backend()                      -- "box3d" | "bullet" | "none"
Engine.Physics.createBody(x, y, z, mass, size) -- returns bodyHandle
Engine.Physics.destroyBody(handle)
Engine.Physics.applyImpulse(handle, ix, iy, iz, px, py, pz)
Engine.Physics.applyImpulseRadius(x, y, z, radius, impulse)
Engine.Physics.getTransform(handle)           -- x, y, z, rx, ry, rz
Engine.Physics.setTransform(handle, x, y, z, rx, ry, rz)
Engine.Physics.setFriction(handle, friction)
Engine.Physics.setRestitution(handle, restitution)
Engine.Physics.setFilter(handle, cat, mask [, group])
Engine.Physics.attachShape(handle [, hx, hy, hz])
Engine.Physics.rayCast(x1,y1,z1, x2,y2,z2 [, cat, mask])  -- hit, fraction, body, px, py, pz
Engine.Physics.convexSweep(x1,y1,z1, x2,y2,z2 [, radius])
Engine.Physics.overlapSphere(x,y,z [, radius]) -- body handle table
Engine.Physics.getContacts(handle)            -- manifold table
Engine.Physics.validateReplay(path)           -- Soft Step recording QA
Engine.Physics.pmoveCorrect(pmove)            -- CastMover bridge when phys_pmove 1
Engine.Physics.moverStep(...)                 -- character CastMover step
```

### Shapes, sensors, constraints

```lua
Engine.Physics.createBox(x, y, z [, hx, hy, hz, mass])
Engine.Physics.createSphere(x, y, z [, radius, mass])
Engine.Physics.createStatic(x, y, z [, hx, hy, hz])
Engine.Physics.createSensor(x, y, z [, hx, hy, hz])
Engine.Physics.createShadow(x, y, z [, hx, hy, hz])   -- kinematic shadow body
Engine.Physics.setShadowPose(handle, x, y, z, rx, ry, rz)
Engine.Physics.createBuoyancy(x, y, z, radius, density)
Engine.Physics.addHeightField(x, y, z, w, d, heightsTable)
Engine.Physics.createConstraint(type, a, b, ...) -- filter|parallel|cone|prismatic|wheel|…
Engine.Physics.setConstraintSpring(c, enable [, hz, damp])
Engine.Physics.setSphericalLimits(c, cone [, twistLo, twistHi])
Engine.Physics.setWheelSteering(c, angleRad [, maxTorque])
```

### Event bus

Poll once per frame (or tick) from Lua. `subscribe(type)` is a no-op placeholder — events are retrieved via `pollEvent()`.

```lua
local ev = Engine.Physics.pollEvent()
-- ev == nil when queue empty
-- ev.type: 0=IMPACT, 5=BALANCE_LOST, 6=RAGDOLL_SLEEP, 9=MOTION_ENTER, 10=MOTION_EXIT,
--          11=CONTACT_BEGIN, 12=CONTACT_END, 13=BODY_SLEEP (see phys_events.h)
-- ev.bodyA, ev.bodyB, ev.ragdoll, ev.bone, ev.x, ev.y, ev.z, ev.magnitude
```

### Ragdoll (Euphoria-like)

```lua
-- Death pose (ProcAnim_Kill):
local rag, anim, motor = Engine.Physics.createRagdoll([ragPath], x, y, z)
-- Live Euphoria (balance + motors):
local rag, anim, motor = Engine.Physics.spawnBoundAlive([ragPath], x, y, z)
local ok, numBones = Engine.Physics.loadRagdoll("models/foo.rag")
Engine.Physics.setBoneAnimTarget(rag, bone, px, py, pz [, rx, ry, rz])
Engine.Physics.forceAnimState(anim, state)    -- procAnimState_t enum
Engine.Physics.hitRagdoll(anim [, motor, bone, ix, iy, iz, px, py, pz])
```

### DMM-like destructibles

```lua
local dmm = Engine.Physics.createDmm(x, y, z [, edge, density, gridRes])
local fragCount = Engine.Physics.fractureDmm(dmm [, px, py, pz, impulse])
local active = Engine.Physics.dmmStatus()
```

## Engine.Particles (5 functions)

```lua
Engine.Particles.init()
Engine.Particles.clear()
Engine.Particles.emitSmoke(x, y, z, lifetime, startSize, endSize, alpha)
Engine.Particles.emitSparks(x, y, z, count, speed, lifetime)
Engine.Particles.count()                      -- returns active count
```

## Engine.Music (5 functions)

```lua
Engine.Music.init()
Engine.Music.addLayer(track, type, intensityMin, intensityMax, fadeSpeed)
Engine.Music.setIntensity(intensity)
Engine.Music.addStinger(track, trigger, cooldown, oneShot)
Engine.Music.fadeToSilence(fadeTime)
```

## Engine.Face (11 functions)

```lua
Engine.Face.create(entityNum)                 -- returns faceHandle
Engine.Face.destroy(handle)
Engine.Face.setExpression(handle, exprId, weight, blendTime)
Engine.Face.setFlex(handle, flexId, value)
Engine.Face.setPhoneme(handle, phonemeId, weight)
Engine.Face.setBlinkRate(handle, blinksPerMinute)
Engine.Face.setAU(handle, au|name, intensity) -- FACS Action Unit (see docs/FACS.md)
Engine.Face.setAUSide(handle, au|name, side, intensity)
Engine.Face.getAU(handle, au|name)            -- returns intensity
Engine.Face.clearAUs(handle)
Engine.Face.auName(auIndex)                   -- returns "AU12", etc.
-- Constants: Engine.Face.AU1 .. AU43, SIDE_BOTH, SIDE_LEFT, SIDE_RIGHT
```

## Engine.Horde (7 functions)

```lua
Engine.Horde.init()
Engine.Horde.spawn(x, y, z, health, speed, groupId) -- returns agentHandle
Engine.Horde.kill(handle)
Engine.Horde.setTarget(handle, x, y, z, entityNum)
Engine.Horde.getState(handle)                 -- returns state enum
Engine.Horde.getCount()                       -- returns active count
Engine.Horde.createGroup(x, y, z, radius)     -- returns groupId
```

## Engine.Dismember (5 functions)

```lua
Engine.Dismember.create(entityNum)            -- returns dismemberHandle
Engine.Dismember.damage(handle, limb, damage, woundType)
Engine.Dismember.sever(handle, limb, forceX, forceY, forceZ) -- returns boolean
Engine.Dismember.explode(handle, x, y, z, force, radius)
Engine.Dismember.isAttached(handle, limb)     -- returns boolean
```

## Engine.Choreo (6 functions)

```lua
Engine.Choreo.create(sceneName)               -- returns sceneHandle
Engine.Choreo.addActor(scene, entityNum, name) -- returns actorIndex
Engine.Choreo.addEvent(scene, type, startTime, duration, actor, param)
Engine.Choreo.play(scene)
Engine.Choreo.stop(scene)
Engine.Choreo.isPlaying(scene)                -- returns boolean
```

## Engine.Response (4 functions)

```lua
Engine.Response.addRule(name, concept)        -- returns ruleId
Engine.Response.addCriteria(ruleId, type, value, strValue)
Engine.Response.addResponse(ruleId, soundFile, animation, delay, weight)
Engine.Response.trigger(concept)
```

## Constants

```lua
-- Director phases
DIR_PHASE_BUILDUP = 0
DIR_PHASE_SUSTAIN = 1
DIR_PHASE_PEAK = 2
DIR_PHASE_RESPITE = 3
DIR_PHASE_RELAX = 4

-- Horde states
HORDE_STATE_IDLE = 0
HORDE_STATE_CHASE = 2
HORDE_STATE_ATTACK = 3
HORDE_STATE_DEAD = 5

-- Limbs
LIMB_HEAD = 0
LIMB_ARM_UPPER_L = 4
LIMB_LEG_UPPER_L = 10

-- Wound types
WOUND_GUNSHOT = 4
WOUND_EXPLOSION = 7

-- Expressions
EXPR_HAPPY = 1
EXPR_PAIN = 7
EXPR_DEAD = 8

-- Choreography event types
CHOREO_EVT_SPEAK = 0
CHOREO_EVT_ANIMATE = 1
CHOREO_EVT_CAMERA_CUT = 4
CHOREO_EVT_SOUND = 7
```

## Engine.Save (disk v1)

Persists under SQLite when available (`save/engine_profile.db`, table `save_slots`) and still writes the legacy JSON fallback `save/engine_slot_<N>.json` with `protocolVersion`, `modVersion`, `label`, and `checksum`.

```lua
Engine.Save.write(slot, "checkpoint_name")   -- returns boolean
local label = Engine.Save.read(slot)         -- returns string or nil
local last = Engine.Save.lastSlot()          -- returns integer
```

Protocol version: `1` (see [g_engine_systems.h](../runtime/game/g_engine_systems.h)). Legacy `save/engine_slot_*.txt` still reads.

Console (client, no Lua): `engine_save_write <slot> <label>`, `engine_save_read <slot>`, `engine_save_info`.

## Engine.DB

SQLite-backed engine profile/gameplay data service.

```lua
local ok = Engine.DB.available()
local path = Engine.DB.path()
Engine.DB.exec("CREATE TABLE IF NOT EXISTS notes(id INTEGER PRIMARY KEY, body TEXT)")
local first = Engine.DB.queryOne("SELECT body FROM notes ORDER BY id LIMIT 1")
Engine.DB.profileSet("player_name", "Ranger")
local name = Engine.DB.profileGet("player_name")
Engine.DB.profileDelete("player_name")
```

## Engine.Cvars

```lua
local name = Engine.Cvars.getString("name")
local speed = Engine.Cvars.getNumber("g_speed")
local maxClients = Engine.Cvars.getInteger("sv_maxclients")
local enabled = Engine.Cvars.getBoolean("net_p2p")
local exists = Engine.Cvars.exists("sv_hostname")
local flags = Engine.Cvars.flags("sv_hostname")
Engine.Cvars.set("sv_hostname", "Arena")
Engine.Cvars.setNumber("timescale", 1.0)
Engine.Cvars.setInteger("sv_maxclients", 16)
Engine.Cvars.setBoolean("g_allowVote", true)
Engine.Cvars.reset("sv_hostname")
```

## Engine.Telemetry

```lua
Engine.Telemetry.record("metric_name", 1.0)
local v = Engine.Telemetry.get("metric_name")
Engine.Telemetry.clear()
```

Cvar: `engine_telemetry` (default `1`).

## Engine.Replay

```lua
local frame = Engine.Replay.frameIndex()
local base = Engine.Replay.baseTime()
```

Cvar: `engine_replay` (default `1`). Frame index advances when client snapshots are valid.

## Engine.Quest

```lua
Engine.Quest.add("quest_id", "Title", "active")
Engine.Quest.setStage("quest_id", "complete")
local stage = Engine.Quest.getStage("quest_id")
local n = Engine.Quest.count()
```

## Engine.Dialogue

```lua
Engine.Dialogue.start("Speaker", "Line of dialogue")
Engine.Dialogue.clear()
local n = Engine.Dialogue.count()
```

## Engine.FogBiology

Coastal fog bioaerosol ecology (Evans et al. 2019). See [FOG_BIOLOGY.md](FOG_BIOLOGY.md).

```lua
Engine.FogBiology.enabled()                    -- boolean (r_fogBiology)
Engine.FogBiology.getPhase()                   -- "clear" | "fog" | "post_fog"
Engine.FogBiology.getMarineInfluence()         -- 0..1
Engine.FogBiology.getCoastKm()                 -- current coast distance km
Engine.FogBiology.getPathogenRisk()            -- 0..1 deposition/pathogen heuristic
Engine.FogBiology.getCommunity([phase])       -- { shannon, marine, oceanOtu, deposition, richness, gramNegative, rhodospirillales, pathogenTaxa, phyla={...} }
Engine.FogBiology.setSite("maine"|"namib")     -- or 0/1
Engine.FogBiology.setCoastKm(km)
Engine.FogBiology.setMarineWind(0..1)
Engine.FogBiology.setFogActive(bool)
Engine.FogBiology.poll()                     -- { phase, marine, coastKm, shannon, deposition, pathogen }
```
