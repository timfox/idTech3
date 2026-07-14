-- OSCAR hybrid AIM demo: poll Engine.Oscar events (client or server Lua VM).
-- Requires oscar_enable 1, oscar_account, and IDTECH3_OSCAR_PASSWORD.
-- Usage: script_reload scripts/lua/demo_oscar_aim.lua

local function status_line()
	if not Engine or not Engine.Oscar then
		return "Engine.Oscar unavailable"
	end
	local s = Engine.Oscar.GetStatus()
	if type(s) ~= "table" then
		return tostring(Engine.Oscar.GetState())
	end
	return string.format(
		"state=%s room=%s buddies=%s err=%s",
		tostring(s.state),
		tostring(s.room ~= "" and s.room or "<none>"),
		tostring(s.buddyCount or Engine.Oscar.BuddyCount()),
		tostring(s.lastError ~= "" and s.lastError or "-")
	)
end

function demo_oscar_poll()
	if not Engine or not Engine.Oscar then
		print("OSCAR Lua: Engine.Oscar missing")
		return 0
	end
	local n = 0
	while true do
		local ev = Engine.Oscar.PollEvent()
		if not ev then
			break
		end
		n = n + 1
		if ev.type == "instant_message" then
			print(string.format("OSCAR IM <%s>: %s", ev.screenName or "?", ev.text or ""))
		elseif ev.type == "room_message" then
			print(string.format("OSCAR room %s <%s>: %s", ev.room or "?", ev.screenName or "?", ev.text or ""))
		elseif ev.type == "presence_changed" then
			print(string.format("OSCAR presence %s: %s", ev.screenName or "?", ev.status or ""))
		elseif ev.type == "connected" then
			print("OSCAR Lua: connected")
		elseif ev.type == "disconnected" then
			print(string.format("OSCAR Lua: disconnected (%s)", ev.text or ""))
		else
			print(string.format("OSCAR event: %s", tostring(ev.type)))
		end
	end
	return n
end

function demo_oscar_status()
	print("OSCAR Lua: " .. status_line())
	if Engine and Engine.Oscar and Engine.Oscar.BuddyCount then
		local count = Engine.Oscar.BuddyCount()
		for i = 0, count - 1 do
			local b = Engine.Oscar.GetBuddy(i)
			if b then
				print(string.format("  buddy[%d] %s %s%s",
					i, b.screenName or "?", b.status or "?",
					b.online and " (online)" or ""))
			end
		end
	end
end

print("demo_oscar_aim.lua loaded — demo_oscar_status() / demo_oscar_poll()")
demo_oscar_status()
