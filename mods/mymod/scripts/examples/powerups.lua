-- Enhanced Power-ups System
-- Demonstrates Lua-powered gameplay mechanics

print("=== ENHANCED POWER-UPS SYSTEM LOADED ===")

-- =============================================================================
-- POWER-UP DEFINITIONS
-- =============================================================================

local POWERUPS = {
    -- Speed Boost with visual effects
    SPEED = {
        name = "Speed Boost",
        duration = 30,  -- seconds
        multiplier = 2.0,
        color = {1.0, 0.8, 0.0},  -- Golden effect
        effects = {"particles", "trail", "sound"}
    },

    -- Shield with energy field
    SHIELD = {
        name = "Energy Shield",
        duration = 20,
        absorption = 0.8,  -- 80% damage reduction
        color = {0.2, 0.8, 1.0},  -- Blue energy
        effects = {"shield_field", "particles", "glow"}
    },

    -- Invisibility with cloaking effect
    INVISIBILITY = {
        name = "Cloaking Device",
        duration = 15,
        transparency = 0.1,  -- 10% visible
        color = {0.5, 0.5, 1.0},  -- Purple distortion
        effects = {"distortion", "particles", "sound"}
    },

    -- Quad Damage with enhanced effects
    QUAD_DAMAGE = {
        name = "Quad Damage",
        duration = 25,
        multiplier = 4.0,
        color = {1.0, 0.0, 1.0},  -- Magenta glow
        effects = {"glow", "particles", "screen_effect", "sound"}
    },

    -- Regeneration with healing over time
    REGENERATION = {
        name = "Regeneration",
        duration = 40,
        heal_rate = 5,  -- HP per second
        color = {0.0, 1.0, 0.0},  -- Green healing
        effects = {"particles", "glow", "sound"}
    },

    -- Teleportation ability
    TELEPORT = {
        name = "Teleport",
        duration = 10,
        range = 1000,  -- units
        color = {0.8, 0.0, 1.0},  -- Purple teleport
        effects = {"teleport_flash", "particles", "sound"}
    }
}

-- =============================================================================
-- ACTIVE POWER-UPS TRACKING
-- =============================================================================

local active_powerups = {}  -- player_id -> {powerup_type, end_time, effects}

-- =============================================================================
-- POWER-UP SYSTEM FUNCTIONS
-- =============================================================================

function Powerups_Grant(player_id, powerup_type)
    local powerup = POWERUPS[powerup_type]
    if not powerup then
        print("ERROR: Unknown powerup type: " .. tostring(powerup_type))
        return false
    end

    local end_time = os.time() + powerup.duration
    active_powerups[player_id] = {
        type = powerup_type,
        end_time = end_time,
        data = powerup
    }

    -- Apply immediate effects
    Powerups_ApplyEffects(player_id, powerup, true)

    -- Emit event for other systems
    Events.emit("powerup_granted", {
        player_id = player_id,
        powerup_type = powerup_type,
        duration = powerup.duration
    })

    print(string.format("Player %d granted %s for %d seconds",
        player_id, powerup.name, powerup.duration))

    return true
end

function Powerups_Remove(player_id, powerup_type)
    local active = active_powerups[player_id]
    if active and active.type == powerup_type then
        -- Remove effects
        Powerups_ApplyEffects(player_id, active.data, false)

        active_powerups[player_id] = nil

        Events.emit("powerup_expired", {
            player_id = player_id,
            powerup_type = powerup_type
        })

        print(string.format("Player %d powerup %s expired",
            player_id, POWERUPS[powerup_type].name))

        return true
    end
    return false
end

function Powerups_HasActive(player_id, powerup_type)
    local active = active_powerups[player_id]
    return active and active.type == powerup_type and active.end_time > os.time()
end

function Powerups_GetActive(player_id)
    local active = active_powerups[player_id]
    if active and active.end_time > os.time() then
        return active.type, active.end_time - os.time()
    end
    return nil, 0
end

function Powerups_GetAllActive(player_id)
    local result = {}
    for p_id, data in pairs(active_powerups) do
        if p_id == player_id and data.end_time > os.time() then
            table.insert(result, {
                type = data.type,
                time_left = data.end_time - os.time(),
                name = data.data.name
            })
        end
    end
    return result
end

-- =============================================================================
-- EFFECT APPLICATION SYSTEM
-- =============================================================================

function Powerups_ApplyEffects(player_id, powerup, enable)
    local effects = powerup.effects
    local color = powerup.color

    for _, effect in ipairs(effects) do
        if effect == "particles" then
            Powerups_ApplyParticleEffect(player_id, powerup, enable)
        elseif effect == "trail" then
            Powerups_ApplyTrailEffect(player_id, powerup, enable)
        elseif effect == "glow" then
            Powerups_ApplyGlowEffect(player_id, powerup, enable)
        elseif effect == "shield_field" then
            Powerups_ApplyShieldEffect(player_id, powerup, enable)
        elseif effect == "distortion" then
            Powerups_ApplyDistortionEffect(player_id, powerup, enable)
        elseif effect == "screen_effect" then
            Powerups_ApplyScreenEffect(player_id, powerup, enable)
        elseif effect == "sound" then
            Powerups_ApplySoundEffect(player_id, powerup, enable)
        elseif effect == "teleport_flash" then
            Powerups_ApplyTeleportEffect(player_id, powerup, enable)
        end
    end
