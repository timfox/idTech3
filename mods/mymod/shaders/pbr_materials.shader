// Enhanced PBR Materials for MyMod
// Demonstrates physically based rendering capabilities

// =============================================================================
// METALLIC MATERIALS
// =============================================================================

// High-quality gold material with PBR properties
textures/pbr/gold
{
	qer_editorimage textures/pbr/gold_d
	{
		map textures/pbr/gold_d
		rgbGen identity
	}
	{
		map textures/pbr/gold_n
		tcGen base
	}
	{
		map textures/pbr/gold_m
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/pbr/gold_r
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

// Silver material with high reflectivity
textures/pbr/silver
{
	qer_editorimage textures/pbr/silver_d
	{
		map textures/pbr/silver_d
		rgbGen identity
	}
	{
		map textures/pbr/silver_n
		tcGen base
	}
	{
		map textures/pbr/silver_m
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/pbr/silver_r
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

// Copper with oxidation effects
textures/pbr/copper
{
	qer_editorimage textures/pbr/copper_d
	{
		map textures/pbr/copper_d
		rgbGen identity
	}
	{
		map textures/pbr/copper_n
		tcGen base
	}
	{
		map textures/pbr/copper_m
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/pbr/copper_r
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

// =============================================================================
// DIELECTRIC MATERIALS (NON-METALLIC)
// =============================================================================

// High-quality marble with subsurface scattering
textures/pbr/marble
{
	qer_editorimage textures/pbr/marble_d
	surfaceparm nonsolid
	{
		map textures/pbr/marble_d
		rgbGen identity
	}
	{
		map textures/pbr/marble_n
		tcGen base
	}
	{
		map textures/pbr/marble_r
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

// Glass with refraction and reflection
textures/pbr/glass
{
	qer_editorimage textures/pbr/glass_d
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nomarks
	cull disable
	{
		map textures/pbr/glass_d
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen identity
		tcGen environment
	}
	{
		map textures/pbr/glass_n
		tcGen base
	}
	{
		map textures/pbr/glass_r
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

// Wood with realistic grain
textures/pbr/wood
{
	qer_editorimage textures/pbr/wood_d
	{
		map textures/pbr/wood_d
		rgbGen identity
	}
	{
		map textures/pbr/wood_n
		tcGen base
	}
	{
		map textures/pbr/wood_r
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
// SPECIAL EFFECT MATERIALS
// =============================================================================

// Emissive material for glowing effects
textures/pbr/emissive_blue
{
	qer_editorimage textures/pbr/emissive_d
	surfaceparm nolightmap
	{
		map textures/pbr/emissive_d
		rgbGen identity
	}
	{
		map textures/pbr/emissive_e
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.5 0.5 0 0.5
		tcGen base
	}
}

// Holographic material with animated effects
textures/pbr/hologram
{
	qer_editorimage textures/pbr/hologram_d
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nomarks
	cull disable
	{
		map textures/pbr/hologram_d
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen wave sin 0 1 0 1
		tcMod scroll 0 0.1
	}
	{
		map textures/pbr/hologram_n
		tcGen base
	}
}

// =============================================================================
// ENVIRONMENT MATERIALS
// =============================================================================

// PBR water with reflections
textures/pbr/water
{
	qer_editorimage textures/pbr/water_d
	surfaceparm nonsolid
	surfaceparm water
	surfaceparm trans
	cull disable
	deformVertexes wave 64 sin 0 3 0 0.5
	{
		map textures/pbr/water_d
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
		tcMod scroll 0.05 0.05
	}
	{
		map textures/pbr/water_n
		tcGen base
	}
	{
		map textures/pbr/water_r
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

// PBR lava with emissive properties
textures/pbr/lava
{
	qer_editorimage textures/pbr/lava_d
	surfaceparm nonsolid
	surfaceparm lava
	surfaceparm trans
	cull disable
	deformVertexes wave 64 sin 0 2 0 0.5
	{
		map textures/pbr/lava_d
		rgbGen identity
		tcMod scroll 0.1 0.1
	}
	{
		map textures/pbr/lava_e
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.5 0.5 0 0.5
		tcGen base
	}
}

// =============================================================================
// ADVANCED MATERIAL FEATURES
// =============================================================================

// Material with parallax occlusion mapping
textures/pbr/stone_parallax
{
	qer_editorimage textures/pbr/stone_d
	{
		map textures/pbr/stone_d
		rgbGen identity
	}
	{
		map textures/pbr/stone_n
		tcGen base
	}
	{
		map textures/pbr/stone_h
		tcGen base
		// Parallax mapping would be handled by engine
	}
	{
		map textures/pbr/stone_r
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

// Material with subsurface scattering (skin/fabric)
textures/pbr/fabric
{
	qer_editorimage textures/pbr/fabric_d
	{
		map textures/pbr/fabric_d
		rgbGen identity
	}
	{
		map textures/pbr/fabric_n
		tcGen base
	}
	{
		map textures/pbr/fabric_sss
		tcGen base
		// Subsurface scattering handled by engine
	}
	{
		map textures/pbr/fabric_r
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

// =============================================================================
// MATERIAL VARIATIONS
// =============================================================================

// Rough metal variations
textures/pbr/metal_rough
{
	qer_editorimage textures/pbr/metal_d
	{
		map textures/pbr/metal_d
		rgbGen identity
	}
	{
		map textures/pbr/metal_n
		tcGen base
	}
	{
		map textures/pbr/metal_m_rough
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/pbr/metal_r_rough
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

// Smooth metal variations
textures/pbr/metal_smooth
{
	qer_editorimage textures/pbr/metal_d
	{
		map textures/pbr/metal_d
		rgbGen identity
	}
	{
		map textures/pbr/metal_n
		tcGen base
	}
	{
		map textures/pbr/metal_m_smooth
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/pbr/metal_r_smooth
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen identity
	}
}

// =============================================================================
// UTILITY MATERIALS
// =============================================================================

// Debug material showing PBR channels
textures/pbr/debug_channels
{
	qer_editorimage textures/pbr/debug_d
	{
		map textures/pbr/debug_albedo
		rgbGen identity
	}
	{
		map textures/pbr/debug_normal
		tcGen base
	}
	{
		map textures/pbr/debug_metalness
		tcGen base
		blendFunc GL_DST_COLOR GL_ZERO
	}
	{
		map textures/pbr/debug_roughness
		tcGen base
		blendFunc GL_SRC_ALPHA GL_ONE
	}
}

// Invisible material for optimization
textures/pbr/invisible
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
