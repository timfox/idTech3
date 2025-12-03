--[[
=============================================================================
Atmosphere System Example
Demonstrates scriptable lighting and mood control
=============================================================================
]]

-- Load API
require("api.init")

-- Example: Transition to mysterious atmosphere
Atmosphere.SetPreset(Atmosphere.PRESET_MYSTERIOUS, 2.0)  -- 2 second transition

-- Example: Add fog
Atmosphere.SetFog(0.05, 100.0, 2000.0, 0.3, 0.3, 0.4, 1.0)

-- Example: Set bloom
Atmosphere.SetBloom(0.8, 0.8, 1.2, 1.0)

-- Example: Change time of day
coroutine.wrap(function()
    for timeOfDay = 0.0, 1.0, 0.01 do
        Atmosphere.SetTimeOfDay(timeOfDay, 0.1)
        wait(0.1)
    end
end)()

-- Example: Combat atmosphere
Events.on("encounter_start", function()
    Atmosphere.SetPreset(Atmosphere.PRESET_COMBAT, 0.5)
end)

Events.on("encounter_end", function()
    Atmosphere.SetPreset(Atmosphere.PRESET_CALM, 1.0)
end)

print("Atmosphere example loaded")

