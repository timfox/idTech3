
textures/cosmo_sfx/flag_scimitar_brown1
{
        qer_editorimage textures/cosmo_sfx/flag_scimitar_brown1.tga
        surfaceparm nomarks
        surfaceparm alphashadow
        cull none
        
    {
        map textures/cosmo_sfx/flag_scimitar_brown1.tga
        blendFunc GL_ONE GL_ZERO
        alphaFunc GE128
        depthWrite
		rgbGen identity
	}
    {
		map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
        depthFunc equal
		rgbGen identity
	}

}

textures/cosmo_sfx/flag_oa_brown1
{
        qer_editorimage textures/cosmo_sfx/flag_oa_brown1.tga
        surfaceparm nomarks
        surfaceparm alphashadow
        cull none
        
    {
        map textures/cosmo_sfx/flag_oa_brown1.tga
        blendFunc GL_ONE GL_ZERO
        alphaFunc GE128
		rgbGen identity
	}
    {
		map $lightmap
		rgbGen identity
		blendFunc GL_DST_COLOR GL_ZERO
		depthFunc equal
	}
}

textures/cosmo_sfx/flag_oa_red1
{
        qer_editorimage textures/cosmo_sfx/flag_oa_red1.tga
        surfaceparm nomarks
        surfaceparm alphashadow
        cull none
        
    {
        map textures/cosmo_sfx/flag_oa_red1.tga
        blendFunc GL_ONE GL_ZERO
        alphaFunc GE128
		rgbGen identity
	}
    {
		map $lightmap
		rgbGen identity
		blendFunc GL_DST_COLOR GL_ZERO
		depthFunc equal
	}
}

textures/cosmo_sfx/flag_oa_blue1
{
        qer_editorimage textures/cosmo_sfx/flag_oa_blue1.tga
        surfaceparm nomarks
        surfaceparm alphashadow
        cull none
        
    {
        map textures/cosmo_sfx/flag_oa_blue1.tga
        blendFunc GL_ONE GL_ZERO
        alphaFunc GE128
		rgbGen identity
	}
    {
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		depthFunc equal
		rgbGen identity
	}
}

textures/cosmo_skies/killsky_r
{
	qer_editorimage textures/skies/killsky_1.jpg
	q3map_lightimage textures/skies/skywater.tga
	surfaceparm nomarks
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm sky
	q3map_surfacelight 200
	q3map_globaltexture
	q3map_sun	1 1 1  200  305  70
	skyParms full 512 -
	{
		map textures/skies/intelredclouds.jpg
		tcmod scale 2 3
		tcmod scroll -.01 .02
	}
	{
		map textures/skies/killsky_1.jpg
		blendfunc filter
		tcmod scale 3 2
		tcmod scroll 0.05 0.03
	}
}

textures/cosmo_skies/foghullsky1
{
	qer_editorimage textures/cosmo_sfx/fog_or.jpg
	surfaceparm noimpact
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm sky

	q3map_sun .5 .37 .19 130 30 65
	q3map_surfacelight 270
	skyparms - 512 -

	{
		map textures/cosmo_sfx/fog_or.jpg
		depthWrite
	}
}

textures/cosmo_skies/foghull_or
{
    qer_editorimage textures/cosmo_sfx/fog_or.jpg
    qer_nocarve
    surfaceparm	trans
    surfaceparm	nonsolid
    surfaceparm	nolightmap
    surfaceparm	fog
    fogparms ( .76 .56 .16 ) 2048

}

