// Bootstrap media for idtech3_demo.pk3 (empty base/): HUD charset, console tint,
// and shaders referenced during renderer init. Images live under gfx/ in the same pk3.

white
{
	nomipmaps
	nopicmip
	{
		map gfx/demo/bootstrap_white.png
		rgbGen identity
		alphaGen identity
		blendFunc blend
	}
}

console
{
	nomipmaps
	nopicmip
	{
		map gfx/demo/bootstrap_console.png
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}

gfx/2d/bigchars
{
	nomipmaps
	nopicmip
	{
		map gfx/2d/bigchars.png
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}

projectionShadow
{
	{
		map gfx/demo/bootstrap_shadow.png
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}

flareShader
{
	{
		map gfx/demo/bootstrap_flare.png
		rgbGen vertex
		alphaGen vertex
		blendFunc add
	}
}

fonts/demo_console_sdf
{
	nomipmaps
	nopicmip
	{
		map fonts/demo_console_sdf.png
		blendFunc blend
		rgbGen vertex
		alphaGen vertex
	}
}
