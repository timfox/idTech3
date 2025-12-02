-- Event Bus Lua API Wrapper
-- Provides a cleaner interface for the Events system

Events = Events or {}

-- Subscribe to an event with optional filter
function Events.subscribe(event_name, callback, filter)
	if filter then
		-- Wrap callback with filter
		local wrapped = function(event_name, ...)
			if filter(...) then
				callback(event_name, ...)
			end
		end
		Events.on(event_name, wrapped)
	else
		Events.on(event_name, callback)
	end
end

-- Unsubscribe from an event
function Events.unsubscribe(event_name, callback)
	Events.off(event_name, callback)
end

-- Emit an event with data
function Events.publish(event_name, ...)
	Events.emit(event_name, ...)
end

