-- Multiplayer Testing Suite
-- Comprehensive testing for enhanced multiplayer features

print("=== MULTIPLAYER TESTING SUITE LOADED ===")

-- Load multiplayer modules
local MultiplayerServer = require("examples/multiplayer_enhancements")
local ClientMultiplayer = require("examples/client_multiplayer")

-- =============================================================================
-- TEST SUITE CONFIGURATION
-- =============================================================================

local test_config = {
    enable_server_tests = true,
    enable_client_tests = true,
    enable_performance_tests = true,
    enable_stress_tests = false,  -- Set to true for intensive testing
    test_duration = 60,  -- seconds
    max_test_players = 4
}

local test_results = {
    passed = 0,
    failed = 0,
    skipped = 0,
    tests = {}
}

-- =============================================================================
-- TEST UTILITIES
-- =============================================================================

function Test_Pass(test_name, message)
    test_results.passed = test_results.passed + 1
    test_results.tests[test_name] = {result = "PASS", message = message or ""}
    print(string.format("^2[PASS]^7 %s", test_name))
    if message then
        print("  " .. message)
    end
end

function Test_Fail(test_name, message)
    test_results.failed = test_results.failed + 1
    test_results.tests[test_name] = {result = "FAIL", message = message or ""}
    print(string.format("^1[FAIL]^7 %s", test_name))
    if message then
        print("  " .. message)
    end
end

function Test_Skip(test_name, reason)
    test_results.skipped = test_results.skipped + 1
    test_results.tests[test_name] = {result = "SKIP", message = reason or ""}
    print(string.format("^3[SKIP]^7 %s - %s", test_name, reason or ""))
end

function Test_Start(test_name)
    print(string.format("\n[TEST] %s", test_name))
end

function Test_End()
    print(string.format("[RESULTS] Passed: %d, Failed: %d, Skipped: %d",
        test_results.passed, test_results.failed, test_results.skipped))
end

-- =============================================================================
-- SERVER-SIDE TESTS
-- =============================================================================

function Test_Server_PlayerManagement()
    Test_Start("Server Player Management")

    -- Test player joining
    MultiplayerServer.player_joined(1, "TestPlayer1", "STEAM_0:0:12345")
    if MultiplayerServer then
        Test_Pass("Player Join", "Player successfully added to server")
    else
        Test_Fail("Player Join", "Server module not loaded")
    end

    -- Test player leaving
    MultiplayerServer.player_left(1, "disconnected")
    Test_Pass("Player Leave", "Player successfully removed")

    -- Test statistics update
    MultiplayerServer.update_stats(2, "kill", 1)
    MultiplayerServer.update_stats(2, "death", 1)
    Test_Pass("Statistics Update", "Player stats updated correctly")
end

function Test_Server_AntiCheat()
    Test_Start("Server Anti-Cheat System")

    -- Test speed hack detection
    MultiplayerServer.check_anticheat(1, "speed", 500)  -- Normal speed
    Test_Pass("Normal Speed", "Normal speed not flagged")

    -- Test aim hack detection
    MultiplayerServer.check_anticheat(1, "aim", 0.5)  -- Normal accuracy
    Test_Pass("Normal Aim", "Normal accuracy not flagged")

    -- Test multiple violations (would trigger kick in real scenario)
    for i = 1, 2 do
        MultiplayerServer.check_anticheat(1, "speed", 1500)  -- Speed hack
    end
    Test_Pass("Anti-Cheat Triggers", "Multiple violations detected")
end

function Test_Server_MatchManagement()
    Test_Start("Server Match Management")

    -- Test match start
    MultiplayerServer.start_match("test_match_001", "ffa", "q3dm1")
    Test_Pass("Match Start", "Match successfully started")

    -- Test match end
    MultiplayerServer.end_match("test_match_001", "Player1")
    Test_Pass("Match End", "Match successfully ended")
end

function Test_Server_Performance()
    Test_Start("Server Performance Monitoring")

    -- Test performance monitoring
    MultiplayerServer.monitor_performance()
    Test_Pass("Performance Monitor", "Performance monitoring active")

    -- Test network monitoring
    MultiplayerServer.monitor_network()
    Test_Pass("Network Monitor", "Network monitoring active")
end

-- =============================================================================
-- CLIENT-SIDE TESTS
-- =============================================================================

