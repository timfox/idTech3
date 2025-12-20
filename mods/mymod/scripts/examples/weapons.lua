-- Enhanced Weapons System
-- Demonstrates advanced weapon mechanics with Lua scripting

print("=== ENHANCED WEAPONS SYSTEM LOADED ===")

-- =============================================================================
-- WEAPON DEFINITIONS
-- =============================================================================

local WEAPONS = {
    -- Plasma Rifle with charge mechanics
    PLASMA_RIFLE = {
        name = "Plasma Rifle",
        damage = 35,
        fire_rate = 8,  -- shots per second
        ammo_type = "plasma",
        max_ammo = 50,
        features = {"charge_shot", "overcharge", "cooling_system"}
    },

    -- Smart Rocket Launcher with homing
    SMART_ROCKET = {
        name = "Smart Rocket Launcher",
        damage = 100,
        fire_rate = 1.5,
        ammo_type = "rockets",
        max_ammo = 10,
        features = {"homing", "proximity_detonation", "target_lock"}
    },

    -- Nano Blade with energy mechanics
    NANO_BLADE = {
        name = "Nano Blade",
        damage = 80,
        fire_rate = 2.0,
        ammo_type = "energy",
        max_ammo = 100,
        features = {"energy_regen", "combo_system", "shield_penetration"}
    },

    -- Frost Cannon with freeze effects
    FROST_CANNON = {
        name = "Frost Cannon",
        damage = 25,
        fire_rate = 12,
        ammo_type = "cryo",
        max_ammo = 75,
        features = {"freeze_effect", "slow_motion", "ice_shield"}
    },

    -- Arc Thrower with chain lightning
    ARC_THROWER = {
        name = "Arc Thrower",
        damage = 45,
        fire_rate = 6,
        ammo_type = "energy",
        max_ammo = 60,
        features = {"chain_lightning", "emp_effect", "energy_shield"}
    }
}

-- =============================================================================
-- WEAPON STATE TRACKING
-- =============================================================================

local weapon_states = {}  -- player_id -> weapon data
local active_effects = {} -- effect tracking

-- =============================================================================
-- WEAPON SYSTEM FUNCTIONS
-- =============================================================================

function Weapons_Equip(player_id, weapon_type)
    local weapon = WEAPONS[weapon_type]
    if not weapon then
        print("ERROR: Unknown weapon type: " .. tostring(weapon_type))
        return false
    end

    weapon_states[player_id] = {
        type = weapon_type,
        data = weapon,
        ammo = weapon.max_ammo,
        charge_level = 0,
        last_fire = 0,
        effects = {}
    }

    -- Initialize weapon features
    Weapons_InitializeFeatures(player_id, weapon)

    Events.emit("weapon_equipped", {
        player_id = player_id,
        weapon_type = weapon_type,
        ammo = weapon.max_ammo
    })

    print(string.format("Player %d equipped %s (%d ammo)",
        player_id, weapon.name, weapon.max_ammo))

    return true
end

function Weapons_Fire(player_id)
    local state = weapon_states[player_id]
    if not state then return false end

    local weapon = state.data
    local current_time = os.time()

    -- Check fire rate
    if current_time - state.last_fire < 1.0 / weapon.fire_rate then
        return false
    end

    -- Check ammo
    if state.ammo <= 0 then
        Weapons_PlaySound(player_id, "no_ammo")
        return false
    end

    -- Execute weapon-specific firing logic
    local success = Weapons_ExecuteFire(player_id, state)

    if success then
        state.last_fire = current_time
        state.ammo = state.ammo - 1

        Events.emit("weapon_fired", {
            player_id = player_id,
            weapon_type = state.type,
            ammo_left = state.ammo
        })
    end

    return success
end

function Weapons_Reload(player_id)
    local state = weapon_states[player_id]
    if not state then return false end

    local weapon = state.data
    local reload_amount = weapon.max_ammo - state.ammo

    if reload_amount > 0 then
        state.ammo = weapon.max_ammo

        Weapons_PlaySound(player_id, "reload")

        Events.emit("weapon_reloaded", {
            player_id = player_id,
            weapon_type = state.type,
            ammo = state.ammo
        })

        return true
    end

    return false
end

-- =============================================================================
-- WEAPON-SPECIFIC FIRE LOGIC
-- =============================================================================

function Weapons_ExecuteFire(player_id, state)
    local weapon_type = state.type

    if weapon_type == "PLASMA_RIFLE" then
        return Weapons_FirePlasmaRifle(player_id, state)
    elseif weapon_type == "SMART_ROCKET" then
        return Weapons_FireSmartRocket(player_id, state)
    elseif weapon_type == "NANO_BLADE" then
        return Weapons_FireNanoBlade(player_id, state)
    elseif weapon_type == "FROST_CANNON" then
        return Weapons_FireFrostCannon(player_id, state)
    elseif weapon_type == "ARC_THROWER" then
        return Weapons_FireArcThrower(player_id, state)
    end

    return false
end

