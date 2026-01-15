# ECS and Single Player Integration

## Overview

This document describes the Entity Component System (ECS) and Single Player enhancements added to idTech3, inspired by EntityPlus mod and using the entt library.

## ECS System

### Files Created
- `src/game/g_ecs.h` - ECS header with component definitions
- `src/game/g_ecs.cpp` - ECS core implementation
- `src/game/g_ecs_integration.cpp` - Integration with legacy gentity_t system

### Core Components

1. **TransformComponent** - Position, rotation, scale
2. **HealthComponent** - Health, armor, invulnerability
3. **PhysicsComponent** - Velocity, acceleration, mass, friction
4. **RenderComponent** - Model, shader, color, visibility
5. **AIComponent** - Behavior, reaction time, awareness, targeting
6. **WeaponComponent** - Weapon type, ammo, fire rate
7. **InventoryComponent** - Item storage
8. **TriggerComponent** - Interactive objects

### Single Player Components

1. **SPEnemyComponent** - Enemy spawning and respawning
2. **SPObjectiveComponent** - Mission objectives
3. **SPCheckpointComponent** - Save points
4. **SPDialogComponent** - NPC conversations

### Features

- Modern C++23 ECS architecture using entt
- Component-based entity management
- System updates (Physics, AI, Weapons, Triggers)
- Integration bridge with legacy gentity_t system
- Automatic component synchronization

## Single Player System

### Files Created
- `src/game/g_singleplayer.h` - Single player API
- `src/game/g_singleplayer.c` - Single player implementation

### Features

1. **Game Modes**
   - Arena - Combat-focused
   - Story - Campaign mode
   - Survival - Endless waves
   - Tutorial - Learning mode

2. **Objectives System**
   - Kill objectives
   - Collect objectives
   - Reach location objectives
   - Protect objectives
   - Destroy objectives
   - Interact objectives
   - Timed objectives

3. **Checkpoint System**
   - Save/load player state
   - Position and angle storage
   - Health/armor/ammo persistence

4. **Enemy Spawning**
   - Configurable spawners
   - Respawnable enemies
   - Wave-based spawning
   - Difficulty scaling

5. **Statistics Tracking**
   - Kills/deaths
   - Secrets found
   - Level completion time
   - Objective completion

## Integration

### Build System

The ECS system is integrated via CMake:
- `USE_ENTT` option enables/disables ECS
- Uses entt single-header include for simplicity
- C++ files compiled with C++23 standard

### Usage

```c
// Initialize systems
G_ECS_Init();
G_SP_Init();

// Create entity with components
entt::entity enemy = G_ECS_CreateEntity();
auto& registry = G_ECS_GetRegistry();
registry.emplace<TransformComponent>(enemy);
registry.emplace<HealthComponent>(enemy);
registry.emplace<AIComponent>(enemy);
registry.emplace<SPEnemyComponent>(enemy);

// Create objective
sp_objective_t* obj = G_SP_CreateObjective("kill_boss", SP_OBJECTIVE_KILL);
obj->targetValue = 1;
obj->status = SP_OBJECTIVE_ACTIVE;

// Update systems
G_ECS_Update(deltaTime);
G_SP_Update();
```

## Future Enhancements

- Full bidirectional mapping between gentity_t and ECS entities
- PBR material components
- Animation components
- Sound/audio components
- Network replication components
- Serialization/deserialization for save games