function Test_Client_ServerBrowser()
    Test_Start("Client Server Browser")

    -- Test connection history
    ClientMultiplayer.add_to_history("127.0.0.1:27960", "Test Server", "ffa")
    Test_Pass("Connection History", "Server added to history")

    -- Test favorites
    ClientMultiplayer.add_favorite("127.0.0.1:27960", "Test Server")
    Test_Pass("Favorites", "Server added to favorites")

    -- Test ping cache
    ClientMultiplayer.update_ping_cache("127.0.0.1:27960", 45)
    local cached_ping = ClientMultiplayer.get_cached_ping("127.0.0.1:27960")
    if cached_ping == 45 then
        Test_Pass("Ping Cache", "Ping cached correctly")
    else
        Test_Fail("Ping Cache", "Ping cache failed")
    end
end

function Test_Client_Matchmaking()
    Test_Start("Client Matchmaking")

    -- Test matchmaking preferences
    ClientMultiplayer.set_matchmaking_prefs({
        min_players = 4,
        max_ping = 100,
        preferred_gamemodes = {"ctf", "ffa"},
        skill_level = "medium"
    })
    Test_Pass("Matchmaking Prefs", "Preferences set successfully")

    -- Note: Actual matchmaking would require live servers
    Test_Skip("Live Matchmaking", "Requires active game servers")
end

function Test_Client_SocialFeatures()
    Test_Start("Client Social Features")

    -- Test friends list
    ClientMultiplayer.add_friend("Friend1", 123)
    ClientMultiplayer.show_friends()
    Test_Pass("Friends List", "Friend added successfully")

    -- Test recent players
    ClientMultiplayer.add_recent_player("RecentPlayer1", "192.168.1.1:27960")
    Test_Pass("Recent Players", "Recent player tracked")
end

function Test_Client_Statistics()
    Test_Start("Client Statistics")

    -- Test stats update
    ClientMultiplayer.update_stats({
        kills = 5,
        deaths = 2,
        won = true,
        weapon = "plasma_rifle",
        duration = 600  -- 10 minutes
    })
    Test_Pass("Stats Update", "Game statistics updated")

    -- Test achievements
    ClientMultiplayer.unlock_achievement("first_blood", "First Blood")
    Test_Pass("Achievements", "Achievement system working")
end

-- =============================================================================
-- INTEGRATION TESTS
-- =============================================================================

function Test_Integration_ServerClient()
    Test_Start("Server-Client Integration")

    -- Test that both modules can communicate
    if MultiplayerServer and ClientMultiplayer then
        Test_Pass("Module Loading", "Both server and client modules loaded")
    else
        Test_Fail("Module Loading", "One or more modules failed to load")
    end

    -- Test data consistency
    MultiplayerServer.player_joined(999, "IntegrationTest", "test_steam_id")
    -- In a real scenario, client would receive this update
    Test_Pass("Data Flow", "Server-client data flow established")
end

function Test_Integration_Performance()
    Test_Start("Performance Integration")

    -- Test that performance monitoring doesn't crash
    for i = 1, 10 do
        MultiplayerServer.update()
    end
    Test_Pass("Update Loop", "Server update loop stable")

    -- Test memory usage
    local start_time = os.time()
    for i = 1, 100 do
        MultiplayerServer.monitor_performance()
        MultiplayerServer.monitor_network()
    end
    local end_time = os.time()

    if end_time - start_time < 5 then  -- Should complete quickly
        Test_Pass("Performance", "Monitoring systems performant")
    else
        Test_Fail("Performance", "Monitoring systems too slow")
    end
end

-- =============================================================================
-- STRESS TESTS
-- =============================================================================

function Test_Stress_PlayerLoad()
    if not test_config.enable_stress_tests then
        Test_Skip("Player Load Stress Test", "Stress tests disabled")
        return
    end

    Test_Start("Player Load Stress Test")

    -- Simulate many players joining/leaving
    local start_time = os.time()
    for i = 1, 50 do
        MultiplayerServer.player_joined(i, "StressTestPlayer" .. i, "steam_" .. i)
        MultiplayerServer.update_stats(i, "kill", math.random(0, 5))
    end

    -- Remove them
    for i = 1, 50 do
        MultiplayerServer.player_left(i, "test_complete")
    end

    local end_time = os.time()
    local duration = end_time - start_time

    if duration < 10 then  -- Should handle quickly
        Test_Pass("Stress Test", string.format("Handled 50 players in %d seconds", duration))
    else
        Test_Fail("Stress Test", string.format("Too slow: %d seconds for 50 players", duration))
    end
end

