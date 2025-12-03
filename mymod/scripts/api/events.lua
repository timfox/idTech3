--[[
=============================================================================
Events API - Event bus wrapper for designer-friendly event handling
Note: Events table is already registered globally by C code
This module adds convenience functions
=============================================================================
]]

-- Events table is already global, just add convenience functions

-- Convenience functions for common animation events
function Events.onHitFrame(callback)
    Events.on("anim_hit_frame", callback)
end

function Events.onParryWindowOpen(callback)
    Events.on("anim_parry_window_open", callback)
end

function Events.onParryWindowClose(callback)
    Events.on("anim_parry_window_close", callback)
end

function Events.onRecoverStart(callback)
    Events.on("anim_recover_start", callback)
end

function Events.onRecoverEnd(callback)
    Events.on("anim_recover_end", callback)
end

function Events.onFootstep(callback)
    Events.on("anim_footstep", callback)
end

function Events.onWeaponFire(callback)
    Events.on("anim_weapon_fire", callback)
end

function Events.onWeaponReload(callback)
    Events.on("anim_weapon_reload", callback)
end

function Events.onCustom(callback)
    Events.on("anim_custom", callback)
end

