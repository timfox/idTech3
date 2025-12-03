--[[
=============================================================================
Game API - Designer-friendly wrapper for game functions
=============================================================================
]]

Game = {}

-- Logging
function Game.Log(message)
    print("[Game] " .. tostring(message))
end

-- Objectives
function Game.SetObjective(text)
    -- TODO: Implement objective system
    Game.Log("Objective: " .. tostring(text))
end

function Game.ClearObjective()
    -- TODO: Implement objective clearing
    Game.Log("Objective cleared")
end

-- World state
function Game.SetWorldState(key, value, transitionTime)
    transitionTime = transitionTime or 0.0
    -- TODO: Implement world state system
    Game.Log("World state [" .. tostring(key) .. "] = " .. tostring(value))
end

function Game.GetWorldState(key)
    -- TODO: Implement world state retrieval
    return 0.0
end

-- Entity spawning
function Game.SpawnEntity(classname, x, y, z)
    local entityNum = game_spawn_entity(classname, x, y, z)
    if entityNum >= 0 then
        Game.Log("Spawned " .. tostring(classname) .. " at (" .. x .. ", " .. y .. ", " .. z .. ")")
    end
    return entityNum
end

-- Entity queries
function Game.EntityExists(entityNum)
    return game_entity_exists(entityNum) ~= 0
end

function Game.GetEntityCount()
    return game_get_entity_count()
end

-- Events
function Game.TriggerEvent(entityNum, eventName)
    return game_trigger_event(entityNum, eventName) ~= 0
end

return Game

