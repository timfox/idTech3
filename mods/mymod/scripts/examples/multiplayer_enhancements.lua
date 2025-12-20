-- Enhanced Multiplayer Features
-- Server-side enhancements for competitive gaming

print("=== ENHANCED MULTIPLAYER SYSTEM LOADED ===")

-- =============================================================================
-- SERVER STATISTICS & ANALYTICS
-- =============================================================================

local server_stats = {
    start_time = os.time(),
    total_connections = 0,
    active_players = 0,
    peak_players = 0,
    matches_played = 0,
    total_kills = 0,
    total_deaths = 0,
    server_uptime = 0
}

local player_stats = {}  -- player_id -> stats
local match_stats = {}   -- match_id -> stats

-- =============================================================================
-- PLAYER TRACKING SYSTEM
-- =============================================================================

function Multiplayer_PlayerJoined(player_id, player_name, steam_id)
    server_stats.total_connections = server_stats.total_connections + 1
    server_stats.active_players = server_stats.active_players + 1
    server_stats.peak_players = math.max(server_stats.peak_players, server_stats.active_players)

    player_stats[player_id] = {
        name = player_name,
        steam_id = steam_id or "unknown",
        join_time = os.time(),
        kills = 0,
        deaths = 0,
        score = 0,
        playtime = 0,
        weapons_used = {},
        powerups_collected = {},
        ping_history = {},
        connection_quality = "good"
    }

    -- Log connection
    Multiplayer_LogEvent("player_join", {
        player_id = player_id,
        name = player_name,
        steam_id = steam_id,
        timestamp = os.time()
    })

    -- Welcome message with enhanced features
    if Server then
        Server.SendMessage(player_id, "^2Welcome to Enhanced idTech3 Server!")
        Server.SendMessage(player_id, "^3Features: PBR Graphics, Advanced Weapons, Dynamic Environments")
    end

    print(string.format("Player %s (ID: %d) joined. Active players: %d",
        player_name, player_id, server_stats.active_players))
end

function Multiplayer_PlayerLeft(player_id, reason)
    if player_stats[player_id] then
        local stats = player_stats[player_id]
        stats.playtime = os.time() - stats.join_time

        -- Log final stats
        Multiplayer_LogEvent("player_leave", {
            player_id = player_id,
            name = stats.name,
            playtime = stats.playtime,
            kills = stats.kills,
            deaths = stats.deaths,
            score = stats.score,
            reason = reason or "unknown"
        })

        player_stats[player_id] = nil
    end

    server_stats.active_players = math.max(0, server_stats.active_players - 1)
    print(string.format("Player %d left. Reason: %s. Active players: %d",
        player_id, reason or "unknown", server_stats.active_players))
end

function Multiplayer_UpdatePlayerStats(player_id, stat_type, value)
    local stats = player_stats[player_id]
    if not stats then return end

    if stat_type == "kill" then
        stats.kills = stats.kills + value
        server_stats.total_kills = server_stats.total_kills + value
    elseif stat_type == "death" then
        stats.deaths = stats.deaths + value
        server_stats.total_deaths = server_stats.total_deaths + value
    elseif stat_type == "score" then
        stats.score = stats.score + value
    elseif stat_type == "weapon" then
        stats.weapons_used[value] = (stats.weapons_used[value] or 0) + 1
    elseif stat_type == "powerup" then
        stats.powerups_collected[value] = (stats.powerups_collected[value] or 0) + 1
    end
end

-- =============================================================================
-- ANTI-CHEAT SYSTEM
-- =============================================================================

local anticheat_flags = {}  -- player_id -> flags

function Multiplayer_AntiCheat_Check(player_id, check_type, value)
    local flags = anticheat_flags[player_id] or {}

    if check_type == "speed" then
        -- Speed hack detection
        if value > 1000 then  -- Unrealistic speed
            flags.speed_hack = (flags.speed_hack or 0) + 1
            if flags.speed_hack >= 3 then
                Multiplayer_KickPlayer(player_id, "Speed hack detected")
                return
            end
        end

    elseif check_type == "aim" then
        -- Aim hack detection (perfect accuracy)
        if value > 0.95 and math.random() < 0.01 then  -- Random sampling
            flags.aim_hack = (flags.aim_hack or 0) + 1
            if flags.aim_hack >= 5 then
                Multiplayer_KickPlayer(player_id, "Aim hack suspected")
                return
            end
        end

    elseif check_type == "wallhack" then
        -- Wallhack detection (seeing through walls)
        flags.wallhack = (flags.wallhack or 0) + 1
        if flags.wallhack >= 10 then
            Multiplayer_KickPlayer(player_id, "Wallhack detected")
        end
    end

    anticheat_flags[player_id] = flags
end