function Weapons_FirePlasmaRifle(player_id, state)
    -- Calculate damage based on charge level
    local base_damage = state.data.damage
    local charge_multiplier = 1.0 + (state.charge_level * 0.5)  -- Up to 50% bonus
    local damage = base_damage * charge_multiplier

    -- Fire plasma bolt
    local hit_pos = Weapons_CalculateTrajectory(player_id)
    if hit_pos then
        Weapons_ApplyDamage(hit_pos, damage, "plasma")

        -- Visual effects
        Weapons_SpawnEffect("plasma_bolt", player_id, hit_pos)
        Weapons_SpawnEffect("plasma_impact", hit_pos)

        -- Reset charge after firing
        state.charge_level = 0
    end

    Weapons_PlaySound(player_id, "plasma_fire")
    return true
end

function Weapons_FireSmartRocket(player_id, state)
    -- Launch homing rocket
    local rocket_id = Weapons_SpawnProjectile("smart_rocket", player_id)

    if rocket_id then
        -- Find nearest enemy for homing
        local target = Weapons_FindNearestEnemy(player_id)
        if target then
            Weapons_SetProjectileTarget(rocket_id, target)
        end

        Weapons_PlaySound(player_id, "rocket_launch")
        return true
    end

    return false
end

function Weapons_FireNanoBlade(player_id, state)
    -- Melee energy blade attack
    local range = 100
    local damage = state.data.damage

    -- Check for combo system
    local combo_multiplier = Weapons_CalculateCombo(player_id)
    damage = damage * combo_multiplier

    -- Find targets in range
    local targets = Weapons_FindTargetsInRange(player_id, range)

    for _, target in ipairs(targets) do
        Weapons_ApplyDamage(target, damage, "energy")
        Weapons_SpawnEffect("nano_slash", target)

        -- Shield penetration effect
        if Weapons_HasShield(target) then
            Weapons_DisableShield(target, 2.0)  -- Disable for 2 seconds
        end
    end

    Weapons_PlaySound(player_id, "nano_slash")
    return #targets > 0
end

function Weapons_FireFrostCannon(player_id, state)
    -- Fire freezing projectile
    local hit_pos = Weapons_CalculateTrajectory(player_id)

    if hit_pos then
        Weapons_ApplyDamage(hit_pos, state.data.damage, "cryo")

        -- Apply freeze effect
        Weapons_ApplyFreezeEffect(hit_pos, 3.0)  -- Freeze for 3 seconds

        -- Visual effects
        Weapons_SpawnEffect("frost_beam", player_id, hit_pos)
        Weapons_SpawnEffect("ice_explosion", hit_pos)

        Weapons_PlaySound(player_id, "frost_fire")
        return true
    end

    return false
end

function Weapons_FireArcThrower(player_id, state)
    -- Chain lightning attack
    local primary_target = Weapons_CalculateTrajectory(player_id)

    if primary_target then
        Weapons_ApplyDamage(primary_target, state.data.damage, "energy")

        -- Chain to nearby enemies
        local chained_targets = Weapons_FindChainTargets(primary_target, 3, 200)  -- Max 3 chains, 200 unit range

        for i, target in ipairs(chained_targets) do
            local chain_damage = state.data.damage * (0.7 ^ i)  -- Damage falls off with distance
            Weapons_ApplyDamage(target, chain_damage, "energy")

            -- Visual chain effect
            Weapons_SpawnEffect("lightning_chain", primary_target, target)
        end

        -- EMP effect on primary target
        Weapons_ApplyEMPEffect(primary_target, 1.5)

        Weapons_SpawnEffect("arc_burst", player_id, primary_target)
        Weapons_PlaySound(player_id, "arc_fire")

        return true
    end

    return false
end

-- =============================================================================
-- WEAPON FEATURES AND SYSTEMS
-- =============================================================================

function Weapons_InitializeFeatures(player_id, weapon)
    for _, feature in ipairs(weapon.features) do
        if feature == "charge_shot" then
            Weapons_EnableChargeSystem(player_id)
        elseif feature == "energy_regen" then
            Weapons_EnableEnergyRegen(player_id)
        elseif feature == "combo_system" then
            Weapons_EnableComboSystem(player_id)
        end
    end
end

function Weapons_EnableChargeSystem(player_id)
    -- Allow charging shots for bonus damage
    print("Charge system enabled for player " .. player_id)
end

function Weapons_EnableEnergyRegen(player_id)
    -- Regenerate ammo over time
    weapon_states[player_id].energy_regen = true
    print("Energy regeneration enabled for player " .. player_id)
end

function Weapons_EnableComboSystem(player_id)
    weapon_states[player_id].combo_count = 0
    weapon_states[player_id].combo_timer = 0
    print("Combo system enabled for player " .. player_id)
end

-- =============================================================================
-- UTILITY FUNCTIONS
-- =============================================================================

function Weapons_CalculateTrajectory(player_id)
    -- Simplified trajectory calculation
    -- In real implementation, this would use raycasting
    return Game.get_player_aim_target(player_id)
end

