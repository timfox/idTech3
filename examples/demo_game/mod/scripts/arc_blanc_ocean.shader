arc_blanc_ocean
{
	surfaceparm nonsolid
	surfaceparm trans
	cull disable
	{
		map *arc_blanc_height
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
		tcGen texture
	}
}