function Multiplayer_KickPlayer(player_id, reason)
    print(string.format("Kicking player %d: %s", player_id, reason))

    Multiplayer_LogEvent("player_kicked", {
        player_id = player_id,
        reason = reason,
        timestamp = os.time()
    })

    if Server then
        Server.KickPlayer(player_id, reason)
    end
end

-- =============================================================================
-- MATCH MANAGEMENT
-- =============================================================================

function Multiplayer_StartMatch(match_id, gamemode, map)
    match_stats[match_id] = {
        id = match_id,
        gamemode = gamemode,
        map = map,
        start_time = os.time(),
        end_time = nil,
        players = {},
        events = {},
        winner = nil
    }

    server_stats.matches_played = server_stats.matches_played + 1

    Multiplayer_LogEvent("match_start", {
        match_id = match_id,
        gamemode = gamemode,
        map = map,
        player_count = server_stats.active_players
    })

    print(string.format("Match %s started: %s on %s (%d players)",
        match_id, gamemode, map, server_stats.active_players))
end

function Multiplayer_EndMatch(match_id, winner)
    local match = match_stats[match_id]
    if match then
        match.end_time = os.time()
        match.duration = match.end_time - match.start_time
        match.winner = winner

        Multiplayer_LogEvent("match_end", {
            match_id = match_id,
            duration = match.duration,
            winner = winner,
            total_events = #match.events
        })

        print(string.format("Match %s ended. Duration: %d seconds. Winner: %s",
            match_id, match.duration, winner or "Draw"))
    end
end

-- =============================================================================
-- NETWORK MONITORING
-- =============================================================================

local network_stats = {
    total_packets = 0,
    lost_packets = 0,
    ping_average = 0,
    bandwidth_usage = 0
}

function Multiplayer_MonitorNetwork()
    -- Update network statistics
    if Server then
        local stats = Server.GetNetworkStats()
        network_stats.total_packets = stats.packets_sent or 0
        network_stats.lost_packets = stats.packets_lost or 0
        network_stats.bandwidth_usage = stats.bandwidth or 0

        -- Calculate average ping
        local total_ping = 0
        local player_count = 0
        for player_id, _ in pairs(player_stats) do
            local ping = Server.GetPlayerPing(player_id) or 0
            total_ping = total_ping + ping
            player_count = player_count + 1
        end

        if player_count > 0 then
            network_stats.ping_average = total_ping / player_count
        end
    end
end

-- =============================================================================
-- PERFORMANCE MONITORING
-- =============================================================================

function Multiplayer_MonitorPerformance()
    if Performance then
        local fps = Performance.get_fps()
        local memory = Performance.get_memory_usage()
        local cpu = Performance.get_cpu_usage()

        -- Log performance issues
        if fps < 30 then
            Multiplayer_LogEvent("performance_warning", {
                type = "low_fps",
                fps = fps,
                memory = memory,
                cpu = cpu,
                player_count = server_stats.active_players
            })
        end

        if memory > 500 * 1024 * 1024 then  -- 500MB
            Multiplayer_LogEvent("performance_warning", {
                type = "high_memory",
                fps = fps,
                memory = memory,
                cpu = cpu,
                player_count = server_stats.active_players
            })
        end
    end
end

-- =============================================================================
-- ENHANCED GAMEPLAY FEATURES
-- =============================================================================