end

-- Individual effect implementations
function Powerups_ApplyParticleEffect(player_id, powerup, enable)
    if enable then
        -- Start particle system around player
        if Effects then
            Effects.spawn_particles("powerup_" .. powerup.name:lower():gsub(" ", "_"),
                player_id, powerup.duration * 10)  -- particles per second
        end
        print(string.format("Applied particle effect for %s to player %d",
            powerup.name, player_id))
    else
        -- Stop particle system
        if Effects then
            Effects.stop_particles(player_id)
        end
    end
end

function Powerups_ApplyTrailEffect(player_id, powerup, enable)
    if enable then
        -- Add motion trail
        if Effects then
            Effects.add_trail(player_id, powerup.color, 0.5)  -- trail duration
        end
    else
        if Effects then
            Effects.remove_trail(player_id)
        end
    end
end

function Powerups_ApplyGlowEffect(player_id, powerup, enable)
    if enable then
        -- Add player glow
        if Effects then
            Effects.add_glow(player_id, powerup.color, 1.5)  -- glow intensity
        end
    else
        if Effects then
            Effects.remove_glow(player_id)
        end
    end
end

function Powerups_ApplyShieldEffect(player_id, powerup, enable)
    if enable then
        -- Create energy shield field
        if Effects then
            Effects.create_shield(player_id, powerup.color, powerup.absorption)
        end
    else
        if Effects then
            Effects.remove_shield(player_id)
        end
    end
end

function Powerups_ApplyDistortionEffect(player_id, powerup, enable)
    if enable then
        -- Apply cloaking distortion
        if Effects then
            Effects.add_distortion(player_id, powerup.transparency)
        end
    else
        if Effects then
            Effects.remove_distortion(player_id)
        end
    end
end

function Powerups_ApplyScreenEffect(player_id, powerup, enable)
    if enable then
        -- Apply screen space effect (for local player only)
        if Effects and player_id == Game.get_local_player_id() then
            Effects.add_screen_effect(powerup.color, 0.3)  -- effect intensity
        end
    else
        if Effects and player_id == Game.get_local_player_id() then
            Effects.remove_screen_effect()
        end
    end
end

function Powerups_ApplySoundEffect(player_id, powerup, enable)
    if enable then
        -- Play powerup activation sound
        if Audio then
            Audio.play_sound("powerup_activate", player_id)
            Audio.play_looping_sound("powerup_loop_" .. powerup.name:lower():gsub(" ", "_"), player_id)
        end
    else
        if Audio then
            Audio.stop_looping_sound(player_id)
            Audio.play_sound("powerup_deactivate", player_id)
        end
    end
end

function Powerups_ApplyTeleportEffect(player_id, powerup, enable)
    if enable then
        -- Show teleport ready indicator
        if UI then
            UI.show_indicator(player_id, "Teleport Ready", powerup.color, powerup.duration)
        end
    else
        if UI then
            UI.hide_indicator(player_id)
        end
    end
end

-- =============================================================================
-- DAMAGE AND GAMEPLAY MODIFICATION
-- =============================================================================

function Powerups_ModifyDamage(player_id, damage, damage_type)
    local active = active_powerups[player_id]
    if active and active.end_time > os.time() then
        local powerup = active.data

        if active.type == "SHIELD" then
            -- Reduce damage with shield
            local reduced_damage = damage * (1.0 - powerup.absorption)
            print(string.format("Shield absorbed %.0f%% of %d damage for player %d",
                powerup.absorption * 100, damage, player_id))
            return reduced_damage
        elseif active.type == "INVULNERABILITY" then
            -- Complete protection
            return 0
        end
    end
    return damage
end

function Powerups_ModifySpeed(player_id, base_speed)
    local active = active_powerups[player_id]
    if active and active.end_time > os.time() then
        local powerup = active.data

        if active.type == "SPEED" then
            return base_speed * powerup.multiplier
        end
    end
    return base_speed
end

function Powerups_ModifyDamageDealt(player_id, damage)
    local active = active_powerups[player_id]
    if active and active.end_time > os.time() then
        local powerup = active.data

        if active.type == "QUAD_DAMAGE" then
            return damage * powerup.multiplier
        elseif active.type == "BERSERK" then
            return damage * 2.0
        end
    end
    return damage
end

-- =============================================================================
-- REGENERATION SYSTEM
-- =============================================================================

