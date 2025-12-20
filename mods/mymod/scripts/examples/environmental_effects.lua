-- Environmental Effects System
-- Demonstrates dynamic world interaction and atmospheric effects

print("=== ENVIRONMENTAL EFFECTS SYSTEM LOADED ===")

-- =============================================================================
-- ENVIRONMENTAL ZONES
-- =============================================================================

local ENVIRONMENTAL_ZONES = {
    -- Toxic waste zones
    TOXIC_WASTE = {
        name = "Toxic Waste",
        damage_per_second = 10,
        effect_type = "poison",
        visual_effect = "green_haze",
        sound_effect = "toxic_hum",
        particle_effect = "toxic_bubbles"
    },

    -- Radiation zones
    RADIATION = {
        name = "Radiation Zone",
        damage_per_second = 15,
        effect_type = "radiation",
        visual_effect = "radiation_glow",
        sound_effect = "geiger_counter",
        particle_effect = "radiation_particles"
    },

    -- Lava pits
    LAVA = {
        name = "Lava Pit",
        damage_per_second = 50,
        effect_type = "fire",
        visual_effect = "lava_glow",
        sound_effect = "lava_bubble",
        particle_effect = "lava_sparks"
    },

    -- Zero gravity zones
    ZERO_GRAVITY = {
        name = "Zero Gravity",
        gravity_multiplier = 0.1,
        effect_type = "physics",
        visual_effect = "gravity_warp",
        sound_effect = "gravity_hum",
        particle_effect = "gravity_particles"
    },

    -- High gravity zones
    HIGH_GRAVITY = {
        name = "High Gravity",
        gravity_multiplier = 2.5,
        effect_type = "physics",
        visual_effect = "gravity_crush",
        sound_effect = "gravity_strain",
        particle_effect = "gravity_waves"
    },

    -- Healing zones
    HEALING_SPRING = {
        name = "Healing Spring",
        heal_per_second = 20,
        effect_type = "healing",
        visual_effect = "healing_aura",
        sound_effect = "healing_chime",
        particle_effect = "healing_sparks"
    }
}

-- =============================================================================
-- DYNAMIC WEATHER SYSTEM
-- =============================================================================

local WEATHER_TYPES = {
    CLEAR = {
        name = "Clear",
        visibility = 1.0,
        effects = {}
    },

    RAIN = {
        name = "Rain",
        visibility = 0.7,
        effects = {"rain_particles", "puddles", "wet_surfaces"}
    },

    SNOW = {
        name = "Snow",
        visibility = 0.6,
        effects = {"snow_particles", "snow_accumulation", "ice_surfaces"}
    },

    FOG = {
        name = "Fog",
        visibility = 0.4,
        effects = {"fog_haze", "reduced_visibility", "muffled_sounds"}
    },

    STORM = {
        name = "Storm",
        visibility = 0.5,
        effects = {"lightning", "thunder", "heavy_rain", "wind"}
    },

    SANDSTORM = {
        name = "Sandstorm",
        visibility = 0.3,
        effects = {"sand_particles", "reduced_visibility", "wind"}
    }
}

-- =============================================================================
-- ACTIVE ENVIRONMENTAL EFFECTS
-- =============================================================================

local active_zones = {}      -- zone_id -> zone data
local active_weather = nil   -- current weather state
local player_effects = {}    -- player_id -> active effects

-- =============================================================================
-- ZONE MANAGEMENT
-- =============================================================================

function Environmental_CreateZone(zone_type, position, radius, duration)
    local zone = ENVIRONMENTAL_ZONES[zone_type]
    if not zone then
        print("ERROR: Unknown zone type: " .. tostring(zone_type))
        return nil
    end

    local zone_id = "zone_" .. os.time() .. "_" .. math.random(1000)

    active_zones[zone_id] = {
        type = zone_type,
        data = zone,
        position = position,
        radius = radius,
        end_time = duration and (os.time() + duration) or nil,
        created_time = os.time()
    }

    -- Activate visual effects
    Environmental_ActivateZoneEffects(zone_id)

    Events.emit("zone_created", {
        zone_id = zone_id,
        type = zone_type,
        position = position,
        radius = radius
    })

    print(string.format("Created %s zone at (%.1f, %.1f, %.1f) radius %.1f",
        zone.name, position.x, position.y, position.z, radius))

    return zone_id
end

function Environmental_RemoveZone(zone_id)
    local zone = active_zones[zone_id]
    if zone then
        -- Deactivate effects
        Environmental_DeactivateZoneEffects(zone_id)

        active_zones[zone_id] = nil

        Events.emit("zone_removed", {
            zone_id = zone_id,
            type = zone.type
        })

        print("Removed zone: " .. zone_id)
        return true
    end
    return false
