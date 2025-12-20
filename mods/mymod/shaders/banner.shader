// Enhanced Banner Shaders
// Professional banner materials with PBR support

// =============================================================================
// MAIN BANNER MATERIAL
// =============================================================================

textures/mapobjects/banner/banner_main
{
	qer_editorimage textures/mapobjects/banner/banner_main_d
	surfaceparm nonsolid
	surfaceparm nomarks
	cull disable

	// PBR material properties
	{
		map textures/mapobjects/banner/banner_main_d
		rgbGen vertex
		alphaGen vertex
	}
	{
		map textures/mapobjects/banner/banner_main_n
		tcGen base
	}
	{
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

// =============================================================================
// BANNER TRIM (METALLIC)
// =============================================================================

textures/mapobjects/banner/banner_trim
{
	qer_editorimage textures/mapobjects/banner/banner_trim_d
	surfaceparm nonsolid
	surfaceparm nomarks

	// Metallic trim with PBR properties
	{
		map textures/mapobjects/banner/banner_trim_d
		rgbGen identity
	}
	{
		map textures/mapobjects/banner/banner_trim_n
		tcGen base
	}
	{
		map textures/mapobjects/banner/banner_trim_m
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/mapobjects/banner/banner_trim_r
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
	{
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

// =============================================================================
// BANNER LOGO (EMISSIVE)
// =============================================================================

textures/mapobjects/banner/banner_logo
{
	qer_editorimage textures/mapobjects/banner/banner_logo_d
	surfaceparm nonsolid
	surfaceparm nomarks
	surfaceparm nolightmap

	// Emissive logo with glow effect
	{
		map textures/mapobjects/banner/banner_logo_d
		rgbGen identity
	}
	{
		map textures/mapobjects/banner/banner_logo_e
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.7 0.3 0 1.0  // Pulsing glow
		tcGen base
	}
	{
		map textures/mapobjects/banner/banner_logo_e
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.3 0.2 0.5 0.8  // Secondary glow
		tcMod scroll 0.1 0
		tcGen base
	}
}

// =============================================================================
// BANNER GLOW EFFECT
// =============================================================================

textures/mapobjects/banner/banner_glow
{
	qer_editorimage textures/mapobjects/banner/banner_glow_d
	surfaceparm nonsolid
	surfaceparm nomarks
	surfaceparm trans
	surfaceparm nolightmap
	cull disable

	// Soft glow effect around banner
	{
		map textures/mapobjects/banner/banner_glow_d
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen wave sin 0.2 0.8 0 0.7
		tcMod scroll 0 0.05
	}
	{
		map textures/mapobjects/banner/banner_glow_d
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen wave sin 0.8 0.2 0.3 0.9
		tcMod scroll 0.03 0.08
		tcMod scale 0.8 0.8
	}
}

// =============================================================================
// METALLIC BANNER ELEMENTS
// =============================================================================

textures/mapobjects/banner/banner_metal
{
	qer_editorimage textures/mapobjects/banner/banner_metal_d
	surfaceparm nonsolid
	surfaceparm nomarks

	// High-quality metallic material
	{
		map textures/mapobjects/banner/banner_metal_d
		rgbGen identity
	}
	{
		map textures/mapobjects/banner/banner_metal_n
		tcGen base
	}
	{
		map textures/mapobjects/banner/banner_metal_m
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/mapobjects/banner/banner_metal_r
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
	{
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

// =============================================================================
// ANIMATED BANNER EFFECTS
// =============================================================================

textures/mapobjects/banner/banner_animated
{
	qer_editorimage textures/mapobjects/banner/banner_anim_d
	surfaceparm nonsolid
	surfaceparm nomarks
	surfaceparm nolightmap

	// Animated banner with flowing effects
	{
		map textures/mapobjects/banner/banner_anim_d
		rgbGen identity
	}
	{
		animMap 2 textures/mapobjects/banner/banner_anim1 textures/mapobjects/banner/banner_anim2 textures/mapobjects/banner/banner_anim3
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.3 0.7 0 0.5
		tcMod scroll 0.1 0.05
	}
}

// =============================================================================
// PARTICLE BANNER EFFECTS
// =============================================================================

banner_particles
{
	{
		map textures/mapobjects/banner/banner_particle
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

// =============================================================================
// BANNER ENVIRONMENT MAPPING
// =============================================================================

textures/mapobjects/banner/banner_env
{
	qer_editorimage textures/mapobjects/banner/banner_env_d
	surfaceparm nonsolid
	surfaceparm nomarks

	// Environment mapped banner
	{
		map textures/mapobjects/banner/banner_env_d
		rgbGen identity
	}
	{
		map textures/mapobjects/banner/banner_env
		tcGen environment
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
		alphaGen const 0.3
	}
	{
		map $lightmap
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}

// =============================================================================
// UTILITY BANNERS
// =============================================================================

// Invisible banner for optimization
textures/mapobjects/banner/banner_invisible
{
	surfaceparm nodraw
	surfaceparm nonsolid
	surfaceparm trans
	{
		map *white
		blendFunc GL_ZERO GL_ONE
		rgbGen const ( 0 0 0 )
	}
}

// Debug banner showing wireframe
textures/mapobjects/banner/banner_debug
{
	qer_editorimage textures/mapobjects/banner/banner_debug_d
	surfaceparm nonsolid
	surfaceparm nomarks
	polygonOffset
	{
		map *white
		rgbGen const ( 1 1 1 )
		alphaGen const 0.3
	}
}
