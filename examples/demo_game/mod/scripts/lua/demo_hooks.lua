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

-- Console: lua_run demo_engine_scaffold()
function demo_engine_scaffold()
  if Engine and Engine.Telemetry then
    Engine.Telemetry.record("demo_scaffold", 1)
    print("[idtech3_demo] telemetry demo_scaffold=" .. tostring(Engine.Telemetry.get("demo_scaffold")))
  end
  if Engine and Engine.Replay then
    print("[idtech3_demo] replay frame=" .. tostring(Engine.Replay.frameIndex()))
  end
  if Engine and Engine.Quest then
    Engine.Quest.add("demo_q1", "Scaffold quest", "active")
    Engine.Quest.setStage("demo_q1", "done")
    print("[idtech3_demo] quest stage=" .. tostring(Engine.Quest.getStage("demo_q1")))
  end
  if Engine and Engine.Dialogue then
    Engine.Dialogue.start("Guide", "Engine scaffolding check.")
    print("[idtech3_demo] dialogue lines=" .. tostring(Engine.Dialogue.count()))
  end
  if Engine and Engine.Save then
    Engine.Save.write(0, "demo_scaffold_save")
    local label = Engine.Save.read(0)
    print("[idtech3_demo] save slot 0=" .. tostring(label))
  end
end

local function demo_sp_slice()
  if Engine and Engine.AnimGraph then
    if Engine.AnimGraph.load("animgraph/idle_run.txt") then
      Engine.AnimGraph.setState("idle")
      print("[idtech3_demo] animgraph idle_run loaded")
    end
  end
  demo_engine_scaffold()
end

-- Console: lua_run demo_sp_slice()
demo_banner()