function Multiplayer_SpawnDynamicPowerup()
    -- Spawn powerups based on game state
    if server_stats.active_players >= 4 then
        -- In larger games, spawn more powerups
        return "QUAD_DAMAGE"
    elseif server_stats.active_players >= 2 then
        -- In medium games, spawn standard powerups
        local powerups = {"SPEED", "SHIELD", "REGENERATION"}
        return powerups[math.random(#powerups)]
    end

    return nil
end

function Multiplayer_AdjustDifficulty()
    -- Adjust gameplay based on player skill levels
    local avg_skill = 0
    local player_count = 0

    for _, stats in pairs(player_stats) do
        if stats.kills > 0 or stats.deaths > 0 then
            local kdr = stats.kills / math.max(1, stats.deaths)
            avg_skill = avg_skill + kdr
            player_count = player_count + 1
        end
    end

    if player_count > 0 then
        avg_skill = avg_skill / player_count

        if avg_skill > 2.0 then
            -- High skill players - increase challenge
            return "hard"
        elseif avg_skill > 1.0 then
            -- Medium skill players - normal difficulty
            return "normal"
        else
            -- Low skill players - reduce challenge
            return "easy"
        end
    end

    return "normal"
end

-- =============================================================================
-- LOGGING SYSTEM
-- =============================================================================

local log_buffer = {}
local log_file = "multiplayer_server.log"

function Multiplayer_LogEvent(event_type, data)
    local entry = {
        timestamp = os.time(),
        type = event_type,
        data = data
    }

    table.insert(log_buffer, entry)

    -- Write to file periodically
    if #log_buffer >= 10 then
        Multiplayer_FlushLogs()
    end
end

function Multiplayer_FlushLogs()
    if #log_buffer == 0 then return end

    local file = io.open(log_file, "a")
    if file then
        for _, entry in ipairs(log_buffer) do
            file:write(string.format("[%s] %s: %s\n",
                os.date("%Y-%m-%d %H:%M:%S", entry.timestamp),
                entry.type,
                Multiplayer_SerializeData(entry.data)))
        end
        file:close()
    end

    log_buffer = {}
end

function Multiplayer_SerializeData(data)
    if type(data) == "table" then
        local parts = {}
        for k, v in pairs(data) do
            table.insert(parts, string.format("%s=%s", k, tostring(v)))
        end
        return "{" .. table.concat(parts, ", ") .. "}"
    end
    return tostring(data)
end

-- =============================================================================
-- STATISTICS & REPORTING
-- =============================================================================

function Multiplayer_GenerateReport()
    print("=== SERVER STATISTICS REPORT ===")

    local uptime = os.time() - server_stats.start_time
    print(string.format("Server Uptime: %d seconds (%d hours)",
        uptime, math.floor(uptime / 3600)))

    print(string.format("Total Connections: %d", server_stats.total_connections))
    print(string.format("Peak Players: %d", server_stats.peak_players))
    print(string.format("Matches Played: %d", server_stats.matches_played))
    print(string.format("Total Kills: %d", server_stats.total_kills))
    print(string.format("Total Deaths: %d", server_stats.total_deaths))

    if server_stats.total_deaths > 0 then
        local kdr = server_stats.total_kills / server_stats.total_deaths
        print(string.format("Server K/D Ratio: %.2f", kdr))
    end

    print("\n=== PLAYER STATISTICS ===")
    for player_id, stats in pairs(player_stats) do
        if stats.kills > 0 or stats.deaths > 0 then
            local kdr = stats.kills / math.max(1, stats.deaths)
            print(string.format("Player %s: %d/%d (%.2f KDR)",
                stats.name, stats.kills, stats.deaths, kdr))
        end
    end

    print("\n=== NETWORK STATISTICS ===")
    print(string.format("Average Ping: %.1f ms", network_stats.ping_average))
    print(string.format("Packet Loss: %.2f%%", (network_stats.lost_packets / math.max(1, network_stats.total_packets)) * 100))
    print(string.format("Bandwidth Usage: %.1f KB/s", network_stats.bandwidth_usage / 1024))

    print("================================")
end

-- =============================================================================
-- UPDATE LOOP
-- =============================================================================

function Multiplayer_Update()
    -- Update server uptime
    server_stats.server_uptime = os.time() - server_stats.start_time

    -- Monitor network and performance
    Multiplayer_MonitorNetwork()
    Multiplayer_MonitorPerformance()

    -- Periodic statistics logging
    if server_stats.server_uptime % 300 == 0 then  -- Every 5 minutes
        Multiplayer_GenerateReport()
        Multiplayer_FlushLogs()
    end
end

-- =============================================================================
-- EVENT HANDLERS
-- =============================================================================

Events.on("player_connected", function(data)
    Multiplayer_PlayerJoined(data.player_id, data.name, data.steam_id)
end)

Events.on("player_disconnected", function(data)
    Multiplayer_PlayerLeft(data.player_id, data.reason)
end)

Events.on("player_killed", function(data)
    Multiplayer_UpdatePlayerStats(data.attacker, "kill", 1)
    Multiplayer_UpdatePlayerStats(data.victim, "death", 1)

    -- Anti-cheat checks
    Multiplayer_AntiCheat_Check(data.attacker, "aim", data.accuracy or 0)
end)

Events.on("match_started", function(data)
    Multiplayer_StartMatch(data.match_id, data.gamemode, data.map)
end)

Events.on("match_ended", function(data)
    Multiplayer_EndMatch(data.match_id, data.winner)
end)

-- Register update callback
if Game then
    Game.register_update_callback(Multiplayer_Update)
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Player management
    player_joined = Multiplayer_PlayerJoined,
    player_left = Multiplayer_PlayerLeft,
    update_stats = Multiplayer_UpdatePlayerStats,

    -- Anti-cheat
    check_anticheat = Multiplayer_AntiCheat_Check,
    kick_player = Multiplayer_KickPlayer,

    -- Match management
    start_match = Multiplayer_StartMatch,
    end_match = Multiplayer_EndMatch,

    -- Monitoring
    monitor_network = Multiplayer_MonitorNetwork,
    monitor_performance = Multiplayer_MonitorPerformance,

    -- Statistics
    generate_report = Multiplayer_GenerateReport,

    -- Logging
    log_event = Multiplayer_LogEvent,
    flush_logs = Multiplayer_FlushLogs,

    -- Update function
    update = Multiplayer_Update
}
