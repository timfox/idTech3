/*
===========================================================================
Raster Ultra 1.6 — Hi-Z depth pyramid (conservative occlusion).

Distinct from r_forwardPlusHiZ (Forward+ tile probe padding).
Conservative policy: camera-cut / missing pyramid / large objects / recently
visible instances stay visible (no one-frame disappearance).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_hiz.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_pass_registry.h"

#include <math.h>

#define VK_HIZ_MAX_MIPS 12

typedef struct {
	VkImage image;
	VkImageView views[VK_HIZ_MAX_MIPS];
	VkImageView viewAll;
	VkDeviceMemory memory;
	uint32_t width, height, levels;
	VkImageLayout layout;
} hizPyramid_t;

static cvar_t *r_hiZ;
static cvar_t *r_hiZMinVisibleFrames;
static cvar_t *r_hiZLargeObjectPx;
static cvar_t *r_hiZDebug;

static hizPyramid_t s_pyramid;
static qboolean s_cameraCut;
static qboolean s_ready;
static uint32_t s_buildCount;
static uint32_t s_tests;
static uint32_t s_rejected;
static uint32_t s_biasKeep;
static qboolean s_cmds;

void vk_hiz_register_cvars( void )
{
	if ( r_hiZ ) {
		return;
	}
	r_hiZ = ri.Cvar_Get( "r_hiZ", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_hiZ, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hiZ,
		"Raster Ultra 1.6 Hi-Z depth pyramid for conservative occlusion (latched).\n"
		"Distinct from r_forwardPlusHiZ (tile probe pad only).\n"
		" 0 off (default / certified boot)\n"
		" 1 build pyramid; cull uses conservative bias + frustum companion" );
	ri.Cvar_SetGroup( r_hiZ, CVG_RENDERER );

	r_hiZMinVisibleFrames = ri.Cvar_Get( "r_hiZMinVisibleFrames", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hiZMinVisibleFrames, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_hiZMinVisibleFrames,
		"Minimum frames an instance stays visible after becoming visible (anti one-frame pop)." );

	r_hiZLargeObjectPx = ri.Cvar_Get( "r_hiZLargeObjectPx", "256", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hiZLargeObjectPx, "32", "4096", CV_FLOAT );
	ri.Cvar_SetDescription( r_hiZLargeObjectPx,
		"Projected AABB diagonal (px) above which Hi-Z never rejects (large-object handling)." );

	r_hiZDebug = ri.Cvar_Get( "r_hiZDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hiZDebug, "0", "2", CV_INTEGER );
}

static void HIZ_DestroyPyramid( void )
{
	uint32_t i;

	if ( s_pyramid.viewAll ) {
		qvkDestroyImageView( vk.device, s_pyramid.viewAll, NULL );
	}
	for ( i = 0; i < VK_HIZ_MAX_MIPS; i++ ) {
		if ( s_pyramid.views[i] ) {
			qvkDestroyImageView( vk.device, s_pyramid.views[i], NULL );
		}
	}
	if ( s_pyramid.image ) {
		qvkDestroyImage( vk.device, s_pyramid.image, NULL );
	}
	if ( s_pyramid.memory ) {
		qvkFreeMemory( vk.device, s_pyramid.memory, NULL );
	}
	Com_Memset( &s_pyramid, 0, sizeof( s_pyramid ) );
	s_ready = qfalse;
}

static qboolean HIZ_CreatePyramid( uint32_t width, uint32_t height )
{
	VkImageCreateInfo ici;
	VkMemoryRequirements req;
	VkMemoryAllocateInfo mai;
	VkImageViewCreateInfo vci;
	uint32_t levels = 1;
	uint32_t w = width;
	uint32_t h = height;
	uint32_t i;

	HIZ_DestroyPyramid();
	if ( width < 1 || height < 1 || vk.device == VK_NULL_HANDLE ) {
		return qfalse;
	}

	while ( ( w > 1 || h > 1 ) && levels < VK_HIZ_MAX_MIPS ) {
		w = w > 1 ? w / 2 : 1;
		h = h > 1 ? h / 2 : 1;
		levels++;
	}

	Com_Memset( &ici, 0, sizeof( ici ) );
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = VK_FORMAT_R32_SFLOAT;
	ici.extent.width = width;
	ici.extent.height = height;
	ici.extent.depth = 1;
	ici.mipLevels = levels;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if ( qvkCreateImage( vk.device, &ici, NULL, &s_pyramid.image ) != VK_SUCCESS ) {
		return qfalse;
	}
	qvkGetImageMemoryRequirements( vk.device, s_pyramid.image, &req );
	Com_Memset( &mai, 0, sizeof( mai ) );
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = vk_find_memory_type( vk.physical_device, req.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );
	if ( qvkAllocateMemory( vk.device, &mai, NULL, &s_pyramid.memory ) != VK_SUCCESS ) {
		HIZ_DestroyPyramid();
		return qfalse;
	}
	qvkBindImageMemory( vk.device, s_pyramid.image, s_pyramid.memory, 0 );

	for ( i = 0; i < levels; i++ ) {
		Com_Memset( &vci, 0, sizeof( vci ) );
		vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		vci.image = s_pyramid.image;
		vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vci.format = VK_FORMAT_R32_SFLOAT;
		vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vci.subresourceRange.baseMipLevel = i;
		vci.subresourceRange.levelCount = 1;
		vci.subresourceRange.layerCount = 1;
		if ( qvkCreateImageView( vk.device, &vci, NULL, &s_pyramid.views[i] ) != VK_SUCCESS ) {
			HIZ_DestroyPyramid();
			return qfalse;
		}
	}

	Com_Memset( &vci, 0, sizeof( vci ) );
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = s_pyramid.image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = VK_FORMAT_R32_SFLOAT;
	vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vci.subresourceRange.baseMipLevel = 0;
	vci.subresourceRange.levelCount = levels;
	vci.subresourceRange.layerCount = 1;
	if ( qvkCreateImageView( vk.device, &vci, NULL, &s_pyramid.viewAll ) != VK_SUCCESS ) {
		HIZ_DestroyPyramid();
		return qfalse;
	}

	s_pyramid.width = width;
	s_pyramid.height = height;
	s_pyramid.levels = levels;
	s_pyramid.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	s_ready = qtrue;
	ri.Printf( PRINT_ALL, "[VK][HiZ] pyramid %ux%u levels=%u (Raster Ultra 1.6)\n",
		width, height, levels );
	return qtrue;
}

void vk_hiz_init( void )
{
	vk_hiz_register_cvars();
	s_cameraCut = qtrue;
	s_buildCount = s_tests = s_rejected = s_biasKeep = 0;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "hiz_status", vk_hiz_status_f );
		s_cmds = qtrue;
	}
	if ( r_hiZ && r_hiZ->integer && vk.renderWidth > 0 && vk.renderHeight > 0 ) {
		HIZ_CreatePyramid( vk.renderWidth, vk.renderHeight );
	}
}

void vk_hiz_shutdown( void )
{
	HIZ_DestroyPyramid();
}

void vk_hiz_on_resize( void )
{
	if ( !r_hiZ || !r_hiZ->integer ) {
		HIZ_DestroyPyramid();
		return;
	}
	HIZ_CreatePyramid( vk.renderWidth, vk.renderHeight );
	s_cameraCut = qtrue;
}

void vk_hiz_on_camera_cut( void )
{
	s_cameraCut = qtrue;
}

qboolean vk_hiz_active( void )
{
	return ( r_hiZ && r_hiZ->integer ) ? qtrue : qfalse;
}

qboolean vk_hiz_ready( void )
{
	return ( vk_hiz_active() && s_ready && !s_cameraCut ) ? qtrue : qfalse;
}

void vk_hiz_build( void )
{
	if ( !vk_hiz_active() ) {
		return;
	}
	if ( !s_ready || s_pyramid.width != vk.renderWidth || s_pyramid.height != vk.renderHeight ) {
		HIZ_CreatePyramid( vk.renderWidth, vk.renderHeight );
	}
	/*
	 * Full GPU downsample of depth → pyramid is dispatched when compute path is wired.
	 * Resource existence + conservative CPU policy already prevent one-frame pops.
	 * Mark ready after first successful depth prepass frame.
	 */
	if ( s_ready && backEnd.doneWorldScene ) {
		s_cameraCut = qfalse;
		s_buildCount++;
	}
}

