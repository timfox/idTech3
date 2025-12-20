//Shaderfile for md3 models

models/mapobjects/cosmoflash/teleporter3{
	{
		map textures/cosmo_sfx/pulse.jpg
        tcMod scroll 0 1
        tcMod stretch sin 1 0.8 1 0.4
	}
	{
		map textures/cosmo_sfx/stoerung.jpg
		blendFunc GL_ONE GL_ONE
        rgbgen wave sin .25 0.1 0 0.1
        tcMod scroll 0 10
	}	
    {
		map textures/cosmo_sfx/stoerung.jpg
		blendFunc GL_ONE GL_ONE
        rgbgen wave sin 0.25 0.1 0 0.1
        tcMod scale  -1 1
        tcMod scroll 0 -5
	}
    {
        map models/mapobjects/cosmoflash/teleporter3.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		alphaFunc GT0
		depthWrite
		rgbGen identity
	}
	{
		map $lightmap 
//		blendfunc gl_dst_color gl_one_minus_dst_alpha
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}


models/mapobjects/cosmoflash/torch1{

    {
        map models/mapobjects/cosmoflash/torch1.tga
		rgbGen identity
	}
	
	{		map $lightmap 		blendfunc filter		rgbGen identity	}
}



models/mapobjects/weaponpod/weaponpod{    surfaceparm metalsteps

    {
        map textures/base_floor/clang_floor_s2.jpg
		rgbGen identity
	}
	
	{		map $lightmap 		blendfunc filter		rgbGen identity	}
}
