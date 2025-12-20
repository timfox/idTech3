-- Example Sequence: Intro Cinematic
-- Demonstrates timeline-based sequences

require("lib/sequence")

-- Define a cinematic sequence
Sequence.create("intro_cinematic", {
	{
		time = 0.0,
		action = function()
			print("Cinematic starting...")
			-- Camera.set_target(player)
		end
	},
	{
		time = 2.0,
		action = function()
			print("Welcome to the game!")
			Events.emit("dialog", "Welcome!")
		end
	},
	{
		time = 5.0,
		action = function()
			print("Restoring camera...")
			-- Camera.restore()
		end
	},
	{
		time = 7.0,
		action = function()
			print("Cinematic complete!")
		end
	}
})

-- Play the sequence (can be called from level script)
-- Sequence.play("intro_cinematic")