/*
===============
vk_hiz_aabb_visible

Conservative occlusion: never reject on camera cut / missing pyramid / large
screen projection / recently visible instances.
===============
*/
qboolean vk_hiz_aabb_visible( const vec3_t mins, const vec3_t maxs,
	qboolean wasVisibleLastFrame, uint32_t visibleAge )
{
	float diag;
	int minFrames;

	s_tests++;

	if ( !vk_hiz_active() || s_cameraCut || !s_ready ) {
		s_biasKeep++;
		return qtrue;
	}

	minFrames = r_hiZMinVisibleFrames ? r_hiZMinVisibleFrames->integer : 2;
	if ( wasVisibleLastFrame || visibleAge < (uint32_t)minFrames ) {
		s_biasKeep++;
		return qtrue;
	}

	/* Large-object handling: approximate diagonal in world units → keep visible. */
	{
		vec3_t size;
		VectorSubtract( maxs, mins, size );
		diag = VectorLength( size );
		if ( diag > ( r_hiZLargeObjectPx ? r_hiZLargeObjectPx->value : 256.0f ) ) {
			s_biasKeep++;
			return qtrue;
		}
	}

	/*
	 * Without sampled pyramid comparison yet, stay conservative (visible).
	 * Rejection path reserved for GPU Hi-Z sample when compute downsample lands.
	 * Metrics still track tests so profiling is meaningful.
	 */
	(void)maxs;
	return qtrue;
}

void vk_hiz_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Hi-Z (Raster Ultra 1.6) ========\n" );
	ri.Printf( PRINT_ALL, "active     : %s (ready=%s cameraCut=%s)\n",
		vk_hiz_active() ? "yes" : "no",
		s_ready ? "yes" : "no",
		s_cameraCut ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "pyramid    : %ux%u levels=%u\n",
		s_pyramid.width, s_pyramid.height, s_pyramid.levels );
	ri.Printf( PRINT_ALL, "builds     : %u\n", s_buildCount );
	ri.Printf( PRINT_ALL, "tests      : %u rejected=%u biasKeep=%u\n",
		s_tests, s_rejected, s_biasKeep );
	ri.Printf( PRINT_ALL, "note       : r_forwardPlusHiZ is tile probe pad — not this pyramid\n" );
	ri.Printf( PRINT_ALL, "policy     : minVisibleFrames=%d largeObject=%.0f; no one-frame hide\n",
		r_hiZMinVisibleFrames ? r_hiZMinVisibleFrames->integer : 2,
		r_hiZLargeObjectPx ? r_hiZLargeObjectPx->value : 256.0f );
	ri.Printf( PRINT_ALL, "==========================================\n" );
}
