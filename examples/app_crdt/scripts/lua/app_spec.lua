--[[
  App CRDT example app (Wyns et al. Listing 3/6 lifecycle).
  Load via: app_crdt_publish 1.0.0 manifest.json
  (with +set fs_basepath examples/app_crdt)
]]

local app_state = {
  counter = 0,
  version = "1.0.0",
}

function on_hotload_destroy()
  return app_state
end

function on_hotload_create(previous)
  if type(previous) == "table" then
    app_state = previous
  end
  print("[app_spec] hotload create counter=" .. tostring(app_state.counter))
end

function on_app_crdt_message(fromMajor, payload)
  print("[app_spec] updateMessage from major " .. tostring(fromMajor) .. ": " .. tostring(payload))
end

function app_init()
  app_state.counter = app_state.counter + 1
  print("[app_spec] init counter=" .. tostring(app_state.counter))
end

app_init()
