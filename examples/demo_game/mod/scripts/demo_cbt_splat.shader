// CBT terrain splat ground (control map + up to 4 layers).

textures/demo/cbt_splat_ground
{
	{
		materialBlend splat
		blendSharpness 6
		splatMap textures/demo/cbt_control.png
		map textures/demo/dirt.png
		normalHeightMap textures/demo/dirt_nh.png
		ormMap textures/demo/dirt_orm.png
		layerMap 1 textures/demo/rock.png
		layerNormalHeightMap 1 textures/demo/rock_nh.png
		layerOrmMap 1 textures/demo/rock_orm.png
	}
}
