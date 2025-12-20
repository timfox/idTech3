-- Sequence DSL Helper
-- Provides higher-level sequence management

Sequence = Sequence or {}

-- Create a sequence with easier syntax
function Sequence.create(name, steps)
	local sequence_steps = {}
	
	for i, step in ipairs(steps) do
		table.insert(sequence_steps, {
			time = step.time or (i * 1.0),
			action = step.action or step
		})
	end
	
	Sequence.define(name, sequence_steps)
	return name
end

-- Play a sequence and return a promise-like object
function Sequence.play_async(name)
	Sequence.play(name)
	-- Return a simple object that can be awaited
	return {
		wait = function()
			-- Wait for sequence to complete
			-- This is a simplified version
		end
	}
end

