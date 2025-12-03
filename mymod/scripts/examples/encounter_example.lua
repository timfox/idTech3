--[[
=============================================================================
Encounter System Example
Demonstrates combat encounter state machine
=============================================================================
]]

-- Load API
require("api.init")

-- Define a boss fight encounter
Encounter.define("boss_fight", {
    trigger = Encounter.triggerProximity({x=100, y=200, z=50}, 100),
    waves = {
        Encounter.wave(5, {{x=0, y=0, z=0}}),
        Encounter.wave(10, {{x=50, y=0, z=0}}),
        Encounter.wave(1, {{x=100, y=200, z=50}})  -- Boss
    },
    on_start = function()
        print("Boss fight started!")
        Game.SetObjective("Defeat the boss")
        Atmosphere.SetPreset(Atmosphere.PRESET_COMBAT, 0.5)
    end,
    on_wave_complete = function(waveNum)
        print("Wave " .. waveNum .. " complete!")
    end,
    on_complete = function()
        print("Boss defeated!")
        Game.ClearObjective()
        Atmosphere.SetPreset(Atmosphere.PRESET_CALM, 2.0)
    end
})

-- Start encounter when player gets close
Events.on("player_entered_trigger", function(triggerName)
    if triggerName == "boss_room" then
        Encounter.start("boss_fight")
    end
end)

print("Encounter example loaded")

