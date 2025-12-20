-- Enhanced Client Multiplayer Features
-- Client-side enhancements for multiplayer gaming

print("=== ENHANCED CLIENT MULTIPLAYER LOADED ===")

-- =============================================================================
-- CONNECTION MANAGEMENT
-- =============================================================================

local connection_history = {}
local server_favorites = {}
local ping_cache = {}

function Client_AddToHistory(server_ip, server_name, gamemode)
    local entry = {
        ip = server_ip,
        name = server_name or "Unknown Server",
        gamemode = gamemode or "Unknown",
        last_connected = os.time(),
        ping = Client_GetCachedPing(server_ip) or 999,
        favorite = server_favorites[server_ip] or false
    }

    -- Remove existing entry for this server
    for i, existing in ipairs(connection_history) do
        if existing.ip == server_ip then
            table.remove(connection_history, i)
            break
        end
    end

    -- Add to front of history
    table.insert(connection_history, 1, entry)

    -- Keep only last 20 servers
    while #connection_history > 20 do
        table.remove(connection_history)
    end

    Client_SaveConnectionHistory()
end

function Client_AddFavorite(server_ip, server_name)
    server_favorites[server_ip] = {
        name = server_name or "Unknown Server",
        added_time = os.time()
    }
    Client_SaveFavorites()
end

function Client_RemoveFavorite(server_ip)
    server_favorites[server_ip] = nil
    Client_SaveFavorites()
end

function Client_GetCachedPing(server_ip)
    return ping_cache[server_ip]
end

function Client_UpdatePingCache(server_ip, ping)
    ping_cache[server_ip] = ping
end

-- =============================================================================
-- SERVER BROWSER ENHANCEMENTS
-- =============================================================================

function Client_EnhancedServerBrowser()
    print("=== ENHANCED SERVER BROWSER ===")

    -- Get server list
    local servers = Client_GetServerList()

    -- Sort by enhanced criteria
    table.sort(servers, function(a, b)
        -- Prioritize favorites
        if a.favorite and not b.favorite then return true end
        if not a.favorite and b.favorite then return false end

        -- Then by ping
        if a.ping ~= b.ping then
            return a.ping < b.ping
        end

        -- Then by player count
        return (a.players or 0) > (b.players or 0)
    end)

    print("Available Servers:")
    for i, server in ipairs(servers) do
        local favorite_mark = server.favorite and "★" or " "
        local ping_color = Client_GetPingColor(server.ping)
        local players = string.format("%d/%d", server.players or 0, server.max_players or 0)

        print(string.format("%d. %s %s%s^7 %s (%s) %s",
            i, favorite_mark, ping_color, server.ping, server.name, players, server.gamemode))
    end

    print("\nCommands:")
    print("  join <number>    - Join server by number")
    print("  favorite <number> - Add server to favorites")
    print("  ping <number>    - Ping server")
    print("  info <number>    - Show detailed server info")
end

function Client_GetPingColor(ping)
    if ping < 50 then return "^2"      -- Green (excellent)
    elseif ping < 100 then return "^3" -- Yellow (good)
    elseif ping < 150 then return "^1" -- Red (poor)
    else return "^4"                   -- Blue (bad)
    end
end

function Client_JoinServerByNumber(server_number)
    local servers = Client_GetServerList()
    local server = servers[server_number]

    if server then
        print("Connecting to: " .. server.name)
        Client_ConnectToServer(server.ip)
        Client_AddToHistory(server.ip, server.name, server.gamemode)
    else
        print("Invalid server number: " .. server_number)
    end
end

function Client_PingServer(server_number)
    local servers = Client_GetServerList()
    local server = servers[server_number]

    if server then
        print("Pinging " .. server.name .. "...")
        local ping = Client_PingServerIP(server.ip)
        if ping then
            print(string.format("Ping: %d ms", ping))
            Client_UpdatePingCache(server.ip, ping)
        else
            print("Ping failed")
        end
    end
end

function Client_ShowServerInfo(server_number)
    local servers = Client_GetServerList()
    local server = servers[server_number]

    if server then
        print("=== SERVER INFORMATION ===")
        print("Name: " .. server.name)
        print("IP: " .. server.ip)
        print("Players: " .. (server.players or 0) .. "/" .. (server.max_players or 0))
        print("Game Mode: " .. (server.gamemode or "Unknown"))
        print("Map: " .. (server.map or "Unknown"))
        print("Ping: " .. (server.ping or "Unknown") .. " ms")
        print("Version: " .. (server.version or "Unknown"))
        print("Password Protected: " .. (server.password and "Yes" or "No"))
        print("Ranked: " .. (server.ranked and "Yes" or "No"))

        if server.modlist then
            print("Mods: " .. table.concat(server.modlist, ", "))
        end

        print("===========================")
    end
