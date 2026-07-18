/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan device and format selection helpers.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_device.h"
#include "vk_util.h"

static VkFormat get_depth_format( VkPhysicalDevice physical_device )
{
	VkFormatProperties props;
	VkFormat formats[2];
	int i;

	if ( glConfig.stencilBits > 0 ) {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM_S8_UINT : VK_FORMAT_D24_UNORM_S8_UINT;
		formats[1] = VK_FORMAT_D32_SFLOAT_S8_UINT;
	} else {
		formats[0] = glConfig.depthBits == 16 ? VK_FORMAT_D16_UNORM : VK_FORMAT_X8_D24_UNORM_PACK32;
		formats[1] = VK_FORMAT_D32_SFLOAT;
	}

	for ( i = 0; (size_t) i < ARRAY_LEN( formats ); i++ ) {
		qvkGetPhysicalDeviceFormatProperties( physical_device, formats[i], &props );
		if ( ( props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ) != 0 ) {
			return formats[i];
		}
	}

	ri.Error( ERR_FATAL, "get_depth_format: failed to find depth attachment format" );
	return VK_FORMAT_UNDEFINED;
}

static qboolean vk_blit_enabled( VkPhysicalDevice physical_device, const VkFormat srcFormat, const VkFormat dstFormat )
{
	VkFormatProperties formatProps;

	qvkGetPhysicalDeviceFormatProperties( physical_device, srcFormat, &formatProps );
	if ( ( formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == 0 ) {
		return qfalse;
	}

	qvkGetPhysicalDeviceFormatProperties( physical_device, dstFormat, &formatProps );
	if ( ( formatProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == 0 ) {
		return qfalse;
	}

	return qtrue;
}

qboolean vk_format_has_features( VkPhysicalDevice physical_device, VkFormat format, VkFormatFeatureFlags required )
{
	VkFormatProperties props;
	qvkGetPhysicalDeviceFormatProperties( physical_device, format, &props );
	return ( props.optimalTilingFeatures & required ) == required;
}

static qboolean vk_hdr_format_supported( VkPhysicalDevice physical_device, VkFormat format )
{
	const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
	return vk_format_has_features( physical_device, format, required );
}

static VkFormat get_hdr_format( VkPhysicalDevice physical_device, VkFormat base_format )
{
	const int hdr_mode = r_hdr ? r_hdr->integer : 0;
	const VkFormat hdr16 = VK_FORMAT_R16G16B16A16_SFLOAT;
	const VkFormat hdr32 = VK_FORMAT_R32G32B32A32_SFLOAT;

	if ( r_fbo->integer == 0 ) {
		return base_format;
	}

	switch ( hdr_mode ) {
		case -1:
			if ( vk_hdr_format_supported( physical_device, VK_FORMAT_B4G4R4A4_UNORM_PACK16 ) ) {
				return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
			}
			ri.Printf( PRINT_WARNING, "[VK][fbo] r_hdr -1 requested %s but it's unsupported; falling back to %s\n",
				vk_format_string( VK_FORMAT_B4G4R4A4_UNORM_PACK16 ),
				vk_format_string( base_format ) );
			return base_format;
		case 1:
			if ( vk_hdr_format_supported( physical_device, hdr16 ) ) {
				return hdr16;
			}
			ri.Printf( PRINT_WARNING, "[VK][fbo] r_hdr 1 requested %s but it's unsupported; falling back to %s\n",
				vk_format_string( hdr16 ),
				vk_format_string( base_format ) );
			return base_format;
		case 2: {
			if ( vk_hdr_format_supported( physical_device, hdr32 ) ) {
				return hdr32;
			}
			if ( vk_hdr_format_supported( physical_device, hdr16 ) ) {
				ri.Printf( PRINT_WARNING, "[VK][fbo] r_hdr 2 requested %s but it's unsupported; falling back to %s\n",
					vk_format_string( hdr32 ),
					vk_format_string( hdr16 ) );
				return hdr16;
			}
			ri.Printf( PRINT_WARNING, "[VK][fbo] r_hdr 2 requested %s/%s but both are unsupported; falling back to %s\n",
				vk_format_string( hdr32 ),
				vk_format_string( hdr16 ),
				vk_format_string( base_format ) );
			return base_format;
		}
		case 3: {
			/* Honest alias: true RGBA64F color RTs are not wired (no dvec4 fragment path). */
			ri.Printf( PRINT_WARNING,
				"[VK][fbo] r_hdr 3 aliases to 32-bit HDR (r_hdr 2); RGBA64F color output is not implemented\n" );
			if ( vk_hdr_format_supported( physical_device, hdr32 ) ) {
				return hdr32;
			}
			if ( vk_hdr_format_supported( physical_device, hdr16 ) ) {
				ri.Printf( PRINT_WARNING, "[VK][fbo] r_hdr 3→2 fallback %s unsupported; using %s\n",
					vk_format_string( hdr32 ),
					vk_format_string( hdr16 ) );
				return hdr16;
			}
			ri.Printf( PRINT_WARNING, "[VK][fbo] r_hdr 3→2 fallback %s/%s unsupported; using %s\n",
				vk_format_string( hdr32 ),
				vk_format_string( hdr16 ),
				vk_format_string( base_format ) );
			return base_format;
		}
		default: return base_format;
	}
}

static VkFormat get_bloom_format( VkPhysicalDevice physical_device, VkFormat fallback )
{
	const VkFormat preferred[] = {
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
		VK_FORMAT_A2R10G10B10_UNORM_PACK32,
		VK_FORMAT_B10G11R11_UFLOAT_PACK32
	};
	const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
		VK_FORMAT_FEATURE_BLIT_SRC_BIT |
		VK_FORMAT_FEATURE_BLIT_DST_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
	uint32_t i;

	for ( i = 0; i < ARRAY_LEN( preferred ); i++ ) {
		const VkFormat fmt = preferred[i];
		if ( fmt == fallback ) {
			return fmt;
		}
		if ( vk_format_has_features( physical_device, fmt, required ) ) {
			return fmt;
		}
	}

	return fallback;
}

typedef struct {
	int bits;
	VkFormat rgb;
	VkFormat bgr;
} present_format_t;

static const present_format_t present_formats[] = {
	{16, VK_FORMAT_B5G6R5_UNORM_PACK16, VK_FORMAT_R5G6B5_UNORM_PACK16},
	{24, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB},
	{30, VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32},
};

static void get_present_format( int present_bits, VkFormat *bgr, VkFormat *rgb )
{
	const present_format_t *pf, *sel;
	int i;

	sel = NULL;
	pf = present_formats;
	for ( i = 0; (size_t) i < ARRAY_LEN( present_formats ); i++, pf++ ) {
		if ( pf->bits <= present_bits ) {
			sel = pf;
		}
	}
	if ( !sel ) {
		*bgr = VK_FORMAT_B8G8R8A8_UNORM;
		*rgb = VK_FORMAT_R8G8B8A8_UNORM;
	} else {
		*bgr = sel->bgr;
		*rgb = sel->rgb;
	}
}

qboolean vk_select_surface_format( VkPhysicalDevice physical_device, VkSurfaceKHR surface )
{
	VkFormat base_bgr, base_rgb;
	VkFormat ext_bgr, ext_rgb;
	VkSurfaceFormatKHR *candidates;
	uint32_t format_count;
	VkResult res;

	res = qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, NULL );
	if ( res < 0 ) {
		ri.Printf( PRINT_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string( res ) );
		return qfalse;
	}

	if ( format_count == 0 ) {
		ri.Printf( PRINT_ERROR, "...no surface formats found\n" );
		return qfalse;
	}

	candidates = (VkSurfaceFormatKHR*)ri.Malloc( format_count * sizeof(VkSurfaceFormatKHR) );

	VK_CHECK( qvkGetPhysicalDeviceSurfaceFormatsKHR( physical_device, surface, &format_count, candidates ) );

	get_present_format( 24, &base_bgr, &base_rgb );

	if ( r_fbo->integer ) {
		get_present_format( r_presentBits->integer, &ext_bgr, &ext_rgb );
	} else {
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if ( format_count == 1 && candidates[0].format == VK_FORMAT_UNDEFINED ) {
		vk.base_format.format = base_bgr;
		vk.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		vk.present_format.format = ext_bgr;
		vk.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else {
		uint32_t i;
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == base_bgr || candidates[i].format == base_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.base_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.base_format = candidates[0];
		}
		for ( i = 0; i < format_count; i++ ) {
			if ( ( candidates[i].format == ext_bgr || candidates[i].format == ext_rgb ) && candidates[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) {
				vk.present_format = candidates[i];
				break;
			}
		}
		if ( i == format_count ) {
			vk.present_format = vk.base_format;
		}
	}

	if ( !r_fbo->integer ) {
		vk.present_format = vk.base_format;
	}

	if ( r_vk_swapchain_srgb ) {
		ri.Cvar_Set( "r_vk_swapchain_srgb", vk_format_is_srgb( vk.present_format.format ) ? "1" : "0" );
	}

	ri.Printf( PRINT_ALL,
		"[VK] Swapchain format %s (%s) colorSpace=%s | Policy A: %s\n",
		vk_format_string( vk.present_format.format ),
		vk_format_is_srgb( vk.present_format.format ) ? "sRGB format" : "UNORM format",
		( vk.present_format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR ) ? "SRGB_NONLINEAR" : "other",
		vk_format_is_srgb( vk.present_format.format )
			? "shader outputs linear (GPU converts on write)"
			: "shader applies manual gamma in gamma.frag when apply_srgb_gamma=1" );

	ri.Free( candidates );

	return qtrue;
}

void vk_setup_surface_formats( VkPhysicalDevice physical_device )
{
	vk.depth_format = get_depth_format( physical_device );

	vk.color_format = get_hdr_format( physical_device, vk.base_format.format );

	if ( r_fbo && r_fbo->integer ) {
		const int hdr_mode = r_hdr ? r_hdr->integer : 0;
		ri.Printf( PRINT_ALL, "[VK][fbo] hdr select r_hdr=%d base=%s color=%s present=%s depth=%s\n",
			hdr_mode,
			vk_format_string( vk.base_format.format ),
			vk_format_string( vk.color_format ),
			vk_format_string( vk.present_format.format ),
			vk_format_string( vk.depth_format ) );
		ri.Printf( PRINT_ALL,
			"[VK][post] r_post=%d r_exposure=%.3f r_tonemap=%d r_post_debug=%d r_vk_clearhdr=%d r_vk_disableblend=%d r_vk_bindlog=%d\n",
			r_post ? r_post->integer : 0,
			r_exposure ? r_exposure->value : 1.0f,
			r_tonemap ? r_tonemap->integer : 0,
			r_post_debug ? r_post_debug->integer : 0,
			r_vk_clearhdr ? r_vk_clearhdr->integer : 1,
			r_vk_disableblend ? r_vk_disableblend->integer : 1,
			r_vk_bindlog ? r_vk_bindlog->integer : 0 );
		ri.Printf( PRINT_ALL,
			"[VK][colorspace] PBR albedo uses VK_FORMAT_*_SRGB; normal/ORM/data maps use linear UNORM. Intermediate HDR target is linear FP (not sRGB).\n" );
		if ( hdr_mode > 0 && vk.color_format == vk.base_format.format ) {
			ri.Printf( PRINT_WARNING, "[VK][fbo] r_hdr %d fell back to base format %s\n",
				hdr_mode, vk_format_string( vk.base_format.format ) );
		}
	}

	vk.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	vk.bloom_format = get_bloom_format( physical_device, vk.color_format );
	vk.ssao_format = VK_FORMAT_R8_UNORM;

	vk.blitEnabled = vk_blit_enabled( physical_device, vk.color_format, vk.capture_format );

	if ( !vk.blitEnabled ) {
		vk.capture_format = vk.color_format;
	}
}

const char *vk_device_renderer_name( const VkPhysicalDeviceProperties *props )
{
	static char buf[sizeof( props->deviceName ) + 64];
	const char *device_type;

	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: device_type = "Integrated"; break;
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: device_type = "Discrete"; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: device_type = "Virtual"; break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU: device_type = "CPU"; break;
		default: device_type = "OTHER"; break;
	}

	Com_sprintf( buf, sizeof( buf ), "%s %s, 0x%04x",
		device_type, props->deviceName, props->deviceID );

	return buf;
}

qboolean vk_device_is_v3dv( const VkPhysicalDeviceProperties *props )
{
	/* V3DV (Mesa) reports "V3D 4.x" or "V3DV" in deviceName on Raspberry Pi 4/5 */
	const char *name = props->deviceName;
	if ( !name || !name[0] )
		return qfalse;
	return ( Q_stristr( name, "V3D" ) != NULL ) ? qtrue : qfalse;
}
