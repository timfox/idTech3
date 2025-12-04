-- Encounter DSL Helper
-- Provides higher-level encounter management

Encounter = Encounter or {}

-- Create a new encounter with wave system
function Encounter.create(name, config)
	local encounter = {
		name = name,
		waves = config.waves or {},
		on_start = config.on_start,
		on_complete = config.on_complete,
		on_fail = config.on_fail,
		current_wave = 0,
		enemies_alive = 0
	}
	
	-- Define encounter
	Encounter.define(name, {
		on_start = function()
			encounter.current_wave = 0
			encounter.enemies_alive = 0
			if encounter.on_start then
				encounter.on_start()
			end
		end,
		on_wave_spawn = function(wave_num)
			local wave = encounter.waves[wave_num]
			if wave and wave.spawn then
				wave.spawn()
			end
		end,
		on_enemy_killed = function(enemy)
			encounter.enemies_alive = encounter.enemies_alive - 1
			if encounter.enemies_alive <= 0 then
				-- Check if more waves
				if encounter.current_wave < #encounter.waves then
					encounter.current_wave = encounter.current_wave + 1
					Encounter.spawn_wave(name, encounter.current_wave)
				else
					-- All waves complete
					if encounter.on_complete then
						encounter.on_complete()
					end
				end
			end
		end,
		on_complete = encounter.on_complete,
		on_fail = encounter.on_fail
	})
	
	return encounter
end

-- Spawn a wave
function Encounter.spawn_wave(encounter_name, wave_num)
	-- This would trigger the on_wave_spawn callback
	-- Implementation depends on encounter system details
end