end

-- =============================================================================
-- MATCHMAKING SYSTEM
-- =============================================================================

local matchmaking_prefs = {
    min_players = 2,
    max_ping = 150,
    preferred_gamemodes = {"ctf", "ffa", "tdm"},
    skill_level = "any",
    region = "any"
}

function Client_FindMatch()
    print("Finding match with preferences:")
    print("  Min Players: " .. matchmaking_prefs.min_players)
    print("  Max Ping: " .. matchmaking_prefs.max_ping .. " ms")
    print("  Game Modes: " .. table.concat(matchmaking_prefs.preferred_gamemodes, ", "))
    print("  Skill Level: " .. matchmaking_prefs.skill_level)
    print("  Region: " .. matchmaking_prefs.region)
    print()

    local servers = Client_GetServerList()
    local candidates = {}

    for _, server in ipairs(servers) do
        if Client_ServerMatchesPreferences(server) then
            table.insert(candidates, server)
        end
    end

    if #candidates == 0 then
        print("No suitable servers found. Try adjusting preferences.")
        return
    end

    -- Sort by quality score
    table.sort(candidates, function(a, b)
        return Client_CalculateServerScore(a) > Client_CalculateServerScore(b)
    end)

    local best_server = candidates[1]
    print("Best match found: " .. best_server.name)
    print("Connecting...")

    Client_ConnectToServer(best_server.ip)
    Client_AddToHistory(best_server.ip, best_server.name, best_server.gamemode)
end

function Client_ServerMatchesPreferences(server)
    -- Check player count
    if (server.players or 0) < matchmaking_prefs.min_players then
        return false
    end

    -- Check ping
    if (server.ping or 999) > matchmaking_prefs.max_ping then
        return false
    end

    -- Check gamemode
    if not Client_IsPreferredGamemode(server.gamemode) then
        return false
    end

    -- Check if server is full
    if server.players and server.max_players and server.players >= server.max_players then
        return false
    end

    return true
end

function Client_IsPreferredGamemode(gamemode)
    if not gamemode then return true end

    for _, pref in ipairs(matchmaking_prefs.preferred_gamemodes) do
        if string.lower(gamemode) == string.lower(pref) then
            return true
        end
    end

    return false
end

function Client_CalculateServerScore(server)
    local score = 0

    -- Prefer lower ping
    score = score + (200 - math.min(server.ping or 200, 200))

    -- Prefer more players (but not full)
    local player_ratio = (server.players or 0) / (server.max_players or 16)
    if player_ratio < 0.9 then  -- Not full
        score = score + (player_ratio * 50)
    end

    -- Prefer favorites
    if server.favorite then
        score = score + 100
    end

    return score
end

function Client_SetMatchmakingPrefs(prefs)
    matchmaking_prefs = prefs
    Client_SaveMatchmakingPrefs()
    print("Matchmaking preferences updated")
end

-- =============================================================================
-- SOCIAL FEATURES
-- =============================================================================

local friends_list = {}
local recent_players = {}

function Client_AddFriend(player_name, player_id)
    friends_list[player_name] = {
        name = player_name,
        player_id = player_id,
        added_time = os.time(),
        status = "offline"
    }
    Client_SaveFriendsList()
    print("Added " .. player_name .. " to friends list")
end

function Client_RemoveFriend(player_name)
    friends_list[player_name] = nil
    Client_SaveFriendsList()
    print("Removed " .. player_name .. " from friends list")
end

function Client_ShowFriendsList()
    print("=== FRIENDS LIST ===")
    local online_count = 0

    for name, friend in pairs(friends_list) do
        local status_color = friend.status == "online" and "^2" or "^1"
        print(string.format("%s%s^7 - %s", status_color, name, friend.status))

        if friend.status == "online" then
            online_count = online_count + 1
        end
    end

    print("Online: " .. online_count .. "/" .. Client_CountTableElements(friends_list))
    print("==================")
end

function Client_AddRecentPlayer(player_name, server_ip)
    recent_players[player_name] = {
        name = player_name,
        server_ip = server_ip,
        last_seen = os.time()
    }

    -- Keep only last 50
    if Client_CountTableElements(recent_players) > 50 then
        -- Remove oldest
        local oldest_name = nil
        local oldest_time = os.time()

        for name, data in pairs(recent_players) do
            if data.last_seen < oldest_time then
                oldest_time = data.last_seen
                oldest_name = name
            end
        end

        if oldest_name then
            recent_players[oldest_name] = nil
        end
    end

    Client_SaveRecentPlayers()
