--[[
=============================================================================
Sequence System Example
Demonstrates timeline-based sequences and cinematics
=============================================================================
]]

-- Load API
require("api.init")

-- Define an intro cinematic sequence
Sequence.define("intro_cinematic", {
    Sequence.step(0.0, function()
        print("Fade in...")
        -- FadeIn(1.0)
    end),
    Sequence.step(1.0, function()
        print("Camera pans to hero")
        -- CameraPanTo(heroPosition, 2.0)
    end),
    Sequence.step(3.0, function()
        print("Hero speaks")
        -- PlayDialogue("intro_line_1")
    end),
    Sequence.step(5.0, function()
        print("Show objective")
        Game.SetObjective("Survive the onslaught")
    end),
    Sequence.step(7.0, function()
        print("Fade out...")
        -- FadeOut(1.0)
    end),
    Sequence.step(8.0, function()
        print("Sequence complete")
        -- Start gameplay
    end)
})

-- Play sequence when level starts
Events.on("level_init", function()
    Sequence.play("intro_cinematic")
end)

print("Sequence example loaded")

