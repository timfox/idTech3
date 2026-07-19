/* Renderer diagnostics and console-control helpers.
 * Included by tr_init.c to preserve static linkage and cvar ABI. */

static int R_CvarInteger( const cvar_t *cv )
{
	return cv ? cv->integer : 0;
}

static float R_CvarValue( const cvar_t *cv )
{
	return cv ? cv->value : 0.0f;
}

static int R_NamedCvarInteger( const char *name )
{
	return ri.Cvar_VariableIntegerValue( name );
}

static float R_NamedCvarValue( const char *name )
{
	return Q_atof( ri.Cvar_VariableString( name ) );
}

static const char *R_NamedCvarString( const char *name )
{
	return ri.Cvar_VariableString( name );
}

static const char *R_YesNo( qboolean value )
{
	return value ? "yes" : "no";
}

static qboolean R_RtxAvailable( void )
{
#ifdef USE_VULKAN_RTX
	return vk.rtxAvailable;
#else
	return qfalse;
#endif
}

static const char *R_RenderPassName( renderPass_t pass )
{
	switch ( pass ) {
	case RENDER_PASS_MAIN:
		return "main";
	case RENDER_PASS_SCREENMAP:
		return "screenmap";
	case RENDER_PASS_SUN_SHADOW:
		return "sun_shadow";
	case RENDER_PASS_POST_BLOOM:
		return "post_bloom";
	case RENDER_PASS_UI_OVERLAY:
		return "ui_overlay";
	case RENDER_PASS_CUBEMAP:
		return "cubemap";
	default:
		return "unknown_pass";
	}
}

static const char *R_RendererProfileName( void )
{
	const char *havenrp = R_NamedCvarString( "r_havenrpProfile" );

	if ( havenrp && havenrp[0] && Q_stricmp( havenrp, "" ) ) {
		if ( !Q_stricmp( havenrp, "stable" ) ) {
			return "havenrp stable";
		}
		if ( !Q_stricmp( havenrp, "quality" ) ) {
			return "havenrp quality";
		}
		if ( !Q_stricmp( havenrp, "rt" ) ) {
			return "havenrp rt";
		}
		if ( !Q_stricmp( havenrp, "experimental" ) ) {
			return "havenrp experimental";
		}
		if ( !Q_stricmp( havenrp, "safe" ) ) {
			return "havenrp safe";
		}
	}
	if ( r_renderMode && r_renderMode->integer == 3 ) {
		return "unified clustered";
	}
	if ( r_hybrid1 && r_hybrid1->integer ) {
		return "hybrid1 overlay";
	}
	if ( r_rtx && r_rtx->integer > 0 ) {
		return "rtx overlay";
	}
	if ( r_renderMode && r_renderMode->integer == 1 && r_deferredLighting && r_deferredLighting->integer ) {
		return "deferred overlay";
	}
	if ( r_renderMode && r_renderMode->integer == 2 && r_forwardPlus && r_forwardPlus->integer ) {
		return "modern vulkan";
	}
	if ( r_renderMode && r_renderMode->integer == 0 ) {
		return "classic/forward";
	}
	return "custom";
}

static const char *R_RendererRecommendedRecoveryCommand( void )
{
	if ( r_renderMode && r_renderMode->integer == 3 ) {
		return "renderer_clustered_safe";
	}
	if ( r_renderMode && r_renderMode->integer == 1 ) {
		return "renderer_deferred_safe";
	}
	if ( r_renderMode && r_renderMode->integer == 2 ) {
		return "renderer_modern_safe";
	}
	return "none";
}

static qboolean R_RendererModeContractClean( void )
{
	const int mode = R_CvarInteger( r_renderMode );

	switch ( mode ) {
	case 1:
		return ( R_CvarInteger( r_forwardPlus ) &&
			R_CvarValue( r_forwardPlusShade ) <= 0.0f &&
			R_CvarInteger( r_deferredGBuffer ) &&
			R_CvarInteger( r_deferredGBufferFill ) ) ? qtrue : qfalse;
	case 2:
		return ( R_CvarInteger( r_forwardPlus ) &&
			R_CvarValue( r_forwardPlusShade ) > 0.0f &&
			R_CvarInteger( r_forwardPlusDepthCull ) &&
			!R_CvarInteger( r_deferredLighting ) ) ? qtrue : qfalse;
	case 3:
		return ( R_CvarInteger( r_forwardPlus ) &&
			R_CvarValue( r_forwardPlusShade ) > 0.0f &&
			R_CvarInteger( r_forwardPlusDepthCull ) &&
			R_CvarInteger( r_deferredGBuffer ) &&
			R_CvarInteger( r_deferredGBufferFill ) &&
			R_CvarInteger( r_deferredLighting ) ) ? qtrue : qfalse;
	default:
		return qtrue;
	}
}

static qboolean R_RendererForwardPlusRuntimeReady( void )
{
	if ( !r_forwardPlus || !r_forwardPlus->integer ) {
		return qtrue;
	}

	if ( vk.forward_plus.buffer == VK_NULL_HANDLE ||
		vk.forward_plus.staging == VK_NULL_HANDLE ||
		vk.forward_plus.tile_buffer == VK_NULL_HANDLE ||
		vk.forward_plus.param_buffer == VK_NULL_HANDLE ||
		vk.forward_plus.param_mapped == NULL ||
		vk.forward_plus.descriptor == VK_NULL_HANDLE ||
		vk.forward_plus.pipeline_layout == VK_NULL_HANDLE ||
		vk.forward_plus.tile_pipeline == VK_NULL_HANDLE ||
		vk.forward_plus.tiles_x == 0u ||
		vk.forward_plus.tiles_y == 0u ||
		vk.forward_plus.tile_capacity_tiles == 0u ) {
		return qfalse;
	}

	return qtrue;
}

