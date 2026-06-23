// Dynamic guest display for in-world monitors (requires r_emulatorScreen 1 + cl_emulator 1).
emulator_screen
{
	nomipmaps
	nopicmip
	{
		map *emulator_screen
		rgbGen identity
		alphaGen identity
		blendFunc blend
	}
}