end

function Environmental_GetZonesInRange(position, range)
    local zones_in_range = {}

    for zone_id, zone in pairs(active_zones) do
        local distance = Environmental_CalculateDistance(position, zone.position)
        if distance <= range then
            table.insert(zones_in_range, {
                id = zone_id,
                data = zone,
                distance = distance
            })
        end
    end

    return zones_in_range
end

-- =============================================================================
-- WEATHER MANAGEMENT
-- =============================================================================

function Environmental_SetWeather(weather_type, duration)
    local weather = WEATHER_TYPES[weather_type]
    if not weather then
        print("ERROR: Unknown weather type: " .. tostring(weather_type))
        return false
    end

    -- Clear existing weather
    if active_weather then
        Environmental_StopWeatherEffects()
    end

    active_weather = {
        type = weather_type,
        data = weather,
        end_time = duration and (os.time() + duration) or nil
    }

    -- Start weather effects
    Environmental_StartWeatherEffects(weather_type)

    Events.emit("weather_changed", {
        weather_type = weather_type,
        visibility = weather.visibility,
        duration = duration
    })

    print("Weather changed to: " .. weather.name)
    return true
end

function Environmental_StopWeather()
    if active_weather then
        Environmental_StopWeatherEffects()
        active_weather = nil

        Events.emit("weather_cleared")
        print("Weather cleared")
    end
end

function Environmental_GetCurrentWeather()
    return active_weather
end

-- =============================================================================
-- PLAYER ENVIRONMENT INTERACTION
-- =============================================================================

function Environmental_UpdatePlayerEffects(player_id)
    local player_pos = Game.get_player_position(player_id)
    local zones_affecting = Environmental_GetZonesInRange(player_pos, 50)  -- Check within 50 units

    -- Clear previous effects for this player
    Environmental_ClearPlayerEffects(player_id)

    -- Apply zone effects
    for _, zone_info in ipairs(zones_affecting) do
        Environmental_ApplyZoneEffect(player_id, zone_info)
    end

    -- Apply weather effects
    if active_weather then
        Environmental_ApplyWeatherEffect(player_id)
    end
end

function Environmental_ApplyZoneEffect(player_id, zone_info)
    local zone = zone_info.data
    local zone_data = zone.data
    local effect_key = string.format("%s_%s", zone.type, player_id)

    -- Apply zone-specific effects
    if zone_data.effect_type == "damage" or zone_data.damage_per_second then
        Environmental_ApplyDamageEffect(player_id, zone_data, effect_key)
    elseif zone_data.effect_type == "healing" then
        Environmental_ApplyHealingEffect(player_id, zone_data, effect_key)
    elseif zone_data.effect_type == "physics" then
        Environmental_ApplyPhysicsEffect(player_id, zone_data, effect_key)
    elseif zone_data.effect_type == "poison" then
        Environmental_ApplyPoisonEffect(player_id, zone_data, effect_key)
    elseif zone_data.effect_type == "radiation" then
        Environmental_ApplyRadiationEffect(player_id, zone_data, effect_key)
    elseif zone_data.effect_type == "fire" then
        Environmental_ApplyFireEffect(player_id, zone_data, effect_key)
    end

    -- Store active effect
    player_effects[effect_key] = {
        type = "zone",
        zone_type = zone.type,
        player_id = player_id,
        start_time = os.time()
    }
end

function Environmental_ApplyWeatherEffect(player_id)
    local weather = active_weather.data
    local effect_key = string.format("weather_%s", player_id)

    -- Apply weather-based effects (reduced visibility, etc.)
    if Effects then
        Effects.set_visibility_modifier(weather.visibility)
    end

    -- Apply weather-specific effects
    for _, effect in ipairs(weather.effects) do
        if effect == "lightning" then
            Environmental_SpawnLightning()
        elseif effect == "wind" then
            Environmental_ApplyWindEffect(player_id)
        end
    end

    player_effects[effect_key] = {
        type = "weather",
        weather_type = active_weather.type,
        player_id = player_id,
        start_time = os.time()
    }
end

-- =============================================================================
-- SPECIFIC EFFECT IMPLEMENTATIONS
-- =============================================================================

function Environmental_ApplyDamageEffect(player_id, zone_data, effect_key)
    -- Apply damage over time
    if not player_effects[effect_key] then
        player_effects[effect_key] = { damage_timer = 0 }
    end

    local effect_data = player_effects[effect_key]
    effect_data.damage_timer = effect_data.damage_timer + 1

    if effect_data.damage_timer >= 60 then  -- Every second
        Game.apply_damage(player_id, zone_data.damage_per_second, zone_data.effect_type)
        effect_data.damage_timer = 0

        -- Visual feedback
        if Effects then
            Effects.spawn_particles(zone_data.particle_effect, player_id, 10)
        end
    end