textures/cosmo_sfx/blood1_decal{	qer_editorimage textures/cosmo_sfx/blood1.tga	nopicmip
    qer_trans 0.75
    q3map_nonplanar 
    q3map_shadeAngle 179 
    q3map_noTJunc 
    surfaceparm nonsolid 
    surfaceparm nomarks 
    surfaceparm pointlight 
    surfaceparm trans 
    polygonOffset

    {
        map textures/cosmo_sfx/blood1.tga
        blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

textures/cosmo_sfx/blood2_decal{	qer_editorimage textures/cosmo_sfx/blood2.tga	nopicmip
    qer_trans 0.75
    q3map_nonplanar 
    q3map_shadeAngle 179 
    q3map_noTJunc 
    surfaceparm nonsolid 
    surfaceparm nomarks 
    surfaceparm pointlight 
    surfaceparm trans 
    polygonOffset

    {
        map textures/cosmo_sfx/blood2.tga
        blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
    {
		map $lightmap
		rgbGen identity
		blendFunc GL_DST_COLOR GL_ZERO
		depthFunc equal
	}
}

textures/cosmo_sfx/chains01
{
        qer_editorimage textures/cosmo_sfx/chains01.tga
        surfaceparm nomarks
        surfaceparm alphashadow
        deformVertexes autosprite2
        cull none
        
    {
        map textures/cosmo_sfx/chains01.tga
        blendFunc GL_ONE GL_ZERO
        alphaFunc GE128
        depthWrite
		rgbGen identity
	}
    {
		map $lightmap
		rgbGen identity
		blendFunc GL_DST_COLOR GL_ZERO
		depthFunc equal
	}
}

textures/cosmo_light/window1
{
	qer_editorimage textures/cosmo_light/window1.tga
	surfaceparm nomarks
    q3map_lightimage textures/cosmo_light/window1_l.tga
	q3map_surfacelight 400
	{
		map textures/cosmo_light/window1.tga
        blendFunc GL_ONE GL_ZERO
        alphaFunc GE128
        depthWrite
		rgbgen identity
	}
    {
     	map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
        depthFunc equal
		rgbGen identity
    }
}


textures/cosmo_light/dmask1_red
{
	qer_editorimage textures/cosmo_light/dmask1.tga
	surfaceparm nomarks
    q3map_lightimage textures/cosmo_light/dmask1_r.tga
	q3map_surfacelight 2000

	{
		map textures/cosmo_light/dmask1.tga
		blendfunc gl_one gl_zero
		rgbgen identity
	}
	{
		map $lightmap
		blendfunc filter
		tcgen lightmap
	}
	{
		map textures/cosmo_light/dmask1_r.tga
		blendfunc add
		rgbGen wave sin 0.25 0.25 0 .25
	}
}

textures/cosmo_light/dmask1_blue
{
	qer_editorimage textures/cosmo_light/dmask1.tga
	surfaceparm nomarks
    q3map_lightimage textures/cosmo_light/dmask1_b.tga
	q3map_surfacelight 2000

	{
		map textures/cosmo_light/dmask1.tga
		blendfunc gl_one gl_zero
		rgbgen identity
	}
	{
		map $lightmap
		blendfunc filter
		tcgen lightmap
	}
	{
		map textures/cosmo_light/dmask1_b.tga
		blendfunc add
		rgbGen wave sin 0.25 0.25 0 .25
	}
}


textures/cosmo_floor/sand01
{
    surfaceparm nodamage
    surfaceparm nosteps
    surfaceparm dust
    q3map_lightmapsamplesize 0.1
    q3map_nonplanar    q3map_shadeangle 80

    {        map $lightmap        rgbGen identity    }
    {        map textures/cosmo_floor/sand01.jpg
        blendFunc GL_DST_COLOR GL_ZERO        rgbGen identity    }

}

textures/cosmo_trim/ctf_wall_red1
{
	qer_editorimage textures/cosmo_trim/ctf_wall_red1.jpg
	surfaceparm nomarks
	{
		map textures/cosmo_trim/ctf_wall_red1.jpg
		rgbgen identity
	}
    {
     	map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
    }
	{
		map textures/cosmo_trim/flashyspots1.jpg
		blendfunc add
		rgbGen wave sin 0.25 0.25 0 .05
	}
}


textures/cosmo_trim/ctf_wall_blue1
{
	qer_editorimage textures/cosmo_trim/ctf_wall_blue1.jpg
	surfaceparm nomarks
	{
		map textures/cosmo_trim/ctf_wall_blue1.jpg
		rgbgen identity
	}
    {
     	map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
    }
	{
		map textures/cosmo_trim/flashyspots1.jpg
		blendfunc add
		rgbGen wave sin 0.25 0.25 0 .05
	}
}


textures/cosmo_liquids/plasma_red
{
	
	q3map_globaltexture
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm noimpact
	surfaceparm lava
	surfaceparm nolightmap
	q3map_surfacelight 1250
	cull disable
	
	tesssize 128
	cull disable
	deformVertexes wave 100 sin 3 2 .1 0.1
	
    {
        map textures/cosmo_liquids/plasma_red.jpg
        rgbGen identity
	}

}

textures/cosmo_trim/darkgreen02
{
        qer_editorimage textures/cosmo_trim/darkgreen02.tga
        tessSize 64
        surfaceparm nomarks
        cull none
        
    {
        map textures/cosmo_trim/darkgreen02.tga
        blendFunc GL_ONE GL_ZERO
        alphaFunc GE128
        depthWrite
		rgbGen identity
	}
    {
		map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
        depthFunc equal
		rgbGen identity
	}

}

textures/cosmo_light/lightyel02_12k
{
	qer_editorimage textures/cosmo_light/lightyel02.jpg
	surfaceparm nomarks
	q3map_surfacelight 12000
	{
		map textures/cosmo_light/lightyel02.jpg
		rgbgen identity
	}
	{
		map $lightmap
        blendFunc filter
		tcgen lightmap
	}
}

textures/cosmo_light/lightyel03_5k
{
	qer_editorimage textures/cosmo_light/lightyel03.jpg
	surfaceparm nomarks
	q3map_surfacelight 5000
	{
		map textures/cosmo_light/lightyel03_f.jpg
		tcMod scroll 0 0.1
		rgbgen identity
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
		map $lightmap
		blendfunc filter
		tcgen lightmap
	}
	{
		map textures/cosmo_light/lightyel03.tga
        blendFunc blend
		rgbGen identity
	}
}

textures/cosmo_wall/grgr_wall2
{
	qer_editorimage textures/cosmo_wall/grgr_wall2.tga
//	surfaceparm nomarks
	q3map_surfacelight 175
	{
		map textures/cosmo_sfx/pulse.jpg
		tcMod scroll 0 -0.65
		rgbgen identity
	}
	{
		map textures/cosmo_wall/grgr_wall2.tga
        blendFunc blend
		rgbGen identity
	}

	{
		map $lightmap
		blendfunc filter
		tcgen lightmap
	}

}

textures/cosmo_light/wall_light1
{
	qer_editorimage textures/cosmo_light/wall_light1.jpg
    q3map_lightimage textures/cosmo_light/wall_light1_m.jpg
	q3map_surfacelight 1600
	{
		map textures/cosmo_light/wall_light1.jpg
        blendFunc GL_ONE GL_ZERO
		rgbgen identity
	}
    {
     	map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
    }
}

textures/cosmo_light/wall_light2
{
	qer_editorimage textures/cosmo_light/wall_light2.jpg
	q3map_surfacelight 1600
	{
		map textures/cosmo_light/wall_light2.jpg
        blendFunc GL_ONE GL_ZERO
		rgbgen identity
	}
    {
     	map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
    }
}

textures/cosmo_floor/metalgrid
{
	qer_editorimage textures/cosmo_floor/metalgrid.tga
	cull none
	surfaceparm nomarks
//    surfaceparm noimpact
	nopicmip
	
	{
		map textures/cosmo_floor/metalgrid.tga
        blendFunc GL_DST_COLOR GL_ZERO
        alphaFunc GE128
		rgbGen identity
	}
	{
		map $lightmap
		blendfunc filter
		tcgen lightmap
	}

}

textures/cosmo_sfx/jumppad
{
	qer_editorimage textures/cosmo_sfx/jumppad.tga

	{
		map textures/cosmo_sfx/jumppad.tga
		blendfunc gl_one gl_zero
		rgbgen identity
	}
	{
		map $lightmap
		blendfunc filter
		tcgen lightmap
	}
	{
		map textures/cosmo_sfx/jumppad_b.jpg
		blendfunc add
		rgbGen wave sin 0.25 0.25 0 .25
	}
}

textures/cosmo_sfx/jumppad2
{
	qer_editorimage textures/cosmo_sfx/jumppad2.tga
    q3map_lightimage textures/cosmo_sfx/jumppad2_b.jpg
	q3map_surfacelight 600

	{
		map textures/cosmo_sfx/jumppad2.tga
		rgbgen identity
	}
	{
		map textures/cosmo_sfx/jumppad2_b.jpg
		blendfunc GL_ONE GL_ONE
		tcMod stretch triangle 2 1 0 0.7
		rgbGen wave sin 0.25 0.25 0 0.7
	}
	{
		map textures/cosmo_sfx/jumppad2.tga
		blendfunc blend
		rgbgen identity
	}
	{
		map $lightmap
		blendFunc filter
		tcgen lightmap
	}

}

textures/cosmo_sfx/teleporter
{
    qer_editorimage textures/cosmo_sfx/teleporter.jpg
    surfaceparm nomarks
    surfaceparm alphashadow
    deformVertexes normal 0.1 1
        
    {
        map textures/cosmo_sfx/teleporter.jpg
        tcGen environment
        rgbGen identity
    }
    {
		map textures/cosmo_sfx/pulse.jpg
		tcMod scroll 0 -0.65
		blendfunc GL_ONE GL_ONE
		rgbgen identity
	}

    
}

textures/cosmo_sfx/decal1{	qer_editorimage textures/cosmo_sfx/decal1.tga	nopicmip
    polygonOffset

    {
        map textures/cosmo_sfx/decal1.tga
        blendFunc blend
		rgbGen identity
	}
}

textures/cosmo_sfx/decal2{	qer_editorimage textures/cosmo_sfx/decal2.tga	nopicmip
    polygonOffset

    {
        map textures/cosmo_sfx/decal2.tga
        blendFunc blend
		rgbGen identity
	}
}

textures/cosmo_sfx/decal3{	qer_editorimage textures/cosmo_sfx/decal3.tga	nopicmip
    polygonOffset

    {
        map textures/cosmo_sfx/decal3.tga
        blendFunc blend
		rgbGen identity
	}
}

textures/cosmo_sfx/decal4{	qer_editorimage textures/cosmo_sfx/decal4.tga	nopicmip
    polygonOffset

    {
        map textures/cosmo_sfx/decal4.tga
        blendFunc blend
		rgbGen identity
	}
}

textures/cosmo_skies/tim_tourney2
{
	qer_editorimage textures/skies/s18.tga
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm sky
	q3map_lightimage textures/skies/s17.tga
	q3map_surfacelight 100
	q3map_sun .5 .5 .5 120 65 90
	skyparms - 1024 -
	{
		map textures/skies/s18.tga
		tcmod scale 2 3
		tcmod scroll 0.03 0.03
	}
	{
		map textures/skies/s14.tga
		blendfunc add
		tcmod scale 3 2
		tcmod scroll 0.015 0.015
	}
}

textures/cosmo_liquids/greenliqu1{	qer_editorimage textures/cosmo_liquids/greenliqu1.jpg
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm slime
    q3map_surfacelight 1750
	deformVertexes bulge 4 2 0.75

    {
        map textures/cosmo_liquids/greenliqu1.jpg
		rgbGen identity
		tcmod scroll 0.03 0.25
	}
}

textures/cosmo_sfx/dm17_jpad
{
        qer_editorimage textures/cosmo_sfx/e8jumpspawn02red.tga
        surfaceparm nomarks
        surfaceparm metalsteps        q3map_surfacelight 15000
//        q3map_lightimage textures/sfx/jc2.jpg
	{		clampmap textures/sfx/jc2.jpg		tcMod stretch sin 1.2 .8 0 1.5
		rgbGen wave square .5 .5 .25 1.5
	}    {
        map textures/cosmo_sfx/e8jumpspawn02red.tga
		blendfunc blend
		rgbGen identity	}	{		map $lightmap 		blendfunc gl_dst_color gl_one_minus_dst_alpha		rgbGen identity		tcGen lightmap 	}
}

textures/base_floor/clang_floor_s2{	qer_editorimage textures/base_floor/clang_floor_s2.jpg
    surfaceparm metalsteps

    {
        map textures/base_floor/clang_floor_s2.jpg
		rgbGen identity
	}
	
	{		map $lightmap 		blendfunc filter		rgbGen identity	}
}

textures/base_floor/clang_decal2{	qer_editorimage textures/base_floor/clang_decal2.tga	nopicmip
    polygonOffset
    surfaceparm metalsteps

    {
        map textures/base_floor/clang_decal2.tga
        blendFunc blend
		rgbGen identity
	}
}


textures/evil6_lights/e6v_light_s_3k{	qer_editorimage textures/evil6_lights/e6v_light_s_nolight.jpg
    surfaceparm nomarks
    q3map_surfacelight 13000
	
    {
        map textures/evil6_lights/e6v_light_s_nolight.jpg
		rgbgen identity
	}
    {
     	map $lightmap
//        blendFunc GL_DST_COLOR GL_ZERO
        blendFunc filter
		rgbGen identity
    }
}

textures/cosmo_light/border7_ceil39_6k{	qer_editorimage textures/gothic_light/border7_ceil39.jpg
    q3map_lightimage textures/gothic_light/border7_ceil39blend.jpg
    q3map_surfacelight 6000
	
    {
        map textures/gothic_light/border7_ceil39.jpg
		rgbgen identity
	}
    {
     	map $lightmap
        blendFunc filter
		rgbGen identity
    }
}


textures/cosmo_sfx/diamond_blue
{
	qer_editorimage textures/cosmo_sfx/diamond.blue.tga
	surfaceparm nomarks
	surfaceparm nonsolid
	q3map_surfacelight 4500
	{
		map textures/cosmo_sfx/diamond.blue.tga
		blendFunc GL_ONE GL_ZERO
	}
    {
		map textures/base_trim/tinfx.jpg
		tcGen environment
		blendfunc add
		rgbGen vertex
	}
}


textures/cosmo_sfx/chalkdoor_d{	qer_editorimage textures/cosmo_sfx/chalkdoor.tga	nopicmip
    polygonOffset

    {
        map textures/cosmo_sfx/chalkdoor.tga
        blendFunc blend
		rgbGen identity
	}
}

textures/cosmo_light/ironcrosslt1_3k
{
	qer_editorimage textures/cosmo_light/ironcrosslt1.tga
	surfaceparm nomarks
    q3map_lightimage textures/cosmo_light/ironcrosslt1.blend.tga
	q3map_surfacelight 3400
	{
		map textures/cosmo_light/ironcrosslt1.tga
        blendFunc GL_ONE GL_ZERO
		rgbgen identity
	}
    {
     	map $lightmap
        blendFunc GL_DST_COLOR GL_ZERO
        depthFunc equal
		rgbGen identity
    }
}

textures/cosmo_light/crystalwhite_5k
{
	qer_editorimage textures/cosmo_sfx/diamond.white.tga
	surfaceparm nomarks
    q3map_lightimage textures/cosmo_sfx/diamond.white.tga
	q3map_surfacelight 7500
	{
		map textures/cosmo_sfx/diamond.white.tga
        blendFunc GL_ONE GL_ZERO
		rgbgen identity
	}
    {
		map textures/base_trim/tinfx.jpg
		tcGen environment
		blendfunc add
		rgbGen vertex
	}
}



/* DETAILS */
textures/cosmo_floor/sand02
{
	{
		map textures/cosmo_floor/sand02.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_sand.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 4 4
		detail
	}
	{
		map textures/detail/d_sandy.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 16 16
		detail
	}
}

textures/cosmo_floor/sand01
{
	{
		map textures/cosmo_floor/sand01.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_sand.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 4 4
		detail
	}
	{
		map textures/detail/d_sandy.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 16 16
		detail
	}
}

textures/cosmo_block/ancient_bricks2
{
	{
		map textures/cosmo_block/ancient_bricks2.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_sandy.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_block/ancient_bricks3
{
	{
		map textures/cosmo_block/ancient_bricks3.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_sandy.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_block/ancient_bricks4
{
	{
		map textures/cosmo_block/ancient_bricks4.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_sandy.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_block/ancient_bricks6
{
	{
		map textures/cosmo_block/ancient_bricks6.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_sandy.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_block/rock01
{
	{
		map textures/cosmo_block/rock01.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_stone.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_block/rock02
{
	{
		map textures/cosmo_block/rock02.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_stone.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_block/rock05
{
	{
		map textures/cosmo_block/rock05.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_stone.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_block/rock06
{
	{
		map textures/cosmo_block/rock06.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_stone.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}

textures/cosmo_floor/bfloor3
{
	{
		map textures/cosmo_floor/bfloor3.tga
		rgbGen identity
	}
	{
		map $lightmap 
		blendfunc filter
	}
	{
		map textures/detail/d_sandy.tga
		blendfunc gl_dst_color gl_src_color
		rgbGen identity
		tcMod scale 2 2
		detail
	}
}
