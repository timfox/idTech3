-- Example Entity Script: Enemy Behavior
-- This script demonstrates entity lifecycle hooks

return {
	-- Called when entity is spawned
	OnSpawn = function(entity)
		print("Enemy spawned: " .. tostring(entity))
		-- Initialize enemy state
	end,
	
	-- Called every frame
	OnUpdate = function(entity, deltaTime)
		-- Update enemy AI, movement, etc.
		-- This is called every frame, so keep it lightweight
	end,
	
	-- Called when entity takes damage
	OnTakeDamage = function(entity, damage, attacker)
		print("Enemy " .. tostring(entity) .. " took " .. tostring(damage) .. " damage")
		-- Handle damage, play effects, etc.
		
		-- Emit event for other systems
		Events.emit("enemy_damaged", entity, damage, attacker)
	end,
	
	-- Called when entity is used/interacted with
	OnUse = function(entity, user)
		print("Enemy " .. tostring(entity) .. " used by " .. tostring(user))
		-- Handle interaction
	end,
	
	-- Called when entity dies
	OnDeath = function(entity)
		print("Enemy " .. tostring(entity) .. " died")
		-- Cleanup, spawn loot, etc.
		Events.emit("enemy_killed", entity)
	end
}