function Weapons_CalculateCombo(player_id)
    local state = weapon_states[player_id]
    if not state then return 1.0 end

    local current_time = os.time()
    if current_time - state.combo_timer > 2.0 then
        -- Combo reset
        state.combo_count = 1
    else
        state.combo_count = math.min(state.combo_count + 1, 5)  -- Max 5x combo
    end

    state.combo_timer = current_time
    return 1.0 + (state.combo_count * 0.2)  -- 20% bonus per combo level
end

function Weapons_FindNearestEnemy(player_id)
    -- Simplified enemy finding
    return Game.find_nearest_enemy(player_id)
end

function Weapons_FindTargetsInRange(player_id, range)
    -- Find all targets within range
    return Game.find_targets_in_range(player_id, range)
end

function Weapons_FindChainTargets(start_pos, max_count, range)
    -- Find targets for chain lightning
    local targets = {}
    local enemies = Game.find_enemies_in_range(start_pos, range)

    for i = 1, math.min(max_count, #enemies) do
        table.insert(targets, enemies[i])
    end

    return targets
end

function Weapons_HasShield(target)
    return Game.entity_has_shield(target)
end

function Weapons_DisableShield(target, duration)
    Game.disable_entity_shield(target, duration)
end

-- =============================================================================
-- EFFECT APPLICATION
-- =============================================================================

function Weapons_ApplyDamage(target, damage, damage_type)
    Game.apply_damage(target, damage, damage_type)
end

function Weapons_ApplyFreezeEffect(target, duration)
    Game.apply_status_effect(target, "frozen", duration)
end

function Weapons_ApplyEMPEffect(target, duration)
    Game.apply_status_effect(target, "emp", duration)
end

function Weapons_SpawnEffect(effect_type, pos1, pos2)
    if Effects then
        Effects.spawn_effect(effect_type, pos1, pos2)
    end
end

function Weapons_SpawnProjectile(projectile_type, player_id)
    if Projectiles then
        return Projectiles.spawn(projectile_type, player_id)
    end
    return nil
end

function Weapons_SetProjectileTarget(projectile_id, target)
    if Projectiles then
        Projectiles.set_target(projectile_id, target)
    end
end

function Weapons_PlaySound(player_id, sound_type)
    if Audio then
        Audio.play_weapon_sound(sound_type, player_id)
    end
end

-- =============================================================================
-- UPDATE SYSTEMS
-- =============================================================================

function Weapons_Update()
    local current_time = os.time()

    -- Update charge systems
    for player_id, state in pairs(weapon_states) do
        if state.charge_level and state.charge_level < 1.0 then
            state.charge_level = math.min(1.0, state.charge_level + 0.02)  -- Charge over time
        end

        -- Energy regeneration
        if state.energy_regen and state.ammo < state.data.max_ammo then
            state.ammo = math.min(state.data.max_ammo, state.ammo + 1)  -- 1 ammo per second
        end
    end

    -- Update active effects
    for effect_id, effect in pairs(active_effects) do
        if current_time > effect.end_time then
            Weapons_RemoveEffect(effect_id)
        end
    end
end

function Weapons_RemoveEffect(effect_id)
    active_effects[effect_id] = nil
    if Effects then
        Effects.remove_effect(effect_id)
    end
end

-- =============================================================================
-- EVENT HANDLERS
-- =============================================================================

Events.on("player_spawned", function(data)
    -- Give default weapon
    Weapons_Equip(data.player_id, "PLASMA_RIFLE")
end)

Events.on("player_died", function(data)
    -- Reset weapon state
    weapon_states[data.player_id] = nil
end)

-- Register update function
if Game then
    Game.register_update_callback(Weapons_Update)
end

-- =============================================================================
-- DEBUG AND TESTING
-- =============================================================================

function Weapons_Debug_ShowState()
    print("=== WEAPON STATES ===")
    for player_id, state in pairs(weapon_states) do
        print(string.format("Player %d: %s (%d/%d ammo)",
            player_id, state.data.name, state.ammo, state.data.max_ammo))

        if state.charge_level then
            print(string.format("  Charge: %.1f%%", state.charge_level * 100))
        end

        if state.combo_count then
            print(string.format("  Combo: %dx", state.combo_count))
        end
    end
    print("===================")
end

function Weapons_Test_All()
    print("Testing weapon systems...")

    -- Test weapon equipping
    for i = 1, 3 do
        local weapons = {"PLASMA_RIFLE", "SMART_ROCKET", "FROST_CANNON"}
        local weapon = weapons[math.random(#weapons)]
        Weapons_Equip(i, weapon)
    end

    -- Show states
    Weapons_Debug_ShowState()

    print("Weapon system test complete!")
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Core functions
    equip = Weapons_Equip,
    fire = Weapons_Fire,
    reload = Weapons_Reload,

    -- Utility functions
    calculate_trajectory = Weapons_CalculateTrajectory,
    find_nearest_enemy = Weapons_FindNearestEnemy,

    -- Debug and testing
    debug_show_state = Weapons_Debug_ShowState,
    test_all = Weapons_Test_All,

    -- Update function
    update = Weapons_Update
}
