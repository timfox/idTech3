-- game.starter main gameplay shell
-- Load: script_reload scripts/lua/main.lua
-- Hot reload: com_scriptWatch 1

local state = {
  boots = 0,
  checkpoints = 0,
  quest_id = "starter.shell",
}

local function say(msg)
  print("[starter] " .. tostring(msg))
end

function on_hotload_destroy()
  say("on_hotload_destroy boots=" .. tostring(state.boots) .. " checkpoints=" .. tostring(state.checkpoints))
  return state
end

function on_hotload_create(previous)
  if type(previous) == "table" then
    state = previous
    say("on_hotload_create restored state")
  else
    say("on_hotload_create fresh state")
  end
end

function starter_boot()
  state.boots = (state.boots or 0) + 1
  if Engine and Engine.Telemetry then
    Engine.Telemetry.record("starter_boots", state.boots)
  end
  if Engine and Engine.Quest then
    Engine.Quest.add(state.quest_id, "Bring the shell to life", "active")
    Engine.Quest.setStage(state.quest_id, "active")
  end
  if Engine and Engine.Dialogue then
    Engine.Dialogue.start("Guide", "The fastest way to find your game's identity is to build one repeatable loop.")
  end
  say("boot #" .. tostring(state.boots))
end

function starter_checkpoint()
  state.checkpoints = (state.checkpoints or 0) + 1
  if Engine and Engine.Save then
    local ok = Engine.Save.write(0, "starter_checkpoint_" .. tostring(state.checkpoints))
    say("save slot 0 -> " .. tostring(ok))
  else
    say("Engine.Save not available")
  end
end

function starter_status()
  local boots = state.boots or 0
  local checkpoints = state.checkpoints or 0
  local telemetry = "n/a"
  if Engine and Engine.Telemetry then
    telemetry = tostring(Engine.Telemetry.get("starter_boots"))
  end
  say("boots=" .. tostring(boots) .. " checkpoints=" .. tostring(checkpoints) .. " telemetry=" .. telemetry)
end

on_hotload_create(nil)
say("main.lua loaded")
say("try: lua_run starter_boot()")
say("try: lua_run starter_status()")
say("try: lua_run starter_checkpoint()")
