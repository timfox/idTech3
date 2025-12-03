--[[
=============================================================================
Animation Events Example
Demonstrates animation-driven gameplay hooks
=============================================================================
]]

-- Load API
require("api.init")

-- Example: Handle hit frames
Events.onHitFrame(function(entityNum)
    print("Hit frame triggered for entity " .. entityNum)
    -- Deal damage, play sound, spawn effects, etc.
end)

-- Example: Handle parry windows
Events.onParryWindowOpen(function(entityNum)
    print("Parry window opened for entity " .. entityNum)
    -- Enable parry detection
end)

Events.onParryWindowClose(function(entityNum)
    print("Parry window closed for entity " .. entityNum)
    -- Disable parry detection
end)

-- Example: Footstep events
Events.onFootstep(function(entityNum)
    print("Footstep for entity " .. entityNum)
    -- Play footstep sound, spawn dust particles, etc.
end)

-- Example: Weapon fire events
Events.onWeaponFire(function(entityNum)
    print("Weapon fired for entity " .. entityNum)
    -- Spawn muzzle flash, play sound, etc.
end)

print("Animation events example loaded")

