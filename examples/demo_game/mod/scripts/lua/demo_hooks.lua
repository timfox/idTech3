-- idtech3_demo: loaded via script_reload when engine is built with USE_LUA=ON.

local function demo_banner()
  print("[idtech3_demo] Lua demo_hooks.lua loaded (script_reload scripts/lua/demo_hooks.lua)")
end

-- Call from console after loading a map: lua_run demo_run_sprites()
function demo_run_sprites()
  if not Engine or not Engine.Sprites then
    print("[idtech3_demo] Engine.Sprites unavailable (USE_LUA build required)")
    return
  end
  Engine.Sprites.spawnLocal("billboard", "sprites/demo_billboard", 2, 2, 8, 0, 0, 96, 48)
  print("[idtech3_demo] spawnLocal billboard at (0,0,96) — see demo_sprites.cfg for server/Lua options")
end

local function demo_sp_slice()
  if Engine and Engine.AnimGraph then
    if Engine.AnimGraph.load("animgraph/idle_run.txt") then
      Engine.AnimGraph.setState("idle")
      print("[idtech3_demo] animgraph idle_run loaded")
    end
  end
  if Engine and Engine.Save then
    Engine.Save.write(0, "demo_sp_autosave")
    print("[idtech3_demo] save slot 0 written (see save/engine_slot_0.json)")
  end
end

-- Console: lua_run demo_sp_slice()
demo_banner()
