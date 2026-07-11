/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Virtual texture scaffold (chocolate): physical page atlas + CPU page table.
Not sparse VkImage residency; not a full world UV rewrite. See docs/VIRTUAL_TEXTURE.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_vt.h"
#include "vk_texture_image.h"

#define VT_PAGE_SIZE     128
#define VT_ATLAS_PAGES_X 8
#define VT_ATLAS_PAGES_Y 8
#define VT_MAX_PAGES     ( VT_ATLAS_PAGES_X * VT_ATLAS_PAGES_Y )
#define VT_ATLAS_W       ( VT_PAGE_SIZE * VT_ATLAS_PAGES_X )
#define VT_ATLAS_H       ( VT_PAGE_SIZE * VT_ATLAS_PAGES_Y )

typedef struct {
	qboolean used;
	int      virtualId;
	char     name[MAX_QPATH];
} vtPageSlot_t;

static cvar_t *r_vt;
static image_t *s_atlas;
static vtPageSlot_t s_slots[VT_MAX_PAGES];
static int s_nextSlot;
static qboolean s_cmds;
static int s_pageHits;
static int s_pageMisses;

static void VT_Status_f( void )
{
	int used = 0;
	int i;

	for ( i = 0; i < VT_MAX_PAGES; i++ ) {
		if ( s_slots[i].used ) {
			used++;
		}
	}
	ri.Printf( PRINT_ALL,
		"[VK][VT] active=%d atlas=%dx%d page=%d slots=%d/%d hits=%d misses=%d\n",
		R_VT_Active() ? 1 : 0,
		VT_ATLAS_W, VT_ATLAS_H, VT_PAGE_SIZE,
		used, VT_MAX_PAGES, s_pageHits, s_pageMisses );
}

static void VT_Flush_f( void )
{
	Com_Memset( s_slots, 0, sizeof( s_slots ) );
	s_nextSlot = 0;
	s_pageHits = s_pageMisses = 0;
	ri.Printf( PRINT_ALL, "[VK][VT] page table flushed\n" );
}

static void VT_Load_f( void )
{
	byte *buf = NULL;
	int len;
	char name[MAX_QPATH];
	int w, h, page;

	if ( !R_VT_Active() ) {
		ri.Printf( PRINT_ALL, "[VK][VT] r_vt 0 — enable and vid_restart\n" );
		return;
	}
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: vt_load <imagepath>\n" );
		return;
	}
	Q_strncpyz( name, ri.Cmd_Argv( 1 ), sizeof( name ) );
	len = ri.FS_ReadFile( name, (void **)&buf );
	if ( len <= 0 || !buf ) {
		/* Procedural demo page when file missing */
		byte solid[VT_PAGE_SIZE * VT_PAGE_SIZE * 4];
		int i;
		for ( i = 0; i < VT_PAGE_SIZE * VT_PAGE_SIZE; i++ ) {
			solid[i * 4 + 0] = (byte)( ( i * 37 ) & 255 );
			solid[i * 4 + 1] = (byte)( ( i * 17 ) & 255 );
			solid[i * 4 + 2] = 180;
			solid[i * 4 + 3] = 255;
		}
		page = R_VT_LoadPageRGBA( solid, VT_PAGE_SIZE, VT_PAGE_SIZE, name );
		ri.Printf( PRINT_ALL, "[VK][VT] vt_load procedural page=%d name=%s\n", page, name );
		return;
	}
	/* Prefer R_LoadImage if available via engine — fall back to procedural on raw read */
	ri.FS_FreeFile( buf );
	{
		byte solid[VT_PAGE_SIZE * VT_PAGE_SIZE * 4];
		int i;
		for ( i = 0; i < VT_PAGE_SIZE * VT_PAGE_SIZE; i++ ) {
			solid[i * 4 + 0] = 200;
			solid[i * 4 + 1] = 160;
			solid[i * 4 + 2] = 80;
			solid[i * 4 + 3] = 255;
		}
		w = h = VT_PAGE_SIZE;
		page = R_VT_LoadPageRGBA( solid, w, h, name );
		ri.Printf( PRINT_ALL, "[VK][VT] vt_load page=%d name=%s (scaffold solid)\n", page, name );
	}
}

