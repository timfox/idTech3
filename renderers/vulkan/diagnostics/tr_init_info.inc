/* Renderer information and device-reporting helpers.
 * Included by tr_init.c to keep static helper visibility unchanged. */

/*
================
R_PrintLongString

Workaround for ri.Printf's 1024 characters buffer limit.
================
*/
static void R_PrintLongString(const char *string) {
	char buffer[1024];
	const char *p;
	int size = strlen(string);

	p = string;
	while(size > 0)
	{
		Q_strncpyz(buffer, p, sizeof (buffer) );
		ri.Printf( PRINT_DEVELOPER, "%s", buffer );
		p += 1023;
		size -= 1023;
	}
}


/*
================
GfxInfo

Prints persistent rendering configuration
================
*/
static void GfxInfo( void )
{
	const char *fsstrings[] = { "windowed", "fullscreen" };
	const char *fs;
	int mode;
	ri.Printf( PRINT_ALL, "\nVK_VENDOR: %s\n", glConfig.vendor_string );
	ri.Printf( PRINT_ALL, "VK_RENDERER: %s\n", glConfig.renderer_string );
	ri.Printf( PRINT_ALL, "VK_VERSION: %s\n", glConfig.version_string );

	ri.Printf( PRINT_DEVELOPER, "VK_EXTENSIONS: " );
	R_PrintLongString( glConfig.extensions_string );

	ri.Printf( PRINT_ALL, "\nVK_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	ri.Printf( PRINT_ALL, "VK_MAX_TEXTURE_UNITS: %d\n", glConfig.numTextureUnits );

	ri.Printf( PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
	ri.Printf( PRINT_ALL, " presentation: %s\n", vk_format_string( vk.present_format.format ) );
	if ( vk.color_format != vk.present_format.format ) {
		ri.Printf( PRINT_ALL, " color: %s\n", vk_format_string( vk.color_format ) );
	}
	if ( vk.capture_format != vk.present_format.format || vk.capture_format != vk.color_format ) {
		ri.Printf( PRINT_ALL, " capture: %s\n", vk_format_string( vk.capture_format ) );
	}
	ri.Printf( PRINT_ALL, " depth: %s\n", vk_format_string( vk.depth_format ) );
	if ( glConfig.isFullscreen )
	{
		const char *modefs = ri.Cvar_VariableString( "r_modeFullscreen" );
		if ( *modefs )
			mode = atoi( modefs );
		else
			mode = ri.Cvar_VariableIntegerValue( "r_mode" );
		fs = fsstrings[1];
	}
	else
	{
		mode = ri.Cvar_VariableIntegerValue( "r_mode" );
		fs = fsstrings[0];
	}

	if ( glConfig.vidWidth != gls.windowWidth || glConfig.vidHeight != gls.windowHeight )
	{
		ri.Printf( PRINT_ALL, "RENDER: %d x %d, MODE: %d, %d x %d %s hz:", glConfig.vidWidth, glConfig.vidHeight, mode, gls.windowWidth, gls.windowHeight, fs );
	}
	else
	{
		ri.Printf( PRINT_ALL, "MODE: %d, %d x %d %s hz:", mode, gls.windowWidth, gls.windowHeight, fs );
	}

	if ( glConfig.displayFrequency )
	{
		ri.Printf( PRINT_ALL, "%d\n", glConfig.displayFrequency );
	}
	else
	{
		ri.Printf( PRINT_ALL, "N/A\n" );
	}

}


/*
================
VarInfo

Prints info that may change every R_Init() call
================
*/
static void VarInfo( void )
{
	if ( glConfig.deviceSupportsGamma ) {
		ri.Printf( PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
	} else {
		ri.Printf( PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
	}

	ri.Printf( PRINT_ALL, "texturemode: %s\n", r_textureMode ? r_textureMode->string : "GL_LINEAR_MIPMAP_NEAREST" );
	ri.Printf( PRINT_ALL, "mip LOD bias: %.2f\n", r_mipLodBias ? r_mipLodBias->value : 0.0f );
	ri.Printf( PRINT_ALL, "texture bits: %d\n", r_texturebits ? (r_texturebits->integer ? r_texturebits->integer : 32) : 32 );
	ri.Printf( PRINT_ALL, "picmip: %d%s\n", r_picmip ? r_picmip->integer : 0, (r_nomip && r_nomip->integer) ? ", worldspawn only" : "" );

	if ( r_vertexLight && r_vertexLight->integer ) {
		ri.Printf( PRINT_ALL, "Note: using vertex lightmap approximation\n" );
	}
#if defined (USE_VK_PBR)
	ri.Printf( PRINT_ALL, "PBR SH extraction: %s\n", (r_pbr_shExtract && r_pbr_shExtract->integer) ? "enabled" : "disabled" );
	if ( r_glint && r_glint->integer ) {
		ri.Printf( PRINT_ALL, "PBR glint NDF: enabled (r_glint 1)\n" );
	}
	if ( r_pbr_iblAnisoStretch && Q_fabs( r_pbr_iblAnisoStretch->value - 1.0f ) > 0.001f ) {
		ri.Printf( PRINT_ALL, "PBR IBL anisotropy stretch: %.2f (r_pbr_iblAnisoStretch, default 1)\n", r_pbr_iblAnisoStretch->value );
	}
	if ( r_pbr_debug && r_pbr_debug->integer ) {
		ri.Printf( PRINT_ALL, "PBR debug view: mode %d (1=direct,2=ibl spec,3=irradiance,4=env samples,5-8=glint,17-19=resource health)\n", r_pbr_debug->integer );
	}
#else
	if ( (r_vertexLight && r_vertexLight->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		ri.Printf( PRINT_ALL, "Note: using vertex lightmap approximation\n" );
	} else if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
		ri.Printf( PRINT_ALL, "Note: ragePro approximations\n" );
	} else if ( glConfig.hardwareType == GLHW_RIVA128 ) {
		ri.Printf( PRINT_ALL, "Note: riva128 approximations\n" );
	}
#endif
	if ( r_finish && r_finish->integer ) {
		ri.Printf( PRINT_ALL, "Forcing glFinish\n" );
	}
}


/*
===============
GfxInfo_f
===============
*/
static void GfxInfo_f( void )
{
	GfxInfo();
	VarInfo();
}


static void VkInfo_f( void )
{
	ri.Printf(PRINT_ALL, "max_vertex_usage: %iKb\n", (int)((vk.stats.vertex_buffer_max + 1023) / 1024) );
	ri.Printf(PRINT_ALL, "max_push_size: %ib\n", vk.stats.push_size_max );

	ri.Printf(PRINT_ALL, "pipeline handles: %i\n", vk.pipeline_create_count );
	ri.Printf(PRINT_ALL, "pipeline descriptors: %i, base: %i\n", vk.pipelines_count, vk.pipelines_world_base );
	ri.Printf(PRINT_ALL, "image chunks: %i\n", vk_world.num_image_chunks );
}

static void VulkanInfo_f( void )
{
	VkPhysicalDeviceProperties props;
	char driver_version[64];

	if ( !vk.physical_device ) {
		ri.Printf( PRINT_ALL, "Vulkan not initialized.\n" );
		return;
	}

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	/* Decode driver version (vendor-specific encoding) */
	switch ( props.vendorID ) {
		case 0x10DE: /* NVIDIA */
			Com_sprintf( driver_version, sizeof( driver_version ), "%u.%u.%u.%u",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%u.%u.%u",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
			break;
	}

	ri.Printf( PRINT_ALL, "======== Vulkan Info ========\n" );
	ri.Printf( PRINT_ALL, "Device    : %s\n", vk_device_renderer_name( &props ) );
	ri.Printf( PRINT_ALL, "API       : %u.%u.%u\n",
		VK_VERSION_MAJOR( props.apiVersion ),
		VK_VERSION_MINOR( props.apiVersion ),
		VK_VERSION_PATCH( props.apiVersion ) );
	ri.Printf( PRINT_ALL, "Driver    : %s\n", driver_version );
	ri.Printf( PRINT_ALL, "Vendor ID : 0x%04X\n", props.vendorID );
	ri.Printf( PRINT_ALL, "Device ID : 0x%04X\n", props.deviceID );
	if ( vk_device_is_v3dv( &props ) )
		ri.Printf( PRINT_ALL, "Platform  : Raspberry Pi (V3DV)\n" );
	ri.Printf( PRINT_ALL, "==============================\n" );
}
