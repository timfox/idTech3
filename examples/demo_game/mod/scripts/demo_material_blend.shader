// Multi-material PBR height-blend demo (vertex RGBA weights + NH alpha).
// Textures: textures/demo/* under bootstrap_media (packed into demo pk3).

textures/demo/blend_ground
{
	{
		materialBlend vertex
		blendSharpness 8
		map textures/demo/dirt.png
		normalHeightMap textures/demo/dirt_nh.png
		ormMap textures/demo/dirt_orm.png
		layerMap 1 textures/demo/rock.png
		layerNormalHeightMap 1 textures/demo/rock_nh.png
		layerOrmMap 1 textures/demo/rock_orm.png
	}
}
