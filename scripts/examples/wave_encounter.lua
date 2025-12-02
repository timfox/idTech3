-- Example Encounter: Wave Defense
-- Demonstrates encounter system with wave spawning

require("lib/encounter")

-- Define spawn points
local spawn_points = {
	{100, 200, 50},
	{200, 200, 50},
	{300, 200, 50},
	{400, 200, 50},
	{500, 200, 50}
}

-- Create encounter
Encounter.create("wave_defense", {
	waves = {
		{
			spawn = function()
				print("Wave 1 starting!")
				for i = 1, 5 do
					local x, y, z = spawn_points[i][1], spawn_points[i][2], spawn_points[i][3]
					local enemy = game_spawn_entity("enemy", x, y, z)
					if enemy >= 0 then
						game_entity_attach_script(enemy, "scripts/examples/enemy_behavior.lua")
					end
				end
			end
		},
		{
			spawn = function()
				print("Wave 2 starting!")
				for i = 1, 8 do
					local x, y, z = spawn_points[i % #spawn_points + 1][1], 
					                spawn_points[i % #spawn_points + 1][2], 
					                spawn_points[i % #spawn_points + 1][3]
					local enemy = game_spawn_entity("enemy", x, y, z)
					if enemy >= 0 then
						game_entity_attach_script(enemy, "scripts/examples/enemy_behavior.lua")
					end
				end
			end
		}
	},
	on_start = function()
		print("Wave defense encounter started!")
	end,
	on_complete = function()
		print("All waves defeated! Encounter complete!")
	end,
	on_fail = function()
		print("Encounter failed!")
	end
})

-- Start the encounter (can be called from level script or trigger)
-- Encounter.start("wave_defense")