function Powerups_ProcessRegeneration()
    local current_time = os.time()

    for player_id, data in pairs(active_powerups) do
        if data.end_time > current_time then
            local powerup = data.data

            if data.type == "REGENERATION" then
                -- Apply healing over time
                if Game then
                    local current_health = Game.get_player_health(player_id)
                    local max_health = Game.get_player_max_health(player_id)

                    if current_health < max_health then
                        local heal_amount = math.min(powerup.heal_rate, max_health - current_health)
                        Game.modify_player_health(player_id, heal_amount)

                        -- Visual healing effect
                        if Effects then
                            Effects.spawn_particles("heal", player_id, 5)
                        end
                    end
                end
            end
        else
            -- Powerup expired
            Powerups_Remove(player_id, data.type)
        end
    end
end

-- =============================================================================
-- TELEPORTATION SYSTEM
-- =============================================================================

function Powerups_ExecuteTeleport(player_id)
    local active = active_powerups[player_id]
    if active and active.type == "TELEPORT" and active.end_time > os.time() then
        -- Find safe teleport location
        local current_pos = Game.get_player_position(player_id)
        local teleport_pos = Powerups_FindTeleportLocation(current_pos, active.data.range)

        if teleport_pos then
            -- Execute teleport
            Game.set_player_position(player_id, teleport_pos)

            -- Effects
            if Effects then
                Effects.teleport_flash(current_pos)
                Effects.teleport_flash(teleport_pos)
            end

            if Audio then
                Audio.play_sound("teleport", player_id)
            end

            -- Remove teleport powerup after use
            Powerups_Remove(player_id, "TELEPORT")

            Events.emit("player_teleported", {
                player_id = player_id,
                from = current_pos,
                to = teleport_pos
            })

            return true
        end
    end
    return false
end

function Powerups_FindTeleportLocation(from_pos, max_range)
    -- Simple teleport location finding (would use more sophisticated logic in real implementation)
    local angles = {0, 90, 180, 270}
    local distance = max_range * 0.5  -- Start with half range

    for _, angle in ipairs(angles) do
        local rad_angle = math.rad(angle)
        local test_pos = {
            x = from_pos.x + math.cos(rad_angle) * distance,
            y = from_pos.y + math.sin(rad_angle) * distance,
            z = from_pos.z
        }

        if Game.is_valid_position(test_pos) then
            return test_pos
        end
    end

    return nil  -- No safe location found
end

-- =============================================================================
-- EVENT HANDLERS
-- =============================================================================

-- Set up event listeners
Events.on("player_spawned", function(data)
    -- Give random powerup for testing (remove in production)
    if math.random() < 0.1 then  -- 10% chance
        local powerups = {"SPEED", "SHIELD", "QUAD_DAMAGE", "REGENERATION"}
        local random_powerup = powerups[math.random(#powerups)]
        Powerups_Grant(data.player_id, random_powerup)
    end
end)

Events.on("player_damaged", function(data)
    local modified_damage = Powerups_ModifyDamage(data.player_id, data.damage, data.damage_type)
    data.damage = modified_damage  -- Modify the damage taken
end)

Events.on("player_attack", function(data)
    local modified_damage = Powerups_ModifyDamageDealt(data.player_id, data.damage)
    data.damage = modified_damage  -- Modify the damage dealt
end)

-- =============================================================================
-- UPDATE LOOP
-- =============================================================================

local last_update = 0
function Powerups_Update()
    local current_time = os.time()

    -- Process regeneration every second
    if current_time - last_update >= 1 then
        Powerups_ProcessRegeneration()
        last_update = current_time
    end
end

-- Register update function to be called every frame
if Game then
    Game.register_update_callback(Powerups_Update)
end

-- =============================================================================
-- DEBUG AND TESTING FUNCTIONS
-- =============================================================================

function Powerups_Debug_ShowActive()
    print("=== ACTIVE POWERUPS ===")
    for player_id, data in pairs(active_powerups) do
        if data.end_time > os.time() then
            local time_left = data.end_time - os.time()
            print(string.format("Player %d: %s (%d seconds left)",
                player_id, data.data.name, time_left))
        end
    end
    print("======================")
end

function Powerups_Test_All()
    print("Testing all powerup systems...")

    -- Test granting powerups
    for i, powerup_type in ipairs({"SPEED", "SHIELD", "QUAD_DAMAGE", "REGENERATION"}) do
        Powerups_Grant(i, powerup_type)
    end

    -- Show active powerups
    Powerups_Debug_ShowActive()

    print("Powerup system test complete!")
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Core functions
    grant = Powerups_Grant,
    remove = Powerups_Remove,
    has_active = Powerups_HasActive,
    get_active = Powerups_GetActive,
    get_all_active = Powerups_GetAllActive,

    -- Damage modification
    modify_damage = Powerups_ModifyDamage,
    modify_speed = Powerups_ModifySpeed,
    modify_damage_dealt = Powerups_ModifyDamageDealt,

    -- Special abilities
    teleport = Powerups_ExecuteTeleport,

    -- Debug and testing
    debug_show_active = Powerups_Debug_ShowActive,
    test_all = Powerups_Test_All,

    -- Update function (called by game loop)
    update = Powerups_Update
}
