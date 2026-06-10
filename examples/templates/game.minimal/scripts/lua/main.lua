-- Minimal game template (s&box game.minimal analogue).
-- Load: script_reload scripts/lua/main.lua
-- Enable auto-reload: com_scriptWatch 1

local state = { score = 0 }

function on_hotload_destroy()
  print("[game.minimal] on_hotload_destroy score=" .. tostring(state.score))
  return state
end

function on_hotload_create(previous)
  if type(previous) == "table" then
    state = previous
    print("[game.minimal] on_hotload_create restored score=" .. tostring(state.score))
  else
    print("[game.minimal] on_hotload_create fresh start")
  end
end

function game_minimal_ping()
  state.score = state.score + 1
  print("[game.minimal] ping score=" .. tostring(state.score))
end

on_hotload_create(nil)
print("[game.minimal] main.lua loaded — lua_run game_minimal_ping()")
