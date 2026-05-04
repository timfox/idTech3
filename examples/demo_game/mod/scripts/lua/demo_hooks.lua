-- idtech3_demo: loaded via script_reload when engine is built with USE_LUA=ON.
-- Prints once at load; no gameplay impact.

local function demo_banner()
  print("[idtech3_demo] Lua demo_hooks.lua loaded (script_reload scripts/lua/demo_hooks.lua)")
end

demo_banner()
