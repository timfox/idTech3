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

## Engine.Physics (6 functions)

```lua
Engine.Physics.init()                         -- returns boolean
Engine.Physics.step(dt)
Engine.Physics.createBody(x, y, z, mass, size) -- returns bodyHandle
Engine.Physics.destroyBody(handle)
Engine.Physics.applyImpulse(handle, ix, iy, iz, px, py, pz)
Engine.Physics.getTransform(handle)           -- returns x, y, z, rx, ry, rz
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

## Engine.Face (6 functions)

```lua
Engine.Face.create(entityNum)                 -- returns faceHandle
Engine.Face.destroy(handle)
Engine.Face.setExpression(handle, exprId, weight, blendTime)
Engine.Face.setFlex(handle, flexId, value)
Engine.Face.setPhoneme(handle, phonemeId, weight)
Engine.Face.setBlinkRate(handle, blinksPerMinute)
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
