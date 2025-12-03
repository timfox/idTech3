--[[
=============================================================================
Material API - Runtime material parameter control
=============================================================================
]]

Material = {}

-- Set material wetness
function Material.SetWetness(materialIndex, wetness)
    return MaterialSetWetness(materialIndex, wetness) ~= 0
end

-- Set material damage
function Material.SetDamage(materialIndex, damage)
    return MaterialSetDamage(materialIndex, damage) ~= 0
end

-- Set material corruption
function Material.SetCorruption(materialIndex, corruption)
    return MaterialSetCorruption(materialIndex, corruption) ~= 0
end

-- Set material magic glow
function Material.SetMagicGlow(materialIndex, glow, r, g, b)
    r = r or 1.0
    g = g or 1.0
    b = b or 1.0
    return MaterialSetMagicGlow(materialIndex, glow, r, g, b) ~= 0
end

-- Get material wetness
function Material.GetWetness(materialIndex)
    return MaterialGetWetness(materialIndex)
end

-- Get material damage
function Material.GetDamage(materialIndex)
    return MaterialGetDamage(materialIndex)
end

-- Get material corruption
function Material.GetCorruption(materialIndex)
    return MaterialGetCorruption(materialIndex)
end

-- Get material magic glow
function Material.GetMagicGlow(materialIndex)
    return MaterialGetMagicGlow(materialIndex)
end

return Material