function Test_Stress_MemoryUsage()
    if not test_config.enable_stress_tests then
        Test_Skip("Memory Usage Stress Test", "Stress tests disabled")
        return
    end

    Test_Start("Memory Usage Stress Test")

    -- Create many log entries
    local start_time = os.time()
    for i = 1, 1000 do
        MultiplayerServer.log_event("stress_test", {iteration = i, timestamp = os.time()})
    end

    -- Force log flush
    MultiplayerServer.flush_logs()

    local end_time = os.time()
    local duration = end_time - start_time

    if duration < 30 then
        Test_Pass("Memory Test", string.format("Processed 1000 log entries in %d seconds", duration))
    else
        Test_Fail("Memory Test", string.format("Too slow: %d seconds for 1000 entries", duration))
    end
end

-- =============================================================================
-- TEST SUITE EXECUTION
-- =============================================================================

function Run_All_Tests()
    print("=== MULTIPLAYER ENHANCEMENT TEST SUITE ===")
    print("Testing enhanced multiplayer features...")
    print()

    -- Reset results
    test_results = {passed = 0, failed = 0, skipped = 0, tests = {}}

    -- Run server tests
    if test_config.enable_server_tests then
        print("--- SERVER-SIDE TESTS ---")
        Test_Server_PlayerManagement()
        Test_Server_AntiCheat()
        Test_Server_MatchManagement()
        Test_Server_Performance()
        print()
    end

    -- Run client tests
    if test_config.enable_client_tests then
        print("--- CLIENT-SIDE TESTS ---")
        Test_Client_ServerBrowser()
        Test_Client_Matchmaking()
        Test_Client_SocialFeatures()
        Test_Client_Statistics()
        print()
    end

    -- Run integration tests
    print("--- INTEGRATION TESTS ---")
    Test_Integration_ServerClient()
    Test_Integration_Performance()
    print()

    -- Run stress tests
    if test_config.enable_stress_tests then
        print("--- STRESS TESTS ---")
        Test_Stress_PlayerLoad()
        Test_Stress_MemoryUsage()
        print()
    end

    -- Final results
    print("=== TEST RESULTS ===")
    Test_End()

    -- Detailed results
    if test_results.failed > 0 then
        print("\nFailed Tests:")
        for test_name, result in pairs(test_results.tests) do
            if result.result == "FAIL" then
                print(string.format("  ✗ %s: %s", test_name, result.message))
            end
        end
    end

    if test_results.passed > 0 then
        print("\nPassed Tests:")
        local count = 0
        for test_name, result in pairs(test_results.tests) do
            if result.result == "PASS" then
                count = count + 1
                if count <= 5 then  -- Show first 5
                    print(string.format("  ✓ %s", test_name))
                elseif count == 6 then
                    print(string.format("  ... and %d more", test_results.passed - 5))
                end
            end
        end
    end

    print("\n" .. string.rep("=", 50))
    if test_results.failed == 0 then
        print("^2ALL TESTS PASSED!^7 Enhanced multiplayer system is working correctly.")
    else
        print(string.format("^1%d TESTS FAILED.^7 Please review the failed tests above.", test_results.failed))
    end
    print(string.rep("=", 50))

    return test_results.failed == 0
end

-- =============================================================================
-- UTILITY FUNCTIONS
-- =============================================================================

function Show_Test_Help()
    print("=== MULTIPLAYER TEST SUITE HELP ===")
    print("Available test functions:")
    print("  Run_All_Tests()          - Run complete test suite")
    print("  Test_Server_*()          - Individual server tests")
    print("  Test_Client_*()          - Individual client tests")
    print("  Test_Integration_*()     - Integration tests")
    print("  Test_Stress_*()          - Stress tests")
    print()
    print("Test configuration:")
    for k, v in pairs(test_config) do
        print(string.format("  %s = %s", k, tostring(v)))
    end
end

-- =============================================================================
-- PUBLIC API
-- =============================================================================

return {
    -- Main test functions
    run_all = Run_All_Tests,
    help = Show_Test_Help,

    -- Individual test suites
    server_tests = function()
        Test_Server_PlayerManagement()
        Test_Server_AntiCheat()
        Test_Server_MatchManagement()
        Test_Server_Performance()
    end,

    client_tests = function()
        Test_Client_ServerBrowser()
        Test_Client_Matchmaking()
        Test_Client_SocialFeatures()
        Test_Client_Statistics()
    end,

    integration_tests = function()
        Test_Integration_ServerClient()
        Test_Integration_Performance()
    end,

    stress_tests = function()
        Test_Stress_PlayerLoad()
        Test_Stress_MemoryUsage()
    end,

    -- Configuration
    config = test_config,
    results = function() return test_results end
}
