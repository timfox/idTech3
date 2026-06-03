// Engine-native sprite prop demo shaders (misc_billboard / misc_flipbook / misc_imposter).
// Images: gfx/demo/demo_sprite_*.png (regenerate via gen_demo_bootstrap_media.py)

sprites/demo_billboard
{
	cull disable
	{
		map gfx/demo/demo_sprite_billboard.png
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}

sprites/demo_flipbook
{
	cull disable
	{
		map gfx/demo/demo_sprite_flipbook.png
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}

sprites/demo_imposter
{
	cull disable
	{
		map gfx/demo/demo_sprite_imposter.png
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}
