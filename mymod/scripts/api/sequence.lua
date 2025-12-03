--[[
=============================================================================
Sequence API - Timeline-based sequences and cinematics
Note: Sequence table is already registered globally by C code
This module adds convenience functions
=============================================================================
]]

-- Sequence table is already global, just add convenience functions

-- Helper function to create a step
function Sequence.step(time, action)
    return {
        time = time,
        action = action
    }
end

-- Example usage:
-- Sequence.define("intro_cinematic", {
--     Sequence.step(0.0, function() print("Start") end),
--     Sequence.step(2.0, function() print("Middle") end),
--     Sequence.step(5.0, function() print("End") end)
-- })
-- Sequence.play("intro_cinematic")