end

function Environmental_ApplyHealingEffect(player_id, zone_data, effect_key)
    -- Apply healing over time
    if not player_effects[effect_key] then
        player_effects[effect_key] = { heal_timer = 0 }
    end

    local effect_data = player_effects[effect_key]
    effect_data.heal_timer = effect_data.heal_timer + 1

    if effect_data.heal_timer >= 60 then  -- Every second
        local current_health = Game.get_player_health(player_id)
        local max_health = Game.get_player_max_health(player_id)

        if current_health < max_health then
            local heal_amount = math.min(zone_data.heal_per_second, max_health - current_health)
            Game.modify_player_health(player_id, heal_amount)

            -- Visual feedback
            if Effects then
                Effects.spawn_particles(zone_data.particle_effect, player_id, 5)
            end
        end

        effect_data.heal_timer = 0
    end
end

function Environmental_ApplyPhysicsEffect(player_id, zone_data, effect_key)
    -- Modify player physics
    if zone_data.gravity_multiplier then
        Game.set_player_gravity(player_id, zone_data.gravity_multiplier)
    end
end

function Environmental_ApplyPoisonEffect(player_id, zone_data, effect_key)
    -- Apply poison status effect
    Game.apply_status_effect(player_id, "poisoned", 5.0)  -- 5 second duration
end

function Environmental_ApplyRadiationEffect(player_id, zone_data, effect_key)
    -- Apply radiation sickness
    Game.apply_status_effect(player_id, "irradiated", 10.0)
end

function Environmental_ApplyFireEffect(player_id, zone_data, effect_key)
    -- Apply burning damage
    Game.apply_status_effect(player_id, "burning", 3.0)
end

function Environmental_ApplyWindEffect(player_id)
    -- Apply wind force to player
    local wind_force = {x = math.random(-50, 50), y = math.random(-50, 50), z = 0}
    Game.apply_force(player_id, wind_force)
end

function Environmental_SpawnLightning()
    -- Random lightning strikes
    if math.random() < 0.05 then  -- 5% chance per update
        local strike_pos = {
            x = math.random(-1000, 1000),
            y = math.random(-1000, 1000),
            z = 1000
        }

        if Effects then
            Effects.spawn_lightning(strike_pos)
        end

        if Audio then
            Audio.play_sound("thunder", nil, 0.8)
        end
    end
end

-- =============================================================================
-- VISUAL EFFECTS MANAGEMENT
-- =============================================================================

function Environmental_ActivateZoneEffects(zone_id)
    local zone = active_zones[zone_id]
    if not zone then return end

    local zone_data = zone.data

    -- Visual effects
    if Effects then
        Effects.create_zone_effect(zone_id, zone_data.visual_effect, zone.position, zone.radius)
    end

    -- Sound effects
    if Audio then
        Audio.play_looping_sound(zone_data.sound_effect, zone.position)
    end

    -- Particle effects
    if Effects then
        Effects.spawn_zone_particles(zone_id, zone_data.particle_effect, zone.position, zone.radius)
    end
end

function Environmental_DeactivateZoneEffects(zone_id)
    if Effects then
        Effects.remove_zone_effect(zone_id)
    end

    if Audio then
        Audio.stop_looping_sound(zone_id)
    end
end

function Environmental_StartWeatherEffects(weather_type)
    local weather = WEATHER_TYPES[weather_type]

    if Effects then
        for _, effect in ipairs(weather.effects) do
            Effects.start_weather_effect(effect)
        end
    end

    if Audio then
        Audio.start_weather_audio(weather_type)
    end
end

function Environmental_StopWeatherEffects()
    if Effects then
        Effects.stop_all_weather_effects()
    end

    if Audio then
        Audio.stop_weather_audio()
    end
end

-- =============================================================================
-- UTILITY FUNCTIONS
-- =============================================================================

function Environmental_CalculateDistance(pos1, pos2)
    local dx = pos1.x - pos2.x
    local dy = pos1.y - pos2.y
    local dz = pos1.z - pos2.z
    return math.sqrt(dx*dx + dy*dy + dz*dz)
end

function Environmental_ClearPlayerEffects(player_id)
    -- Remove all effects for this player
    for effect_key, effect_data in pairs(player_effects) do
        if effect_data.player_id == player_id then
            Environmental_RemovePlayerEffect(effect_key)
        end
    end
end