static qboolean R_RendererDeferredRuntimeReady( void )
{
	const int mode = R_CvarInteger( r_renderMode );
	const qboolean wantDeferredFill =
		( mode == 1 || mode == 2 || mode == 3 ) &&
		R_CvarInteger( r_deferredGBuffer ) &&
		R_CvarInteger( r_deferredGBufferFill );
	const qboolean wantDeferredLighting =
		( mode == 1 || mode == 3 ) &&
		R_CvarInteger( r_deferredLighting );

	if ( !wantDeferredFill && !wantDeferredLighting ) {
		return qtrue;
	}

	if ( !vk.deferredGbufferAllocated ||
		vk.deferred_gbuffer_albedo_view == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_normal_view == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_material_view == VK_NULL_HANDLE ||
		vk.deferred_lighting_view == VK_NULL_HANDLE ||
		!vk.deferred_gbuffer.pipeline_ready ||
		vk.deferred_gbuffer.pipeline == VK_NULL_HANDLE ||
		vk.deferred_gbuffer.descriptor == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( wantDeferredLighting &&
		( !R_RendererForwardPlusRuntimeReady() ||
		  !vk.deferred_gbuffer.lighting_pipeline_ready ||
		  vk.deferred_gbuffer.lighting_pipeline == VK_NULL_HANDLE ||
		  vk.deferred_gbuffer.lighting_descriptor == VK_NULL_HANDLE ||
		  !vk.deferred_gbuffer.composite_gfx_ready ||
		  vk.deferred_gbuffer.composite_gfx_pipeline == VK_NULL_HANDLE ||
		  vk.deferred_gbuffer.composite_gfx_descriptor == VK_NULL_HANDLE ) ) {
		return qfalse;
	}

	return qtrue;
}

static qboolean R_RendererUnifiedClusteredRuntimeReady( void )
{
	if ( !r_renderMode || r_renderMode->integer != 3 ) {
		return qtrue;
	}

	if ( !vk_unified_clustered_active() ) {
		return qfalse;
	}

	return ( R_RendererForwardPlusRuntimeReady() &&
		R_RendererDeferredRuntimeReady() ) ? qtrue : qfalse;
}

static qboolean R_RendererSceneSourceHealthy( void )
{
	const VkImageView postFog = vk_get_post_fog_source();
	const VkImageView luminance = vk_get_luminance_source();

	if ( vk.fboActive && vk.color_image_view == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( postFog == VK_NULL_HANDLE || luminance == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( vk.uiOverlayActive && vk.renderPassIndex != RENDER_PASS_UI_OVERLAY &&
		vk.inRenderPass ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean R_RendererSwapchainHealthy( void )
{
	if ( vk.swapchain == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( !vk.swapchain_extent_valid ||
		vk.swapchain_extent.width == 0 ||
		vk.swapchain_extent.height == 0 ) {
		return qfalse;
	}
	if ( vk.swapchain_image_count == 0 ) {
		return qfalse;
	}
	return qtrue;
}

static const char *R_RendererSwapchainLastRestartName( void )
{
	if ( vk.swapchain_restart_count == 0 ) {
		return "none";
	}

	return vk_result_string( (VkResult)vk.swapchain_last_restart_result );
}

static qboolean R_RendererSwapchainRestartRecent( void )
{
	const int restartWindowMs = 5000;
	int now;

	if ( vk.swapchain_restart_count == 0 || vk.swapchain_last_restart_ms <= 0 ) {
		return qfalse;
	}

	now = ri.Milliseconds();
	if ( now < vk.swapchain_last_restart_ms ) {
		return qtrue;
	}

	return ( now - vk.swapchain_last_restart_ms ) <= restartWindowMs ? qtrue : qfalse;
}

static qboolean R_RendererCubemapIblReady( const cubemap_t *cube )
{
	if ( !cube || !cube->prefiltered_image || !cube->irradiance_image ) {
		return qfalse;
	}
	if ( cube->prefiltered_image->handle == VK_NULL_HANDLE ||
		cube->prefiltered_image->view == VK_NULL_HANDLE ||
		cube->prefiltered_image->descriptor == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( cube->irradiance_image->handle == VK_NULL_HANDLE ||
		cube->irradiance_image->view == VK_NULL_HANDLE ||
		cube->irradiance_image->descriptor == VK_NULL_HANDLE ) {
		return qfalse;
	}
	return qtrue;
}

static int R_RendererCountReadyCubemaps( void )
{
	return vk.pbr_ibl_ready_cubemap_count;
}

static int R_RendererCountIncompleteCubemaps( void )
{
	return vk.pbr_ibl_incomplete_cubemap_count;
}

static qboolean R_RendererUsingRecoveryFallbacks( void )
{
	if ( vk.pbr_ibl_using_hdr_fallback && tr.numCubemaps > 0 ) {
		return qtrue;
	}
	if ( !R_RendererForwardPlusRuntimeReady() ||
		!R_RendererDeferredRuntimeReady() ||
		!R_RendererUnifiedClusteredRuntimeReady() ) {
		return qtrue;
	}
	if ( R_RendererSwapchainRestartRecent() ) {
		return qtrue;
	}
	return qfalse;
}

static const char *R_RendererHealthLabel( int warnings )
{
	if ( !R_RendererModeContractClean() || !R_RendererSceneSourceHealthy() ||
		!R_RendererForwardPlusRuntimeReady() ||
		!R_RendererDeferredRuntimeReady() ||
		!R_RendererUnifiedClusteredRuntimeReady() ) {
		return "recover";
	}
	if ( R_RendererUsingRecoveryFallbacks() ) {
		return "recover";
	}
	if ( warnings > 0 ) {
		return "warn";
	}
	return "ok";
}

static int R_RendererPrintCompatibilityWarnings( qboolean printOk )
{
	int warnings = 0;
#define R_RENDERER_WARN(...) do { \
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[renderer][compat] " __VA_ARGS__ ); \
		ri.Printf( PRINT_WARNING, S_COLOR_WHITE ); \
		warnings++; \
	} while ( 0 )

	if ( r_deferredGBuffer && r_deferredGBuffer->integer && ( !r_fbo || !r_fbo->integer ) ) {
		R_RENDERER_WARN( "r_deferredGBuffer 1 requires r_fbo 1; G-buffer allocation will be disabled until vid_restart.\n" );
	}
	if ( r_deferredGBuffer && r_deferredGBuffer->integer && r_renderMode && r_renderMode->integer == 0 ) {
		R_RENDERER_WARN( "r_deferredGBuffer 1 is ignored in r_renderMode 0; use r_renderMode 1/2/3 and vid_restart.\n" );
	}
	if ( r_deferredGBufferFill && r_deferredGBufferFill->integer && ( !r_deferredGBuffer || !r_deferredGBuffer->integer ) ) {
		R_RENDERER_WARN( "r_deferredGBufferFill 1 needs r_deferredGBuffer 1 to create deferred targets.\n" );
	}
	if ( r_deferredLighting && r_deferredLighting->integer &&
		( !r_renderMode || ( r_renderMode->integer != 1 && r_renderMode->integer != 3 ) ) ) {
		R_RENDERER_WARN( "r_deferredLighting 1 only runs in r_renderMode 1 or 3; current mode is %d.\n", R_CvarInteger( r_renderMode ) );
	}
	if ( r_renderMode && r_renderMode->integer == 1 ) {
		if ( !r_forwardPlus || !r_forwardPlus->integer ) {
			R_RENDERER_WARN( "deferred lighting mode needs r_forwardPlus 1 for tile light lists.\n" );
		}
		if ( r_forwardPlusShade && r_forwardPlusShade->value <= 0.0f ) {
			R_RENDERER_WARN( "mode-1 deferred expects r_forwardPlusShade > 0 for transparent Forward+ (opaque uses deferred handoff).\n" );
		}
		if ( !r_deferredGBuffer || !r_deferredGBuffer->integer || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) {
			R_RENDERER_WARN( "mode-1 deferred expects r_deferredGBuffer 1 and r_deferredGBufferFill 1 for the opaque capture path.\n" );
		}
		if ( r_deferredLighting && r_deferredLighting->integer &&
			( ( r_taa && r_taa->integer ) ||
			  ( r_ext_smaa && r_ext_smaa->integer ) ||
			  ( r_ext_fxaa && r_ext_fxaa->integer ) ||
			  ( r_postAaAfterBloom && r_postAaAfterBloom->integer ) ) ) {
			R_RENDERER_WARN( "deferred lighting mode is safer with TAA/SMAA/FXAA/post-AA disabled while debugging; use renderer_deferred_safe + vid_restart.\n" );
		}
		if ( vk.msaaActive ) {
			R_RENDERER_WARN( "mode-1 deferred is running with MSAA; direct G-buffer material export falls back to depth-derived defaults.\n" );
		}
		if ( !R_RendererDeferredRuntimeReady() ) {
			R_RENDERER_WARN( "mode-1 deferred cvars are active, but deferred runtime resources are incomplete (G-buffer/deferred pipelines or descriptors missing).\n" );
		}
	}
	if ( r_renderMode && r_renderMode->integer == 3 ) {
		if ( !r_forwardPlus || !r_forwardPlus->integer ) {
			R_RENDERER_WARN( "Unified Clustered (mode 3) needs r_forwardPlus 1 for shared tile lists.\n" );
		}
		if ( !r_forwardPlusShade || r_forwardPlusShade->value <= 0.0f ) {
			R_RENDERER_WARN( "Unified Clustered (mode 3) needs r_forwardPlusShade > 0 for transparent Forward+.\n" );
		}
		if ( !r_deferredGBuffer || !r_deferredGBuffer->integer || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) {
			R_RENDERER_WARN( "Unified Clustered (mode 3) needs r_deferredGBuffer 1 and r_deferredGBufferFill 1.\n" );
		}
		if ( !r_deferredLighting || !r_deferredLighting->integer ) {
			R_RENDERER_WARN( "Unified Clustered (mode 3) needs r_deferredLighting 1 for opaque deferred shade.\n" );
		}
		if ( ( r_taa && r_taa->integer ) ||
			( r_ext_smaa && r_ext_smaa->integer ) ||
			( r_ext_fxaa && r_ext_fxaa->integer ) ||
			( r_postAaAfterBloom && r_postAaAfterBloom->integer ) ) {
			R_RENDERER_WARN( "Unified Clustered is safer with TAA/SMAA/FXAA/post-AA disabled while debugging; use renderer_clustered_safe + vid_restart.\n" );
		}
		if ( r_oit && r_oit->integer ) {
			R_RENDERER_WARN( "Unified Clustered + OIT replaces the transparent Forward+ shade path; use renderer_clustered_safe when debugging opaque/deferred correctness first.\n" );
		}
		if ( vk.msaaActive ) {
			R_RENDERER_WARN( "MSAA is active; Unified Clustered prefers r_ext_multisample 0 for direct G-buffer export.\n" );
		}
		if ( !R_RendererUnifiedClusteredRuntimeReady() ) {
			R_RENDERER_WARN( "Unified Clustered cvars are active, but shared Forward+/deferred runtime resources are incomplete.\n" );
		}
	}
	if ( r_renderMode && r_renderMode->integer == 2 ) {
		if ( !r_forwardPlus || !r_forwardPlus->integer ) {
			R_RENDERER_WARN( "modern profile expects r_forwardPlus 1; current mode will fall back to non-Forward+ behavior.\n" );
		}
		if ( !r_forwardPlusShade || r_forwardPlusShade->value <= 0.0f ) {
			R_RENDERER_WARN( "modern profile expects r_forwardPlusShade > 0 for Forward+ primary dynamic lighting.\n" );
		}
		if ( !r_forwardPlusDepthCull || !r_forwardPlusDepthCull->integer ) {
			R_RENDERER_WARN( "modern profile expects r_forwardPlusDepthCull 1 so tile light cull tracks opaque depth.\n" );
		}
		if ( !r_pbr || !r_pbr->integer ) {
			R_RENDERER_WARN( "modern profile expects r_pbr 1 for physically based material shading.\n" );
		}
		if ( !r_hdr || r_hdr->integer <= 0 ) {
			R_RENDERER_WARN( "modern profile expects HDR enabled (r_hdr 1/2/3).\n" );
		}
		if ( !r_taa || !r_taa->integer ) {
			R_RENDERER_WARN( "modern profile expects r_taa 1 for temporal resolve.\n" );
		}
		if ( !r_taaMotionVectors || !r_taaMotionVectors->integer ) {
			R_RENDERER_WARN( "modern profile expects r_taaMotionVectors 1 for material-pass motion vectors.\n" );
		}
		if ( !r_deferredGBuffer || !r_deferredGBuffer->integer || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) {
			R_RENDERER_WARN( "modern profile expects r_deferredGBuffer 1 and r_deferredGBufferFill 1 for the sidecar G-buffer.\n" );
		}
		if ( r_deferredLighting && r_deferredLighting->integer ) {
			R_RENDERER_WARN( "modern profile keeps r_deferredLighting 0; use renderer_modern_safe + vid_restart after mode-1/3 experiments.\n" );
		}
		if ( !R_RendererForwardPlusRuntimeReady() ) {
			R_RENDERER_WARN( "modern profile cvars are active, but Forward+ runtime resources are incomplete (light/tile buffers or compute pipeline missing).\n" );
		}
		if ( !R_RendererDeferredRuntimeReady() ) {
			R_RENDERER_WARN( "modern profile sidecar G-buffer cvars are active, but deferred runtime resources are incomplete.\n" );
		}
	}
	if ( vk.cubemapActive && tr.numCubemaps > 0 && R_RendererCountIncompleteCubemaps() > 0 ) {
		R_RENDERER_WARN( "PBR IBL has %d incomplete cubemap(s) out of %d; probe selection may remap to the nearest ready cubemap.\n",
			R_RendererCountIncompleteCubemaps(), tr.numCubemaps );
	}
	if ( vk.pbr_ibl_using_hdr_fallback && tr.numCubemaps > 0 ) {
		R_RENDERER_WARN( "PBR IBL is actively using HDR skybox fallback because no ready local cubemap is available.\n" );
	}
	if ( !R_RendererSwapchainHealthy() ) {
		R_RENDERER_WARN( "swapchain state is incomplete (extentValid=%d images=%u handle=0x%llx); fullscreen/resize recovery may need vid_restart.\n",
			vk.swapchain_extent_valid ? 1 : 0,
			(unsigned int)vk.swapchain_image_count,
			(unsigned long long)(uintptr_t)vk.swapchain );
	}
	if ( R_RendererSwapchainRestartRecent() ) {
		R_RENDERER_WARN( "swapchain restarted recently (%s at %d ms); verify fullscreen/resize recovery if the frame still looks wrong.\n",
			R_RendererSwapchainLastRestartName(), vk.swapchain_last_restart_ms );
	}
	if ( vk.deferredGbufferAllocated && !vk.deferredGbufferDirectExport && vk.msaaActive ) {
		R_RENDERER_WARN( "MSAA is active; deferred material export is using the depth-derived fallback instead of direct PBR MRT export.\n" );
	}
	if ( r_rtx && r_rtx->integer > 0 && !R_RtxAvailable() ) {
		R_RENDERER_WARN( "r_rtx %d requested, but Vulkan ray tracing is not available in this build/device session.\n", r_rtx->integer );
	}
	if ( r_hybrid1 && r_hybrid1->integer && !R_RtxAvailable() ) {
		R_RENDERER_WARN( "r_hybrid1 1 requested, but Vulkan ray tracing is not available in this build/device session.\n" );
	}
	if ( r_hybrid1 && r_hybrid1->integer && ( !r_rtxDemo || !r_rtxDemo->integer ) ) {
		R_RENDERER_WARN( "r_hybrid1 1 requires r_rtxDemo 1 for the shared TLAS path.\n" );
	}
	if ( R_NamedCvarInteger( "r_niv" ) && ( !r_fbo || !r_fbo->integer ) ) {
		R_RENDERER_WARN( "r_niv 1 needs r_fbo 1 so the NIV composite can read/write the HDR scene target.\n" );
	}
	if ( R_NamedCvarInteger( "r_vfgi" ) && ( !r_fbo || !r_fbo->integer ) ) {
		R_RENDERER_WARN( "r_vfgi 1 needs r_fbo 1 so the VFGI composite can read/write the HDR scene target.\n" );
	}
	if ( R_NamedCvarInteger( "r_niv" ) && R_NamedCvarInteger( "r_niv_useGBuffer" ) &&
		( !r_deferredGBuffer || !r_deferredGBuffer->integer || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) ) {
		R_RENDERER_WARN( "r_niv_useGBuffer 1 needs r_deferredGBuffer 1 and r_deferredGBufferFill 1 for normal-aware GI.\n" );
	}
	if ( R_NamedCvarInteger( "r_vfgi" ) && R_NamedCvarInteger( "r_vfgi_useGBuffer" ) &&
		( !r_deferredGBuffer || !r_deferredGBuffer->integer || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) ) {
		R_RENDERER_WARN( "r_vfgi_useGBuffer 1 needs r_deferredGBuffer 1 and r_deferredGBufferFill 1 for normal-aware GI.\n" );
	}
	if ( R_NamedCvarInteger( "r_nvc" ) && ( !r_forwardPlus || !r_forwardPlus->integer ) ) {
		R_RENDERER_WARN( "r_nvc 1 expects r_forwardPlus 1 because it refines the Forward+ direct-light path.\n" );
	}
	if ( R_NamedCvarInteger( "r_nvc" ) && R_NamedCvarInteger( "r_nvc_useGBuffer" ) &&
		( !r_deferredGBuffer || !r_deferredGBuffer->integer || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) ) {
		R_RENDERER_WARN( "r_nvc_useGBuffer 1 needs the sidecar G-buffer enabled and filled.\n" );
	}
	if ( R_NamedCvarInteger( "r_ndgi_compute" ) ) {
		R_RENDERER_WARN( "r_ndgi_compute 1 is reserved; NDGI still uses the CPU lightmap decode path in this build.\n" );
	}
	if ( R_NamedCvarInteger( "r_niv" ) && R_NamedCvarValue( "r_niv_ao" ) > 0.0f && ( !r_ssao || !r_ssao->integer ) ) {
		R_RENDERER_WARN( "r_niv_ao is > 0 but r_ssao is 0; NIV contact-aware indirect attenuation will be inactive.\n" );
	}
	if ( R_NamedCvarInteger( "r_vfgi" ) && R_NamedCvarValue( "r_vfgi_ao" ) > 0.0f && ( !r_ssao || !r_ssao->integer ) ) {
		R_RENDERER_WARN( "r_vfgi_ao is > 0 but r_ssao is 0; VFGI contact-aware indirect attenuation will be inactive.\n" );
	}

	if ( warnings == 0 && printOk ) {
		ri.Printf( PRINT_ALL, "[renderer][compat] no compatibility warnings for active profile '%s'.\n", R_RendererProfileName() );
	}

#undef R_RENDERER_WARN
	return warnings;
}

static void R_RendererProfile_f( void )
{
	ri.Printf( PRINT_ALL, "======== Renderer Profile ========\n" );
	ri.Printf( PRINT_ALL, "active    : %s\n", R_RendererProfileName() );
	ri.Printf( PRINT_ALL, "mode      : %d (0=forward, 1=deferred, 2=modern Forward+, 3=Unified Clustered)\n", R_CvarInteger( r_renderMode ) );
	ri.Printf( PRINT_ALL, "base      : exec modern_vulkan.cfg\n" );
	ri.Printf( PRINT_ALL, "overlays  : exec vulkan_overlay_deferred.cfg | vulkan_overlay_unified_clustered.cfg | vulkan_overlay_rtx.cfg | vulkan_overlay_hybrid1.cfg\n" );
	ri.Printf( PRINT_ALL, "classic   : exec classic_baseq3.cfg or classic_openarena_native.cfg\n" );
	ri.Printf( PRINT_ALL, "warnings  : %d\n", R_RendererPrintCompatibilityWarnings( qfalse ) );
	ri.Printf( PRINT_ALL, "==================================\n" );
}

static void R_RendererStatus_f( void )
{
	const char *aoOwner;
	const char *giOwner;
	const char *reflOwner;
	const char *temporalOwner;
	const int avMode = R_NamedCvarInteger( "r_ambientVisibilityMode" );
	const qboolean avActive = ( avMode >= 2 && R_CvarInteger( r_deferredGBuffer ) && vk_deferred_gbuffer_active() ) ? qtrue : qfalse;
	const qboolean surfelOn = R_NamedCvarInteger( "r_surfelGi" ) ? qtrue : qfalse;
	const qboolean rcgiOn = R_NamedCvarInteger( "r_rcgi" ) ? qtrue : qfalse;
	const qboolean hybridOn = R_NamedCvarInteger( "r_hybrid1" ) ? qtrue : qfalse;
	const qboolean ssrOn = R_NamedCvarInteger( "r_ssr" ) ? qtrue : qfalse;
	const qboolean ssaoOn = R_NamedCvarInteger( "r_ssao" ) ? qtrue : qfalse;

	if ( avActive ) {
		aoOwner = "ambient_visibility";
	} else if ( avMode == 1 && ssaoOn ) {
		aoOwner = "legacy_ssao";
	} else if ( ssaoOn && avMode < 2 ) {
		aoOwner = "legacy_ssao";
	} else {
		aoOwner = "none";
	}

	if ( rcgiOn ) {
		giOwner = "rcgi";
	} else if ( surfelOn ) {
		giOwner = "surfel_gi";
	} else if ( R_NamedCvarInteger( "r_shWorldLighting" ) || R_NamedCvarInteger( "r_shLighting" ) || R_NamedCvarInteger( "r_pbr_shExtract" ) ) {
		giOwner = "sh_ibl";
	} else {
		giOwner = "none";
	}

	if ( hybridOn && R_NamedCvarInteger( "r_hybrid1_spec" ) ) {
		reflOwner = "hybrid1_spec";
	} else if ( ssrOn ) {
		reflOwner = "ssr";
	} else {
		reflOwner = "none";
	}

	if ( R_NamedCvarInteger( "r_taa" ) ) {
		temporalOwner = "taa";
	} else if ( hybridOn ) {
		temporalOwner = "hybrid1";
	} else {
		temporalOwner = "none";
	}

	ri.Printf( PRINT_ALL, "======== Renderer Status ========\n" );
	ri.Printf( PRINT_ALL, "profile   : %s (r_havenrpProfile=%s)\n",
		R_RendererProfileName(), R_NamedCvarString( "r_havenrpProfile" ) );
	ri.Printf( PRINT_ALL, "mode req  : r_renderMode=%d\n", R_CvarInteger( r_renderMode ) );
	ri.Printf( PRINT_ALL, "mode eff  : %s forwardPlus=%d deferredGBuffer=%d deferredLighting=%d\n",
		R_RendererProfileName(), R_CvarInteger( r_forwardPlus ),
		R_CvarInteger( r_deferredGBuffer ), R_CvarInteger( r_deferredLighting ) );
	ri.Printf( PRINT_ALL, "owners    : AO=%s  diffuseGI=%s  reflections=%s  temporal=%s\n",
		aoOwner, giOwner, reflOwner, temporalOwner );
	ri.Printf( PRINT_ALL, "declared  : AO=%s GI=%s refl=%s temporal=%s\n",
		R_NamedCvarString( "r_havenrpAoOwner" ),
		R_NamedCvarString( "r_havenrpDiffuseGiOwner" ),
		R_NamedCvarString( "r_havenrpReflectionOwner" ),
		R_NamedCvarString( "r_havenrpTemporalOwner" ) );
	ri.Printf( PRINT_ALL, "RT        : available=%s active(rtx=%d hybrid1=%d surfel=%d rcgi=%d)\n",
		R_YesNo( R_RtxAvailable() ),
		R_NamedCvarInteger( "r_rtx" ), R_NamedCvarInteger( "r_hybrid1" ),
		R_NamedCvarInteger( "r_surfelGi" ), R_NamedCvarInteger( "r_rcgi" ) );
	ri.Printf( PRINT_ALL, "streaming : openWorld=%d bspStream=%d\n",
		R_NamedCvarInteger( "r_openWorld" ), R_NamedCvarInteger( "r_bspStream" ) );
	ri.Printf( PRINT_ALL, "fallback  : reason=%s lkg=%s fails=%d\n",
		R_NamedCvarString( "r_havenrpFallbackReason" ),
		R_NamedCvarString( "r_havenrpLkgProfile" ),
		R_NamedCvarInteger( "r_havenrpFailCount" ) );
	ri.Printf( PRINT_ALL, "backend   : Vulkan (%s)\n", glConfig.renderer_string );
	ri.Printf( PRINT_ALL, "resolution: %dx%d renderTarget=%ux%u\n",
		glConfig.vidWidth, glConfig.vidHeight, vk.renderWidth, vk.renderHeight );
	ri.Printf( PRINT_ALL, "fbo/hdr   : r_fbo=%d active=%s r_hdr=%d colorFormat=%s\n",
		R_CvarInteger( r_fbo ), R_YesNo( vk.fboActive ), R_CvarInteger( r_hdr ), vk_format_string( vk.color_format ) );
	ri.Printf( PRINT_ALL, "pbr       : r_pbr=%d materialBlend=%d\n",
		R_CvarInteger( r_pbr ), R_CvarInteger( r_materialBlend ) );
	ri.Printf( PRINT_ALL, "contract  : clean=%s recovery=%s\n",
		R_YesNo( R_RendererModeContractClean() ), R_RendererRecommendedRecoveryCommand() );
	ri.Printf( PRINT_ALL, "==================================\n" );
}

static void R_HavenRPRendererStatus_f( void )
{
	R_RendererStatus_f();
}

static void R_RendererCompatibility_f( void )
{
	ri.Printf( PRINT_ALL, "======== Renderer Compatibility ========\n" );
	ri.Printf( PRINT_ALL, "profile: %s\n", R_RendererProfileName() );
	ri.Printf( PRINT_ALL, "contract: clean=%s recovery=%s\n",
		R_YesNo( R_RendererModeContractClean() ), R_RendererRecommendedRecoveryCommand() );
	ri.Printf( PRINT_ALL, "passes: inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s\n",
		R_YesNo( vk.inRenderPass ), R_RenderPassName( vk.renderPassIndex ),
		R_YesNo( vk.uiOverlayActive ), R_YesNo( R_RendererSceneSourceHealthy() ) );
	ri.Printf( PRINT_ALL, "present: swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d\n",
		R_YesNo( R_RendererSwapchainHealthy() ),
		vk.swapchain_extent_valid ? vk.swapchain_extent.width : 0u,
		vk.swapchain_extent_valid ? vk.swapchain_extent.height : 0u,
		(unsigned int)vk.swapchain_image_count, glConfig.isFullscreen, R_CvarInteger( r_vk_swapchain_srgb ) );
	ri.Printf( PRINT_ALL, "restart: count=%u last=%s\n",
		(unsigned int)vk.swapchain_restart_count, R_RendererSwapchainLastRestartName() );
	ri.Printf( PRINT_ALL, "restartAt: recent=%s ms=%d\n",
		R_YesNo( R_RendererSwapchainRestartRecent() ), vk.swapchain_last_restart_ms );
	ri.Printf( PRINT_ALL, "sources: postFog=%s scene=%s luminance=%s\n",
		vk_post_fog_source_name( vk_get_post_fog_source() ),
		vk_post_fog_source_name( vk.scene_post_fog_color_source ),
		vk_post_fog_source_name( vk_get_luminance_source() ) );
	ri.Printf( PRINT_ALL, "cubemaps: runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=\"%s\"\n",
		R_YesNo( vk.cubemapActive ), tr.numCubemaps,
		R_RendererCountReadyCubemaps(), R_RendererCountIncompleteCubemaps(),
		R_YesNo( vk.pbr_ibl_using_hdr_fallback ), R_YesNo( vk.pbr_ibl_has_ready_local_cubemap ),
		r_pbr_cubemapInfo ? r_pbr_cubemapInfo->string : "" );
	ri.Printf( PRINT_ALL, "warnings: %d\n", R_RendererPrintCompatibilityWarnings( qtrue ) );
	ri.Printf( PRINT_ALL, "========================================\n" );
}

static void R_RendererHealth_f( void )
{
	const int warnings = R_RendererPrintCompatibilityWarnings( qfalse );
	const char *health = R_RendererHealthLabel( warnings );

	ri.Printf( PRINT_ALL, "======== Renderer Health ========\n" );
	ri.Printf( PRINT_ALL, "health    : %s\n", health );
	ri.Printf( PRINT_ALL, "profile   : %s\n", R_RendererProfileName() );
	ri.Printf( PRINT_ALL, "contract  : clean=%s recovery=%s\n",
		R_YesNo( R_RendererModeContractClean() ), R_RendererRecommendedRecoveryCommand() );
	ri.Printf( PRINT_ALL, "passes    : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s\n",
		R_YesNo( vk.inRenderPass ), R_RenderPassName( vk.renderPassIndex ),
		R_YesNo( vk.uiOverlayActive ), R_YesNo( R_RendererSceneSourceHealthy() ) );
	ri.Printf( PRINT_ALL, "present   : swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d\n",
		R_YesNo( R_RendererSwapchainHealthy() ),
		vk.swapchain_extent_valid ? vk.swapchain_extent.width : 0u,
		vk.swapchain_extent_valid ? vk.swapchain_extent.height : 0u,
		(unsigned int)vk.swapchain_image_count, glConfig.isFullscreen, R_CvarInteger( r_vk_swapchain_srgb ) );
	ri.Printf( PRINT_ALL, "restart   : count=%u last=%s\n",
		(unsigned int)vk.swapchain_restart_count, R_RendererSwapchainLastRestartName() );
	ri.Printf( PRINT_ALL, "restartAt : recent=%s ms=%d\n",
		R_YesNo( R_RendererSwapchainRestartRecent() ), vk.swapchain_last_restart_ms );
	ri.Printf( PRINT_ALL, "sources   : postFog=%s scene=%s luminance=%s\n",
		vk_post_fog_source_name( vk_get_post_fog_source() ),
		vk_post_fog_source_name( vk.scene_post_fog_color_source ),
		vk_post_fog_source_name( vk_get_luminance_source() ) );
	ri.Printf( PRINT_ALL, "cubemaps  : runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=\"%s\"\n",
		R_YesNo( vk.cubemapActive ), tr.numCubemaps,
		R_RendererCountReadyCubemaps(), R_RendererCountIncompleteCubemaps(),
		R_YesNo( vk.pbr_ibl_using_hdr_fallback ), R_YesNo( vk.pbr_ibl_has_ready_local_cubemap ),
		r_pbr_cubemapInfo ? r_pbr_cubemapInfo->string : "" );
	ri.Printf( PRINT_ALL, "runtime   : forward+=%s deferred=%s clustered=%s\n",
		R_YesNo( R_RendererForwardPlusRuntimeReady() ),
		R_YesNo( R_RendererDeferredRuntimeReady() ),
		R_YesNo( R_RendererUnifiedClusteredRuntimeReady() ) );
	ri.Printf( PRINT_ALL, "recovery  : activeFallbacks=%s recentSwapchain=%s\n",
		R_YesNo( R_RendererUsingRecoveryFallbacks() ),
		R_YesNo( R_RendererSwapchainRestartRecent() ) );
	ri.Printf( PRINT_ALL, "warnings  : %d\n", warnings );
	if ( Q_stricmp( health, "ok" ) ) {
		ri.Printf( PRINT_ALL, "next      : %s + vid_restart\n", R_RendererRecommendedRecoveryCommand() );
	}
	ri.Printf( PRINT_ALL, "=================================\n" );
}

static void R_RendererDeferredSafe_f( void )
{
	ri.Cvar_Set( "r_renderMode", "1" );
	ri.Cvar_Set( "r_forwardPlus", "1" );
	ri.Cvar_Set( "r_forwardPlusShade", "0" );
	ri.Cvar_Set( "r_ext_multisample", "0" );
	ri.Cvar_Set( "r_deferredGBuffer", "1" );
	ri.Cvar_Set( "r_deferredGBufferFill", "1" );
	ri.Cvar_Set( "r_deferredLighting", "1" );
	ri.Cvar_Set( "r_deferredUnlitBase", "1" );
	ri.Cvar_Set( "r_deferredLightingStrength", "1" );
	ri.Cvar_Set( "r_deferredSpecular", "1" );
	ri.Cvar_Set( "r_deferredSpecularStrength", "1" );
	ri.Cvar_Set( "r_deferredAOCoupling", "0.65" );
	ri.Cvar_Set( "r_deferredDefaultMetalness", "0" );
	ri.Cvar_Set( "r_deferredDefaultRoughness", "0.55" );
	ri.Cvar_Set( "r_deferredNormalEdgeThreshold", "0.08" );
	ri.Cvar_Set( "r_deferredGBufferDebug", "0" );
	ri.Cvar_Set( "r_taa", "0" );
	ri.Cvar_Set( "r_taaMotionVectors", "0" );
	ri.Cvar_Set( "r_ext_smaa", "0" );
	ri.Cvar_Set( "r_ext_fxaa", "0" );
	ri.Cvar_Set( "r_postAaAfterBloom", "0" );
	ri.Printf( PRINT_ALL,
		"[VK] Deferred safe preset applied: mode 1 deferred + temporal/post AA off + MSAA off. Run vid_restart.\n" );
}

static void R_RendererModernSafe_f( void )
{
	ri.Cvar_Set( "r_fbo", "1" );
	ri.Cvar_Set( "r_hdr", "2" );
	ri.Cvar_Set( "r_pbr", "1" );
	ri.Cvar_Set( "r_materialBlend", "1" );
	ri.Cvar_Set( "r_renderMode", "2" );
	ri.Cvar_Set( "r_forwardPlus", "1" );
	ri.Cvar_Set( "r_forwardPlusShade", "1" );
	ri.Cvar_Set( "r_forwardPlusDepthCull", "1" );
	ri.Cvar_Set( "r_forwardPlusSpecularStrength", "0.65" );
	ri.Cvar_Set( "r_forwardPlusEnergyRenorm", "0.45" );
	ri.Cvar_Set( "r_ext_multisample", "0" );
	ri.Cvar_Set( "r_deferredGBuffer", "1" );
	ri.Cvar_Set( "r_deferredGBufferFill", "1" );
	ri.Cvar_Set( "r_deferredLighting", "0" );
	ri.Cvar_Set( "r_deferredGBufferDebug", "0" );
	ri.Cvar_Set( "r_taa", "1" );
	ri.Cvar_Set( "r_taaMotionVectors", "1" );
	ri.Cvar_Set( "r_ext_smaa", "1" );
	ri.Cvar_Set( "r_ext_fxaa", "0" );
	ri.Cvar_Set( "r_postAaAfterBloom", "1" );
	ri.Printf( PRINT_ALL,
		"[VK] Modern safe preset applied: mode 2 Forward+ + sidecar G-buffer + TAA/SMAA restored, deferred lighting off, MSAA off. Run vid_restart.\n" );
}

static void R_RendererClusteredSafe_f( void )
{
	ri.Cvar_Set( "r_renderMode", "3" );
	ri.Cvar_Set( "r_forwardPlus", "1" );
	ri.Cvar_Set( "r_forwardPlusShade", "1" );
	ri.Cvar_Set( "r_forwardPlusDepthCull", "1" );
	ri.Cvar_Set( "r_ext_multisample", "0" );
	ri.Cvar_Set( "r_deferredGBuffer", "1" );
	ri.Cvar_Set( "r_deferredGBufferFill", "1" );
	ri.Cvar_Set( "r_deferredLighting", "1" );
	ri.Cvar_Set( "r_deferredUnlitBase", "1" );
	ri.Cvar_Set( "r_deferredLightingStrength", "1" );
	ri.Cvar_Set( "r_deferredSpecular", "1" );
	ri.Cvar_Set( "r_deferredSpecularStrength", "1" );
	ri.Cvar_Set( "r_deferredAOCoupling", "0.65" );
	ri.Cvar_Set( "r_deferredDefaultMetalness", "0" );
	ri.Cvar_Set( "r_deferredDefaultRoughness", "0.55" );
	ri.Cvar_Set( "r_deferredNormalEdgeThreshold", "0.08" );
	ri.Cvar_Set( "r_deferredGBufferDebug", "0" );
	ri.Cvar_Set( "r_oit", "0" );
	ri.Cvar_Set( "r_taa", "0" );
	ri.Cvar_Set( "r_taaMotionVectors", "0" );
	ri.Cvar_Set( "r_ext_smaa", "0" );
	ri.Cvar_Set( "r_ext_fxaa", "0" );
	ri.Cvar_Set( "r_postAaAfterBloom", "0" );
	ri.Printf( PRINT_ALL,
		"[VK] Unified Clustered safe preset applied: mode 3 + temporal/post AA/OIT off + MSAA off. Run vid_restart.\n" );
}

static void R_RendererSubsystems_f( void )
{
	const int warnings = R_RendererPrintCompatibilityWarnings( qfalse );

	ri.Printf( PRINT_ALL, "======== Renderer Subsystems ========\n" );
	ri.Printf( PRINT_ALL, "profile    : %s (%s)\n", R_RendererProfileName(), warnings == 0 ? "clean" : "compat-warnings" );
	ri.Printf( PRINT_ALL, "contract   : clean=%s recovery=%s\n",
		R_YesNo( R_RendererModeContractClean() ), R_RendererRecommendedRecoveryCommand() );
	ri.Printf( PRINT_ALL, "passes     : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s\n",
		R_YesNo( vk.inRenderPass ), R_RenderPassName( vk.renderPassIndex ),
		R_YesNo( vk.uiOverlayActive ), R_YesNo( R_RendererSceneSourceHealthy() ) );
	ri.Printf( PRINT_ALL, "present    : swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d\n",
		R_YesNo( R_RendererSwapchainHealthy() ),
		vk.swapchain_extent_valid ? vk.swapchain_extent.width : 0u,
		vk.swapchain_extent_valid ? vk.swapchain_extent.height : 0u,
		(unsigned int)vk.swapchain_image_count, glConfig.isFullscreen, R_CvarInteger( r_vk_swapchain_srgb ) );
	ri.Printf( PRINT_ALL, "restart    : count=%u last=%s\n",
		(unsigned int)vk.swapchain_restart_count, R_RendererSwapchainLastRestartName() );
	ri.Printf( PRINT_ALL, "restartAt  : recent=%s ms=%d\n",
		R_YesNo( R_RendererSwapchainRestartRecent() ), vk.swapchain_last_restart_ms );
	ri.Printf( PRINT_ALL, "sources    : postFog=%s scene=%s luminance=%s\n",
		vk_post_fog_source_name( vk_get_post_fog_source() ),
		vk_post_fog_source_name( vk.scene_post_fog_color_source ),
		vk_post_fog_source_name( vk_get_luminance_source() ) );
	ri.Printf( PRINT_ALL, "cubemaps   : runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=\"%s\"\n",
		R_YesNo( vk.cubemapActive ), tr.numCubemaps,
		R_RendererCountReadyCubemaps(), R_RendererCountIncompleteCubemaps(),
		R_YesNo( vk.pbr_ibl_using_hdr_fallback ), R_YesNo( vk.pbr_ibl_has_ready_local_cubemap ),
		r_pbr_cubemapInfo ? r_pbr_cubemapInfo->string : "" );
	ri.Printf( PRINT_ALL, "runtime    : forward+=%s deferred=%s clustered=%s\n",
		R_YesNo( R_RendererForwardPlusRuntimeReady() ),
		R_YesNo( R_RendererDeferredRuntimeReady() ),
		R_YesNo( R_RendererUnifiedClusteredRuntimeReady() ) );
	ri.Printf( PRINT_ALL, "framegraph : fbo=%s hdr=%d color=%s renderTarget=%ux%u\n",
		R_YesNo( vk.fboActive ), R_CvarInteger( r_hdr ), vk_format_string( vk.color_format ),
		vk.renderWidth, vk.renderHeight );
	ri.Printf( PRINT_ALL, "materials  : pbr=%d blend=%d gbuffer=%s fill=%d directExport=%s\n",
		R_CvarInteger( r_pbr ), R_CvarInteger( r_materialBlend ),
		R_YesNo( vk.deferredGbufferAllocated ), R_CvarInteger( r_deferredGBufferFill ),
		R_YesNo( vk.deferredGbufferDirectExport ) );
	ri.Printf( PRINT_ALL, "lighting   : mode=%d forwardPlus=%d shade=%.2f deferred=%d spec=%.2f ao=%.2f ssao=%d volumetric=%d\n",
		R_CvarInteger( r_renderMode ), R_CvarInteger( r_forwardPlus ), R_CvarValue( r_forwardPlusShade ),
		R_CvarInteger( r_deferredLighting ), R_CvarValue( r_deferredSpecularStrength ),
		R_CvarValue( r_deferredAOCoupling ), R_CvarInteger( r_ssao ), R_CvarInteger( r_volumetricFog ) );
	ri.Printf( PRINT_ALL, "temporal   : taa=%d motionVectors=%d frame=%u unreliable=%s history=%s\n",
		R_CvarInteger( r_taa ), R_CvarInteger( r_taaMotionVectors ), vk.temporal.frameIndex,
		R_YesNo( vk.temporal.unreliableMotionThisFrame ), R_YesNo( vk.temporal.hasValidTAAHistory ) );
	ri.Printf( PRINT_ALL, "post       : post=%d tonemap=%d exposure=%.2f bloom=%d smaa=%d fxaa=%d aaAfterBloom=%d\n",
		R_CvarInteger( r_post ), R_CvarInteger( r_tonemap ), R_CvarValue( r_exposure ),
		R_CvarInteger( r_bloom ), R_CvarInteger( r_ext_smaa ), R_CvarInteger( r_ext_fxaa ),
		R_CvarInteger( r_postAaAfterBloom ) );
	ri.Printf( PRINT_ALL, "gi/neural  : ndgi=%d niv=%s vfgi=%s nvc=%s\n",
		R_NamedCvarInteger( "r_ndgi" ),
		R_YesNo( vk.niv.volume_ready && vk.niv.shade_ready && vk.niv.composite_ready ),
		R_YesNo( vk.vfgi.buffers_ready && vk.vfgi.decode_ready && vk.vfgi.composite_ready ),
		R_YesNo( vk.nvc.weights_ready && vk.nvc.cache_ready && vk.nvc.restir_ready && vk.nvc.composite_ready ) );
	ri.Printf( PRINT_ALL, "ray        : available=%s rtx=%d demo=%d hybrid1=%d entities=%d\n",
		R_YesNo( R_RtxAvailable() ), R_CvarInteger( r_rtx ), R_CvarInteger( r_rtxDemo ),
		R_CvarInteger( r_hybrid1 ), R_CvarInteger( r_rtxEntities ) );
	ri.Printf( PRINT_ALL, "warnings   : %d\n", warnings );
	ri.Printf( PRINT_ALL, "=====================================\n" );
}

/*
===============
R_VolumetricAccurate_f
===============
Apply accurate volumetric fog integration defaults (physical composite, depth march).
*/
static void R_VolumetricAccurate_f( void )
{
	VK_ApplyVolumetricAccurateSettings();
}

/*
===============
R_VolumetricIntegration_f
===============
Set screen-space or froxel volumetric integration (see r_volumetricFogIntegration).
*/
static void R_VolumetricIntegration_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *arg = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";
	int mode = ( arg[0] >= '0' && arg[0] <= '9' ) ? ( arg[0] - '0' ) : -1;

	if ( mode < 0 ) {
		ri.Printf( PRINT_ALL,
			"usage: volumetric_integration <0|1|2|3>\n"
			"  0 = froxel grid march (default, accurate sim profile 2)\n"
			"  1 = screen analytical approximate (fast, Hoffman/Preetham style)\n"
			"  2 = screen ray march with sun shadow map per step\n"
			"  3 = OpenVDB Woodcock/delta tracking (majorant grid; needs vdb_bind_fog + upload)\n" );
		return;
	}

	if ( mode > 3 ) {
		mode = 3;
	}
	ri.Cvar_Set( "r_volumetricFogIntegration", va( "%d", mode ) );
	ri.Printf( PRINT_ALL, "Volumetric integration mode %d active.\n", mode );
}

/*
===============
R_SimRenderProfile_f
===============
Apply simulation/robotics render presets (AMBF-Vulkan alignment). Run vid_restart after.
*/
static void R_SimRenderProfile_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *arg = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";
	int profile = ( arg[0] >= '0' && arg[0] <= '9' ) ? ( arg[0] - '0' ) : -1;

	if ( profile < 0 ) {
		ri.Printf( PRINT_ALL,
			"usage: sim_render_profile <1|2>\n"
			"  1 = AMBF-Vulkan (arXiv:2410.05095): MSAA+FXAA, Reinhard, PBR, no volumetrics\n"
			"  2 = Simulation volumetrics: profile 1 + accurate froxel fog (64 steps, shadows, temporal)\n"
			"Run vid_restart after changing for full effect.\n" );
		return;
	}

	VK_ApplySimRenderProfile( profile );
	if ( r_simRenderProfile ) {
		ri.Cvar_Set( "r_simRenderProfile", va( "%d", profile ) );
	}
	ri.Printf( PRINT_ALL, "Run vid_restart to apply sim render profile %d.\n", profile );
}

/*
===============
R_SimRenderDebug_f
===============
Toggle simulation render debug overlay (console or ImGui HUD).
*/
static void R_SimRenderDebug_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *arg = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";
	int mode = ( arg[0] >= '0' && arg[0] <= '9' ) ? ( arg[0] - '0' ) : -1;

	if ( mode < 0 ) {
		ri.Printf( PRINT_ALL,
			"usage: sim_render_debug <0|1|2>\n"
			"  0 = off\n"
			"  1 = throttled console stats (post chain + volumetric GPU ms)\n"
			"  2 = ImGui HUD (requires r_imgui 1; falls back to console if ImGui off)\n" );
		return;
	}

	if ( r_simRenderDebug ) {
		ri.Cvar_Set( "r_simRenderDebug", va( "%d", mode ) );
	}
	VK_SimRenderDebugStartupLog();
}

#ifdef USE_VULKAN
#include "vk_fp64_points.h"

/*
===============
R_FP64_PointsGen_f
===============
*/
static void R_FP64_PointsGen_f( void )
{
	const int argc = ri.Cmd_Argc();
	int count = ( argc > 1 ) ? atoi( ri.Cmd_Argv( 1 ) ) : 10000;
	int dims = ( argc > 2 ) ? atoi( ri.Cmd_Argv( 2 ) ) : 2;

	if ( !VK_FP64_PointsGenerate( count, dims ) ) {
		ri.Printf( PRINT_WARNING, "fp64_points_gen failed (set r_fp64Points 1 and vid_restart for native fp64)\n" );
	}
}

/*
===============
R_FP64_PointsLoad_f
===============
*/
static void R_FP64_PointsLoad_f( void )
{
	const int argc = ri.Cmd_Argc();
	int dims = ( argc > 2 ) ? atoi( ri.Cmd_Argv( 2 ) ) : 2;

	if ( argc < 2 ) {
		ri.Printf( PRINT_ALL, "usage: fp64_points_load <csv.qpath> [2|3]\n" );
		return;
	}
	if ( !VK_FP64_PointsLoadCsv( ri.Cmd_Argv( 1 ), dims ) ) {
		ri.Printf( PRINT_WARNING, "fp64_points_load failed\n" );
	}
}

static void R_FP64_PointsClear_f( void )
{
	VK_FP64_PointsClear();
	ri.Printf( PRINT_ALL, "fp64_points_clear: dataset released\n" );
}

static void R_FP64_PointsBenchmark_f( void )
{
	const int argc = ri.Cmd_Argc();
	int frames = ( argc > 1 ) ? atoi( ri.Cmd_Argv( 1 ) ) : 60;

	VK_FP64_PointsBenchmark( frames );
}

/*
===============
R_Quality_f
===============
Apply AAA-style quality presets: 0=Low, 1=Medium, 2=High, 3=Ultra.
Requires vid_restart for some settings to take effect.
*/
static void R_Quality_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *arg = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";
	int preset = ( arg[0] >= '0' && arg[0] <= '3' ) ? ( arg[0] - '0' ) : -1;

	if ( preset < 0 ) {
		ri.Printf( PRINT_ALL,
			"usage: r_quality <0|1|2|3>\n"
			"  0 = Low    : Volumetric fog off, SSAO off, bloom off, SMAA off, SSR off\n"
			"  1 = Medium : Fog/SSAO/bloom/SMAA on (low quality), SSR off\n"
			"  2 = High   : Fog/SSAO/bloom/SMAA on (high quality), HBAO, SSR off\n"
			"  3 = Ultra  : All effects on, HBAO, SSR on, max quality\n"
			"Run vid_restart after changing for full effect.\n" );
		return;
	}

	switch ( preset ) {
		case 0: /* Low */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "0" );
			ri.Cvar_Set( "r_ssao", "0" );
			ri.Cvar_Set( "r_bloom", "0" );
			ri.Cvar_Set( "r_ext_smaa", "0" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_sharpen", "0.0" );
			ri.Cvar_Set( "r_exposure_auto", "0" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: Low (performance)\n" );
			break;
		case 1: /* Medium */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "1" );
			ri.Cvar_Set( "r_ssao", "1" );
			ri.Cvar_Set( "r_bloom", "1" );
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_volumetricFogQuality", "1" );
			ri.Cvar_Set( "r_ssaoMethod", "0" );
			ri.Cvar_Set( "r_smaa_preset", "2" );
			ri.Cvar_Set( "r_ssaoSamples", "12" );
			ri.Cvar_Set( "r_sharpen", "0.0" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: Medium\n" );
			break;
		case 2: /* High */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "1" );
			ri.Cvar_Set( "r_ssao", "1" );
			ri.Cvar_Set( "r_bloom", "1" );
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_volumetricFogQuality", "2" );
			ri.Cvar_Set( "r_ssaoMethod", "1" );
			ri.Cvar_Set( "r_smaa_preset", "3" );
			ri.Cvar_Set( "r_hbaoDirections", "8" );
			ri.Cvar_Set( "r_hbaoSteps", "8" );
			ri.Cvar_Set( "r_ssaoSamples", "16" );
			ri.Cvar_Set( "r_sharpen", "0.15" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: High (SMAA + 4x MSAA)\n" );
			break;
		case 3: /* Ultra */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "1" );
			ri.Cvar_Set( "r_ssao", "1" );
			ri.Cvar_Set( "r_bloom", "1" );
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "1" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "1" );
			ri.Cvar_Set( "r_volumetricFogQuality", "3" );
			ri.Cvar_Set( "r_ssaoMethod", "1" );
			ri.Cvar_Set( "r_smaa_preset", "4" );
			ri.Cvar_Set( "r_hbaoDirections", "16" );
			ri.Cvar_Set( "r_hbaoSteps", "16" );
			ri.Cvar_Set( "r_ssaoSamples", "24" );
			ri.Cvar_Set( "r_sharpen", "0.25" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: Ultra (SMAA + 4x MSAA + sample shading)\n" );
			break;
	}
	ri.Printf( PRINT_ALL, "Run vid_restart for full effect.\n" );
}

/*
===============
R_AAQuality_f
===============
Prefer high-quality spatial AA paths for Vulkan: SMAA, MSAA, and optional SSAA.
*/
static void R_AAQuality_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *arg = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";
	int preset = ( arg[0] >= '0' && arg[0] <= '4' ) ? ( arg[0] - '0' ) : -1;

	if ( preset < 0 ) {
		ri.Printf( PRINT_ALL,
			"usage: r_aaQuality <0|1|2|3|4>\n"
			"  0 = Off        : No AA\n"
			"  1 = SMAA       : SMAA High, no MSAA\n"
			"  2 = Balanced   : SMAA High + 2x MSAA\n"
			"  3 = High       : SMAA Ultra + 4x MSAA\n"
			"  4 = Extreme    : SMAA Ultra + 4x MSAA + sample shading + SSAA\n"
			"TAA is disabled in all presets. Run vid_restart for full effect.\n" );
		return;
	}

	ri.Cvar_Set( "r_taa", "0" );

	switch ( preset ) {
		case 0:
			ri.Cvar_Set( "r_ext_smaa", "0" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: Off\n" );
			break;
		case 1:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "3" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: SMAA High\n" );
			break;
		case 2:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "3" );
			ri.Cvar_Set( "r_ext_multisample", "2" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: SMAA High + 2x MSAA\n" );
			break;
		case 3:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "4" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: SMAA Ultra + 4x MSAA\n" );
			break;
		case 4:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "4" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "1" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "1.0" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "1" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: Extreme spatial AA (SMAA Ultra + 4x MSAA + SSAA)\n" );
			break;
	}

	ri.Printf( PRINT_ALL, "Run vid_restart for full effect.\n" );
}

static void VkVolumetricValidate_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *mode = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";

	if ( !mode[0] ) {
		ri.Printf( PRINT_ALL,
			"usage: vkVolumetricValidate <camera|ghosting|localspot|localpoint|msaa|checklist|reset>\n"
			"  camera [captureName]       : force one camera-cut reset and show reset overlay.\n"
			"  ghosting [captureName]     : show motion-vector threshold debug for fast camera tests.\n"
			"  localspot [captureName]    : show local spot-shadow visibility debug.\n"
			"  localpoint [captureName]   : show local point-shadow visibility debug.\n"
			"  msaa [capture base [msaa]] : queue off/on MSAA parity screenshots (+ optional sample count).\n"
			"  checklist                  : print exact validation commands for the final checklist.\n"
			"  reset                      : restore normal fog (r_fogDebug 0, r_volumetricFogValidation 0).\n" );
		return;
	}

	if ( !Q_stricmp( mode, "reset" ) ) {
		ri.Cvar_Set( "r_fogDebug", "0" );
		ri.Cvar_Set( "r_volumetricFogValidation", "0" );
		ri.Printf( PRINT_ALL, "[VK][fog] fog debug disabled: normal fog restored.\n" );
		return;
	}

	if ( !Q_stricmp( mode, "camera" ) ) {
		ri.Cvar_Set( "r_fogDebug", "10" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		ri.Cvar_SetValue( "r_volumetricFogForceCameraCut", 1.0f );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation camera-cut armed: debug=10, forceCut=1. When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "ghosting" ) ) {
		ri.Cvar_Set( "r_fogDebug", "7" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation ghosting view enabled: debug=7 (motion magnitude / threshold). When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "localspot" ) ) {
		ri.Cvar_Set( "r_fog_shadows", "1" );
		ri.Cvar_Set( "r_fogDebug", "8" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation local spot shadow view enabled: debug=8. When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "localpoint" ) ) {
		ri.Cvar_Set( "r_fog_shadows", "1" );
		ri.Cvar_Set( "r_fogDebug", "9" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation local point shadow view enabled: debug=9. When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "msaa" ) ) {
		const int current_msaa = ( r_ext_multisample ) ? r_ext_multisample->integer : 0;
		const char *base = ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) ? ri.Cmd_Argv( 2 ) : "";
		int target_msaa = ( current_msaa > 1 ) ? current_msaa : 4;

		if ( argc > 3 && ri.Cmd_Argv( 3 )[0] ) {
			target_msaa = atoi( ri.Cmd_Argv( 3 ) );
		}
		if ( target_msaa < 2 ) {
			target_msaa = 2;
		} else if ( target_msaa > 8 ) {
			target_msaa = 8;
		}

		if ( !base[0] ) {
			ri.Printf( PRINT_ALL,
				"[VK][fog] msaa parity helper:\n"
				"  run: vkVolumetricValidate msaa <captureBase> [targetSamples]\n"
				"  example: vkVolumetricValidate msaa fog_msaa_parity 4\n"
				"  current r_ext_multisample=%d target=%d\n",
				current_msaa, target_msaa );
			return;
		}

		ri.Cvar_Set( "r_fogDebug", "0" );
		ri.Cmd_ExecuteText( EXEC_APPEND, va(
			"set r_fogDebug 0\n"
			"set r_ext_multisample 0\n"
			"vid_restart\n"
			"wait\nwait\nwait\nwait\n"
			"screenshotJPEG %s_off\n"
			"set r_ext_multisample %d\n"
			"vid_restart\n"
			"wait\nwait\nwait\nwait\n"
			"screenshotJPEG %s_on\n",
			base, target_msaa, base ) );

		if ( current_msaa != target_msaa ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va(
				"set r_ext_multisample %d\n"
				"vid_restart\n",
				current_msaa ) );
		}

		ri.Printf( PRINT_ALL, "[VK][fog] queued MSAA parity capture: base=%s (off vs %dx)\n", base, target_msaa );
		return;
	}

	if ( !Q_stricmp( mode, "checklist" ) ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] final checklist commands:\n"
			"  camera cut reset       : vkVolumetricValidate camera fog_camera_cut\n"
			"  fast camera ghosting   : vkVolumetricValidate ghosting fog_ghosting\n"
			"  local spot shadows     : vkVolumetricValidate localspot fog_local_spot\n"
			"  local point shadows    : vkVolumetricValidate localpoint fog_local_point\n"
			"  msaa parity capture    : vkVolumetricValidate msaa fog_msaa_parity 4\n" );
		return;
	}

	ri.Printf( PRINT_WARNING, "vkVolumetricValidate: unknown mode '%s'\n", mode );
}
#endif
