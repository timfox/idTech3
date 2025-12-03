--[[
=============================================================================
Material System Example
Demonstrates runtime material parameter control
=============================================================================
]]

-- Load API
require("api.init")

-- Example: Make a material wet over time
local materialIndex = 0  -- Replace with actual material index

coroutine.wrap(function()
    for wetness = 0.0, 1.0, 0.1 do
        Material.SetWetness(materialIndex, wetness)
        wait(0.5)  -- Wait 0.5 seconds
    end
end)()

-- Example: Add magic glow to a material
Material.SetMagicGlow(materialIndex, 0.8, 1.0, 0.5, 1.0)  -- Bright green glow

-- Example: Damage a material over time
coroutine.wrap(function()
    for damage = 0.0, 1.0, 0.05 do
        Material.SetDamage(materialIndex, damage)
        wait(0.2)
    end
end)()

print("Material example loaded")

