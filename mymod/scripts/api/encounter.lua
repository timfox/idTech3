--[[
=============================================================================
Encounter API - Combat encounter state machine
Note: Encounter table is already registered globally by C code
This module adds convenience functions
=============================================================================
]]

-- Encounter table is already global, just add convenience functions

-- Helper function to create wave config
function Encounter.wave(enemyCount, spawnPoints)
    return {
        enemyCount = enemyCount,
        spawnPoints = spawnPoints or {}
    }
end

-- Helper function to create trigger config
function Encounter.triggerProximity(position, radius)
    return {
        type = "proximity",
        position = position,
        radius = radius
    }
end

-- Example usage:
-- Encounter.define("boss_fight", {
--     trigger = Encounter.triggerProximity({x=100, y=200, z=50}, 100),
--     waves = {
--         Encounter.wave(5, {{x=0, y=0, z=0}}),
--         Encounter.wave(10, {{x=50, y=0, z=0}})
--     },
--     on_start = function() print("Boss fight started!") end
-- })
-- Encounter.start("boss_fight")