function Environmental_RemovePlayerEffect(effect_key)
    local effect_data = player_effects[effect_key]
    if effect_data then
        -- Clean up specific effects
        if effect_data.type == "zone" then
            local zone_data = ENVIRONMENTAL_ZONES[effect_data.zone_type]
            if zone_data.effect_type == "physics" then
                Game.reset_player_gravity(effect_data.player_id)
            end
        elseif effect_data.type == "weather" then
            if Effects then
                Effects.reset_visibility_modifier()
            end
        end

        player_effects[effect_key] = nil
    end
end

-- =============================================================================
-- UPDATE LOOP
-- =============================================================================

function Environmental_Update()
    local current_time = os.time()

    -- Update all players
    if Game then
        local player_count = Game.get_player_count()
        for i = 1, player_count do
            Environmental_UpdatePlayerEffects(i)
        end
    end

    -- Clean up expired zones
    for zone_id, zone in pairs(active_zones) do
        if zone.end_time and current_time > zone.end_time then
            Environmental_RemoveZone(zone_id)
        end
    end

    -- Clean up expired weather
    if active_weather and active_weather.end_time and current_time > active_weather.end_time then
        Environmental_StopWeather()
    end
end

-- Register update callback
if Game then
    Game.register_update_callback(Environmental_Update)
end

-- =============================================================================
-- EVENT HANDLERS
-- =============================================================================

Events.on("round_start", function(data)
    -- Create some random environmental zones
    Environmental_CreateRandomZones(5)  -- Create 5 random zones

    -- Set random weather
    local weather_types = {"CLEAR", "RAIN", "FOG", "STORM"}
    local random_weather = weather_types[math.random(#weather_types)]
    Environmental_SetWeather(random_weather, 300)  -- 5 minutes
end)

Events.on("round_end", function(data)
    -- Clean up all environmental effects
    for zone_id, _ in pairs(active_zones) do
        Environmental_RemoveZone(zone_id)
    end
    Environmental_StopWeather()
end)

-- =============================================================================
-- RANDOM ZONE GENERATION
-- =============================================================================

function Environmental_CreateRandomZones(count)
    local zone_types = {"TOXIC_WASTE", "RADIATION", "LAVA", "ZERO_GRAVITY", "HIGH_GRAVITY", "HEALING_SPRING"}

    for i = 1, count do
        local zone_type = zone_types[math.random(#zone_types)]
        local position = {
            x = math.random(-500, 500),
            y = math.random(-500, 500),
            z = 0
        }
        local radius = math.random(50, 150)
        local duration = math.random(60, 300)  -- 1-5 minutes

        Environmental_CreateZone(zone_type, position, radius, duration)
    end
end

-- =============================================================================
-- DEBUG AND TESTING
-- =============================================================================

function Environmental_Debug_ShowStatus()
    print("=== ENVIRONMENTAL STATUS ===")

    print("Active Zones:")
    for zone_id, zone in pairs(active_zones) do
        print(string.format("  %s: %s at (%.1f, %.1f, %.1f) radius %.1f",
            zone_id, zone.data.name, zone.position.x, zone.position.y, zone.position.z, zone.radius))
    end

    if active_weather then
        print("Current Weather: " .. active_weather.data.name)
    else
        print("Weather: Clear")
    end

    print("Active Player Effects:")
    for effect_key, effect in pairs(player_effects) do
        print(string.format("  %s: %s for player %d",
            effect_key, effect.type, effect.player_id))
    end

    print("============================")
end

function Environmental_Test_All()
    print("Testing environmental systems...")

    -- Create test zones
    local test_zones = {"TOXIC_WASTE", "HEALING_SPRING", "ZERO_GRAVITY"}
    for _, zone_type in ipairs(test_zones) do
        local pos = {x = math.random(-200, 200), y = math.random(-200, 200), z = 0}
        Environmental_CreateZone(zone_type, pos, 100, 30)
    end

    -- Test weather
    Environmental_SetWeather("RAIN", 60)

    -- Show status
    Environmental_Debug_ShowStatus()

    print("Environmental system test complete!")
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Zone management
    create_zone = Environmental_CreateZone,
    remove_zone = Environmental_RemoveZone,
    get_zones_in_range = Environmental_GetZonesInRange,

    -- Weather management
    set_weather = Environmental_SetWeather,
    stop_weather = Environmental_StopWeather,
    get_current_weather = Environmental_GetCurrentWeather,

    -- Player effects
    update_player_effects = Environmental_UpdatePlayerEffects,
    clear_player_effects = Environmental_ClearPlayerEffects,

    -- Random generation
    create_random_zones = Environmental_CreateRandomZones,

    -- Debug and testing
    debug_show_status = Environmental_Debug_ShowStatus,
    test_all = Environmental_Test_All,

    -- Update function
    update = Environmental_Update
}