void R_VT_Init( void )
{
	r_vt = ri.Cvar_Get( "r_vt", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vt, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vt,
		"Virtual texture scaffold: physical page atlas + CPU page table (MegaTexture-class lite). Default 0." );
	ri.Cvar_SetGroup( r_vt, CVG_RENDERER );

	Com_Memset( s_slots, 0, sizeof( s_slots ) );
	s_nextSlot = 0;
	s_atlas = NULL;
	s_pageHits = s_pageMisses = 0;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "vt_status", VT_Status_f );
		ri.Cmd_AddCommand( "vt_flush", VT_Flush_f );
		ri.Cmd_AddCommand( "vt_load", VT_Load_f );
		s_cmds = qtrue;
	}

	if ( r_vt->integer ) {
		byte *blank;
		int bytes = VT_ATLAS_W * VT_ATLAS_H * 4;
		blank = (byte *)ri.Hunk_AllocateTempMemory( bytes );
		if ( blank ) {
			Com_Memset( blank, 32, bytes );
			s_atlas = R_CreateImage( "*vt_atlas", NULL, blank, VT_ATLAS_W, VT_ATLAS_H, IMGFLAG_CLAMPTOEDGE, 0, 0 );
			ri.Hunk_FreeTempMemory( blank );
		}
		ri.Printf( PRINT_ALL, "[VK][VT] r_vt=1 atlas %dx%d page %d (%d slots)\n",
			VT_ATLAS_W, VT_ATLAS_H, VT_PAGE_SIZE, VT_MAX_PAGES );
	}
}

void R_VT_Shutdown( void )
{
	s_atlas = NULL;
	Com_Memset( s_slots, 0, sizeof( s_slots ) );
}

qboolean R_VT_Active( void )
{
	return ( r_vt && r_vt->integer && s_atlas ) ? qtrue : qfalse;
}

image_t *R_VT_AtlasImage( void )
{
	return s_atlas;
}

int R_VT_Lookup( int virtualPage )
{
	int i;
	for ( i = 0; i < VT_MAX_PAGES; i++ ) {
		if ( s_slots[i].used && s_slots[i].virtualId == virtualPage ) {
			s_pageHits++;
			return i;
		}
	}
	s_pageMisses++;
	return -1;
}

int R_VT_LoadPageRGBA( const byte *rgba, int width, int height, const char *name )
{
	int slot;
	int px, py;
	byte page[VT_PAGE_SIZE * VT_PAGE_SIZE * 4];
	int y, x;

	if ( !R_VT_Active() || !rgba ) {
		return -1;
	}

	slot = s_nextSlot % VT_MAX_PAGES;
	s_nextSlot++;

	Com_Memset( page, 0, sizeof( page ) );
	for ( y = 0; y < VT_PAGE_SIZE && y < height; y++ ) {
		for ( x = 0; x < VT_PAGE_SIZE && x < width; x++ ) {
			int dst = ( y * VT_PAGE_SIZE + x ) * 4;
			int src = ( y * width + x ) * 4;
			page[dst + 0] = rgba[src + 0];
			page[dst + 1] = rgba[src + 1];
			page[dst + 2] = rgba[src + 2];
			page[dst + 3] = rgba[src + 3];
		}
	}

	px = ( slot % VT_ATLAS_PAGES_X ) * VT_PAGE_SIZE;
	py = ( slot / VT_ATLAS_PAGES_X ) * VT_PAGE_SIZE;
	vk_upload_image_data( s_atlas, px, py, VT_PAGE_SIZE, VT_PAGE_SIZE, 1, page,
		VT_PAGE_SIZE * VT_PAGE_SIZE * 4, qtrue );

	s_slots[slot].used = qtrue;
	s_slots[slot].virtualId = slot;
	if ( name ) {
		Q_strncpyz( s_slots[slot].name, name, sizeof( s_slots[slot].name ) );
	} else {
		s_slots[slot].name[0] = '\0';
	}
	return slot;
}
