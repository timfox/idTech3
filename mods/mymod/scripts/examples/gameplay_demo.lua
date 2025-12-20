-- Enhanced Gameplay Demonstration
-- Shows all advanced features working together

print("=== ENHANCED GAMEPLAY DEMO LOADED ===")

-- Load all enhancement modules
local Powerups = require("examples/powerups")
local Weapons = require("examples/weapons")
local Environmental = require("examples/environmental_effects")

-- =============================================================================
-- INTEGRATED GAMEPLAY SYSTEMS
-- =============================================================================

local gameplay_state = {
    round_active = false,
    round_start_time = 0,
    players = {},
    zones_created = 0,
    powerups_spawned = 0
}

-- =============================================================================
-- ROUND MANAGEMENT
-- =============================================================================

function Gameplay_StartRound()
    print("=== STARTING ENHANCED GAMEPLAY ROUND ===")

    gameplay_state.round_active = true
    gameplay_state.round_start_time = os.time()
    gameplay_state.players = {}
    gameplay_state.zones_created = 0
    gameplay_state.powerups_spawned = 0

    -- Initialize environmental effects
    Environmental.create_random_zones(8)  -- 8 random zones
    gameplay_state.zones_created = 8

    -- Set dynamic weather
    local weather_options = {"RAIN", "FOG", "STORM", "CLEAR"}
    local random_weather = weather_options[math.random(#weather_options)]
    Environmental.set_weather(random_weather, 600)  -- 10 minutes

    -- Spawn initial powerups
    Gameplay_SpawnInitialPowerups()

    Events.emit("round_started", {
        round_id = os.time(),
        zones_count = gameplay_state.zones_created,
        weather = random_weather
    })

    print("Round started with:")
    print("  - " .. gameplay_state.zones_created .. " environmental zones")
    print("  - Weather: " .. random_weather)
    print("  - Initial powerups spawned")
end

function Gameplay_EndRound()
    print("=== ENDING GAMEPLAY ROUND ===")

    gameplay_state.round_active = false

    -- Clean up all systems
    Environmental.stop_weather()

    -- Remove all zones
    for zone_id, _ in pairs(Environmental.get_zones_in_range({x=0,y=0,z=0}, 99999)) do
        Environmental.remove_zone(zone_id)
    end

    -- Clear all powerups
    for player_id = 1, Game.get_player_count() do
        local active_powerups = Powerups.get_all_active(player_id)
        for _, powerup in ipairs(active_powerups) do
            Powerups.remove(player_id, powerup.type)
        end
    end

    Events.emit("round_ended", {
        duration = os.time() - gameplay_state.round_start_time,
        zones_created = gameplay_state.zones_created,
        powerups_spawned = gameplay_state.powerups_spawned
    })

    print("Round cleanup complete")
end

-- =============================================================================
-- POWERUP SPAWNING SYSTEM
-- =============================================================================

function Gameplay_SpawnInitialPowerups()
    local spawn_locations = {
        {x = -200, y = -200, z = 50},
        {x = 200, y = -200, z = 50},
        {x = -200, y = 200, z = 50},
        {x = 200, y = 200, z = 50},
        {x = 0, y = 0, z = 100}
    }

    local powerup_types = {"SPEED", "SHIELD", "QUAD_DAMAGE", "REGENERATION", "TELEPORT"}

    for i, location in ipairs(spawn_locations) do
        -- Spawn powerup at location (simplified - would use actual entity spawning)
        local powerup_type = powerup_types[math.random(#powerup_types)]

        -- In a real implementation, this would spawn a pickup entity
        print(string.format("Spawned %s powerup at (%.0f, %.0f, %.0f)",
            powerup_type, location.x, location.y, location.z))

        gameplay_state.powerups_spawned = gameplay_state.powerups_spawned + 1
    end
end

function Gameplay_SpawnDynamicPowerup()
    -- Spawn powerups during gameplay
    if gameplay_state.round_active and math.random() < 0.1 then  -- 10% chance
        local powerup_types = {"SPEED", "SHIELD", "QUAD_DAMAGE"}
        local powerup_type = powerup_types[math.random(#powerup_types)]

        -- Find a safe spawn location (simplified)
        local spawn_pos = {
            x = math.random(-400, 400),
            y = math.random(-400, 400),
            z = 50
        }

        print("Dynamic powerup spawned: " .. powerup_type)
        gameplay_state.powerups_spawned = gameplay_state.powerups_spawned + 1
    end
end

-- =============================================================================
-- PLAYER MANAGEMENT
-- =============================================================================

function Gameplay_PlayerJoined(player_id, player_name)
    gameplay_state.players[player_id] = {
        name = player_name,
        join_time = os.time(),
        score = 0,
        powerups_collected = 0,
        damage_dealt = 0,
        zones_visited = {}
    }

    -- Give starting equipment
    Weapons.equip(player_id, "PLASMA_RIFLE")

    print("Player " .. player_name .. " (ID: " .. player_id .. ") joined the game")
end

function Gameplay_PlayerLeft(player_id)
    local player_data = gameplay_state.players[player_id]
    if player_data then
        print("Player " .. player_data.name .. " left after " ..
              (os.time() - player_data.join_time) .. " seconds")
        gameplay_state.players[player_id] = nil
    end
end

function Gameplay_UpdatePlayerStats(player_id, stat_type, value)
    local player_data = gameplay_state.players[player_id]
    if player_data then
        if stat_type == "score" then
            player_data.score = player_data.score + value
        elseif stat_type == "powerup" then
            player_data.powerups_collected = player_data.powerups_collected + 1
        elseif stat_type == "damage" then
            player_data.damage_dealt = player_data.damage_dealt + value
        end
    end
end

-- =============================================================================
-- GAMEPLAY EVENTS
-- =============================================================================

Events.on("player_spawned", function(data)
    Gameplay_PlayerJoined(data.player_id, data.name or "Player_" .. data.player_id)

    -- Give a random starting powerup for demo purposes
    if math.random() < 0.3 then  -- 30% chance
        local powerup_types = {"SPEED", "SHIELD", "REGENERATION"}
        local powerup_type = powerup_types[math.random(#powerup_types)]
        Powerups.grant(data.player_id, powerup_type)

        Gameplay_UpdatePlayerStats(data.player_id, "powerup", 1)
    end
end)

Events.on("player_died", function(data)
    -- Update stats
    Gameplay_UpdatePlayerStats(data.attacker_id, "score", 10)
    Gameplay_UpdatePlayerStats(data.victim_id, "score", -5)
end)

Events.on("powerup_granted", function(data)
    Gameplay_UpdatePlayerStats(data.player_id, "powerup", 1)
end)

Events.on("weapon_fired", function(data)
    -- Track weapon usage for balancing
    local player_data = gameplay_state.players[data.player_id]
    if player_data then
        player_data.weapon_usage = (player_data.weapon_usage or 0) + 1
    end
end)

Events.on("damage_dealt", function(data)
    Gameplay_UpdatePlayerStats(data.attacker_id, "damage", data.damage)
end)

-- =============================================================================
-- DYNAMIC GAMEPLAY SYSTEMS
-- =============================================================================

function Gameplay_UpdateDynamicSystems()
    if not gameplay_state.round_active then return end

    local current_time = os.time()
    local round_elapsed = current_time - gameplay_state.round_start_time

    -- Spawn dynamic powerups every 30 seconds
    if round_elapsed % 30 == 0 then
        Gameplay_SpawnDynamicPowerup()
    end

    -- Change weather periodically (every 5 minutes)
    if round_elapsed % 300 == 0 and round_elapsed > 0 then
        local weather_options = {"CLEAR", "RAIN", "FOG", "STORM"}
        local new_weather = weather_options[math.random(#weather_options)]
        Environmental.set_weather(new_weather, 300)
        print("Weather changed to: " .. new_weather)
    end

    -- Update zone effects
    for player_id, _ in pairs(gameplay_state.players) do
        Environmental.update_player_effects(player_id)
    end
end

-- =============================================================================
-- STATISTICS AND ANALYTICS
-- =============================================================================

function Gameplay_ShowStatistics()
    print("=== GAMEPLAY STATISTICS ===")

    if gameplay_state.round_active then
        local round_duration = os.time() - gameplay_state.round_start_time
        print("Round Duration: " .. round_duration .. " seconds")
        print("Zones Created: " .. gameplay_state.zones_created)
        print("Powerups Spawned: " .. gameplay_state.powerups_spawned)
    end

    print("Player Statistics:")
    for player_id, data in pairs(gameplay_state.players) do
        print(string.format("  %s (ID:%d): Score=%d, Powerups=%d, Damage=%d",
            data.name, player_id, data.score, data.powerups_collected, data.damage_dealt))
    end

    print("System Status:")
    print("  Powerups: " .. (Powerups and "ACTIVE" or "INACTIVE"))
    print("  Weapons: " .. (Weapons and "ACTIVE" or "INACTIVE"))
    print("  Environmental: " .. (Environmental and "ACTIVE" or "INACTIVE"))

    print("========================")
end

function Gameplay_ShowLeaderboard()
    print("=== LEADERBOARD ===")

    -- Sort players by score
    local sorted_players = {}
    for player_id, data in pairs(gameplay_state.players) do
        table.insert(sorted_players, {id = player_id, data = data})
    end

    table.sort(sorted_players, function(a, b)
        return a.data.score > b.data.score
    end)

    for i, player_info in ipairs(sorted_players) do
        local rank = i == 1 and "🥇" or i == 2 and "🥈" or i == 3 and "🥉" or tostring(i)
        print(string.format("%s %s: %d points",
            rank, player_info.data.name, player_info.data.score))
    end

    print("===================")
end

-- =============================================================================
-- DEMO SCENARIOS
-- =============================================================================

function Gameplay_RunDemoScenario(scenario)
    print("Running demo scenario: " .. scenario)

    if scenario == "powerup_madness" then
        -- Spawn many powerups
        for i = 1, 10 do
            local powerup_types = {"SPEED", "SHIELD", "QUAD_DAMAGE", "REGENERATION", "TELEPORT"}
            local powerup_type = powerup_types[math.random(#powerup_types)]
            -- Grant to random players
            local player_count = 0
            for _ in pairs(gameplay_state.players) do player_count = player_count + 1 end
            if player_count > 0 then
                local random_player = math.random(1, player_count)
                Powerups.grant(random_player, powerup_type)
            end
        end

    elseif scenario == "environmental_hazard" then
        -- Create dangerous zones everywhere
        for i = 1, 15 do
            local hazard_types = {"TOXIC_WASTE", "RADIATION", "LAVA"}
            local hazard_type = hazard_types[math.random(#hazard_types)]
            local pos = {x = math.random(-500, 500), y = math.random(-500, 500), z = 0}
            Environmental.create_zone(hazard_type, pos, math.random(50, 100), 120)
        end

    elseif scenario == "ultimate_showdown" then
        -- Combine all effects
        Environmental.set_weather("STORM", 300)
        Environmental.create_random_zones(12)

        -- Give all players ultimate powerups
        for player_id, _ in pairs(gameplay_state.players) do
            Powerups.grant(player_id, "QUAD_DAMAGE")
            Powerups.grant(player_id, "REGENERATION")
            Weapons.equip(player_id, "ARC_THROWER")
        end

    elseif scenario == "peaceful_mode" then
        -- Create healing zones and calm weather
        Environmental.set_weather("CLEAR", 600)
        for i = 1, 8 do
            local pos = {x = math.random(-300, 300), y = math.random(-300, 300), z = 0}
            Environmental.create_zone("HEALING_SPRING", pos, 75, 300)
        end
    end

    print("Demo scenario '" .. scenario .. "' activated!")
end

-- =============================================================================
-- UPDATE LOOP
-- =============================================================================

function Gameplay_Update()
    if gameplay_state.round_active then
        Gameplay_UpdateDynamicSystems()
    end
end

-- Register update callback
if Game then
    Game.register_update_callback(Gameplay_Update)
end

-- =============================================================================
-- CONSOLE COMMANDS
-- =============================================================================

function Gameplay_ConsoleCommand(cmd, args)
    if cmd == "stats" then
        Gameplay_ShowStatistics()
    elseif cmd == "leaderboard" then
        Gameplay_ShowLeaderboard()
    elseif cmd == "demo" then
        if args and args[1] then
            Gameplay_RunDemoScenario(args[1])
        else
            print("Available demos: powerup_madness, environmental_hazard, ultimate_showdown, peaceful_mode")
        end
    elseif cmd == "start_round" then
        Gameplay_StartRound()
    elseif cmd == "end_round" then
        Gameplay_EndRound()
    elseif cmd == "powerups" then
        Powerups.test_all()
    elseif cmd == "weapons" then
        Weapons.test_all()
    elseif cmd == "environmental" then
        Environmental.test_all()
    else
        print("Available commands: stats, leaderboard, demo <scenario>, start_round, end_round")
        print("System tests: powerups, weapons, environmental")
    end
end

-- Register console command handler
if Console then
    Console.register_command("gameplay", Gameplay_ConsoleCommand)
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Round management
    start_round = Gameplay_StartRound,
    end_round = Gameplay_EndRound,

    -- Player management
    player_joined = Gameplay_PlayerJoined,
    player_left = Gameplay_PlayerLeft,

    -- Statistics
    show_statistics = Gameplay_ShowStatistics,
    show_leaderboard = Gameplay_ShowLeaderboard,

    -- Demo scenarios
    run_demo = Gameplay_RunDemoScenario,

    -- Update function
    update = Gameplay_Update,

    -- Console command
    console_command = Gameplay_ConsoleCommand
}