end

-- =============================================================================
-- STATISTICS & ACHIEVEMENTS
-- =============================================================================

local player_stats = {
    total_games = 0,
    total_kills = 0,
    total_deaths = 0,
    total_wins = 0,
    favorite_weapon = "none",
    playtime_hours = 0,
    achievements = {}
}

function Client_UpdateStats(game_result)
    player_stats.total_games = player_stats.total_games + 1
    player_stats.total_kills = player_stats.total_kills + (game_result.kills or 0)
    player_stats.total_deaths = player_stats.total_deaths + (game_result.deaths or 0)

    if game_result.won then
        player_stats.total_wins = player_stats.total_wins + 1
    end

    if game_result.weapon then
        player_stats.favorite_weapon = game_result.weapon
    end

    player_stats.playtime_hours = player_stats.playtime_hours + (game_result.duration or 0) / 3600

    Client_CheckAchievements()
    Client_SaveStats()
end

function Client_ShowStats()
    print("=== PLAYER STATISTICS ===")
    print("Games Played: " .. player_stats.total_games)
    print("Wins: " .. player_stats.total_wins)
    print("Kills: " .. player_stats.total_kills)
    print("Deaths: " .. player_stats.total_deaths)

    if player_stats.total_deaths > 0 then
        local kdr = player_stats.total_kills / player_stats.total_deaths
        print(string.format("K/D Ratio: %.2f", kdr))
    end

    if player_stats.total_games > 0 then
        local winrate = (player_stats.total_wins / player_stats.total_games) * 100
        print(string.format("Win Rate: %.1f%%", winrate))
    end

    print("Favorite Weapon: " .. player_stats.favorite_weapon)
    print(string.format("Playtime: %.1f hours", player_stats.playtime_hours))
    print("Achievements: " .. Client_CountTableElements(player_stats.achievements))
    print("=========================")
end

function Client_CheckAchievements()
    -- First Blood
    if player_stats.total_kills >= 1 and not player_stats.achievements.first_blood then
        Client_UnlockAchievement("first_blood", "First Blood")
    end

    -- Sharpshooter
    if player_stats.total_kills >= 100 and not player_stats.achievements.sharpshooter then
        Client_UnlockAchievement("sharpshooter", "Sharpshooter")
    end

    -- Veteran
    if player_stats.total_games >= 100 and not player_stats.achievements.veteran then
        Client_UnlockAchievement("veteran", "Veteran")
    end

    -- Winning Streak
    if player_stats.total_wins >= 10 and not player_stats.achievements.winning_streak then
        Client_UnlockAchievement("winning_streak", "Winning Streak")
    end
end

function Client_UnlockAchievement(id, name)
    player_stats.achievements[id] = {
        name = name,
        unlocked_time = os.time()
    }

    print("^2ACHIEVEMENT UNLOCKED: " .. name .. "^7")
    if UI then
        UI.show_achievement_notification(name)
    end
end

-- =============================================================================
-- UTILITY FUNCTIONS
-- =============================================================================

function Client_CountTableElements(tbl)
    local count = 0
    for _ in pairs(tbl) do
        count = count + 1
    end
    return count
end

-- Stub functions (would be implemented with actual engine integration)
function Client_GetServerList() return {} end
function Client_ConnectToServer(ip) print("Connecting to " .. ip) end
function Client_PingServerIP(ip) return math.random(20, 200) end

-- File I/O stubs (would save/load actual files)
function Client_SaveConnectionHistory() end
function Client_SaveFavorites() end
function Client_SaveMatchmakingPrefs() end
function Client_SaveFriendsList() end
function Client_SaveRecentPlayers() end
function Client_SaveStats() end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Connection management
    add_to_history = Client_AddToHistory,
    add_favorite = Client_AddFavorite,
    remove_favorite = Client_RemoveFavorite,

    -- Server browser
    enhanced_browser = Client_EnhancedServerBrowser,
    join_server = Client_JoinServerByNumber,
    ping_server = Client_PingServer,
    server_info = Client_ShowServerInfo,

    -- Matchmaking
    find_match = Client_FindMatch,
    set_matchmaking_prefs = Client_SetMatchmakingPrefs,

    -- Social features
    add_friend = Client_AddFriend,
    remove_friend = Client_RemoveFriend,
    show_friends = Client_ShowFriendsList,
    add_recent_player = Client_AddRecentPlayer,

    -- Statistics
    update_stats = Client_UpdateStats,
    show_stats = Client_ShowStats,
    unlock_achievement = Client_UnlockAchievement,

    -- Utilities
    get_ping_color = Client_GetPingColor
}
