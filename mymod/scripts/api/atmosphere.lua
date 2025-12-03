--[[
=============================================================================
Atmosphere API - Scriptable lighting, fog, and post-processing
=============================================================================
]]

Atmosphere = {}

-- Preset constants
Atmosphere.PRESET_BRUTAL = 0
Atmosphere.PRESET_MYSTERIOUS = 1
Atmosphere.PRESET_COMBAT = 2
Atmosphere.PRESET_CALM = 3
Atmosphere.PRESET_CUSTOM = 4

-- Set atmosphere preset
function Atmosphere.SetPreset(preset, transitionTime)
    transitionTime = transitionTime or 0.0
    return AtmosphereSetPreset(preset, transitionTime) ~= 0
end

-- Set exposure
function Atmosphere.SetExposure(exposure, transitionTime)
    transitionTime = transitionTime or 0.0
    return AtmosphereSetExposure(exposure, transitionTime) ~= 0
end

-- Set fog
function Atmosphere.SetFog(density, start, endDist, r, g, b, transitionTime)
    r = r or 0.5
    g = g or 0.5
    b = b or 0.5
    transitionTime = transitionTime or 0.0
    return AtmosphereSetFog(density, start, endDist, r, g, b, transitionTime) ~= 0
end

-- Set bloom
function Atmosphere.SetBloom(intensity, threshold, size, transitionTime)
    threshold = threshold or 1.0
    size = size or 0.5
    transitionTime = transitionTime or 0.0
    return AtmosphereSetBloom(intensity, threshold, size, transitionTime) ~= 0
end

-- Set color tint
function Atmosphere.SetColorTint(r, g, b, transitionTime)
    transitionTime = transitionTime or 0.0
    return AtmosphereSetColorTint(r, g, b, transitionTime) ~= 0
end

-- Set time of day
function Atmosphere.SetTimeOfDay(timeOfDay, transitionTime)
    transitionTime = transitionTime or 0.0
    return AtmosphereSetTimeOfDay(timeOfDay, transitionTime) ~= 0
end

return Atmosphere

