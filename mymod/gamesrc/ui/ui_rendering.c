/*
=======================================================================

RENDERING OPTIONS MENU

=======================================================================
*/

#include "ui_local.h"


#define ART_FRAMEL			"menu/" MENU_ART_DIR "/frame2_l"
#define ART_FRAMER			"menu/" MENU_ART_DIR "/frame1_r"
#define ART_BACK0			"menu/" MENU_ART_DIR "/back_0"
#define ART_BACK1			"menu/" MENU_ART_DIR "/back_1"

#define ID_GRAPHICS			10
#define ID_DISPLAY			11
#define ID_SOUND			12
#define ID_NETWORK			13
#define ID_RENDERING			14
#define ID_PATHTRACING		15
#define ID_PT_SAMPLES		16
#define ID_PT_BOUNCES		17
#define ID_PT_MAXDEPTH		18
#define ID_PT_DENOISE		19
#define ID_PT_DENOISEITER	20
#define ID_PT_TEMPORAL		21
#define ID_PT_TEMPORALALPHA	22
#define ID_PT_GI			23
#define ID_PT_GIINTENSITY	24
#define ID_PT_OUTPUTSCALE	25
#define ID_BACK				26


const char *pt_samples_items[] = {
	"1",
	"2",
	"4",
	"8",
	"16",
	"32",
	"64",
	"128",
	"256",
	NULL
};

const char *pt_bounces_items[] = {
	"1",
	"2",
	"4",
	"6",
	"8",
	"12",
	"16",
	"24",
	"32",
	NULL
};

const char *pt_maxdepth_items[] = {
	"4",
	"8",
	"12",
	"16",
	"24",
	"32",
	"48",
	"64",
	NULL
};

const char *pt_denoiseiter_items[] = {
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	NULL
};

const char *yesno_names[] = {
	"OFF",
	"ON",
	NULL
};

const char *outputscale_items[] = {
	"0.25x",
	"0.5x",
	"0.75x",
	"1.0x",
	"1.25x",
	"1.5x",
	"2.0x",
	NULL
};


typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menutext_s		graphics;
	menutext_s		display;
	menutext_s		sound;
	menutext_s		network;
	menutext_s		rendering;

	menulist_s		pathtracing;
	menulist_s		pt_samples;
	menulist_s		pt_bounces;
	menulist_s		pt_maxdepth;
	menulist_s		pt_denoise;
	menulist_s		pt_denoiseiter;
	menulist_s		pt_temporal;
	menuslider_s	pt_temporalalpha;
	menulist_s		pt_gi;
	menuslider_s	pt_giintensity;
	menulist_s		pt_outputscale;

	menubitmap_s	back;
} renderingOptionsInfo_t;

static renderingOptionsInfo_t	renderingOptionsInfo;


/*
=================
UI_RenderingOptionsMenu_Event
=================
*/
static void UI_RenderingOptionsMenu_Event( void* ptr, int event ) {
	if( event != QM_ACTIVATED ) {
		return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
	case ID_GRAPHICS:
		UI_PopMenu();
		UI_GraphicsOptionsMenu();
		break;

	case ID_DISPLAY:
		UI_PopMenu();
		UI_DisplayOptionsMenu();
		break;

	case ID_SOUND:
		UI_PopMenu();
		UI_SoundOptionsMenu();
		break;

	case ID_NETWORK:
		UI_PopMenu();
		UI_NetworkOptionsMenu();
		break;

	case ID_RENDERING:
		break;

	case ID_PATHTRACING:
		trap_Cvar_SetValue( "r_pathtracing", renderingOptionsInfo.pathtracing.curvalue );
		break;

	case ID_PT_SAMPLES:
		trap_Cvar_SetValue( "r_pt_samples", atoi( pt_samples_items[renderingOptionsInfo.pt_samples.curvalue] ) );
		break;

	case ID_PT_BOUNCES:
		trap_Cvar_SetValue( "r_pt_bounces", atoi( pt_bounces_items[renderingOptionsInfo.pt_bounces.curvalue] ) );
		break;

	case ID_PT_MAXDEPTH:
		trap_Cvar_SetValue( "r_pt_maxDepth", atoi( pt_maxdepth_items[renderingOptionsInfo.pt_maxdepth.curvalue] ) );
		break;

	case ID_PT_DENOISE:
		trap_Cvar_SetValue( "r_pt_denoise", renderingOptionsInfo.pt_denoise.curvalue );
		break;

	case ID_PT_DENOISEITER:
		trap_Cvar_SetValue( "r_pt_denoiseIterations", atoi( pt_denoiseiter_items[renderingOptionsInfo.pt_denoiseiter.curvalue] ) );
		break;

	case ID_PT_TEMPORAL:
		trap_Cvar_SetValue( "r_pt_temporal", renderingOptionsInfo.pt_temporal.curvalue );
		break;

	case ID_PT_TEMPORALALPHA:
		trap_Cvar_SetValue( "r_pt_temporalAlpha", renderingOptionsInfo.pt_temporalalpha.curvalue / 100.0f );
		break;

	case ID_PT_GI:
		trap_Cvar_SetValue( "r_pt_gi", renderingOptionsInfo.pt_gi.curvalue );
		break;

	case ID_PT_GIINTENSITY:
		trap_Cvar_SetValue( "r_pt_giIntensity", renderingOptionsInfo.pt_giintensity.curvalue / 100.0f );
		break;

	case ID_PT_OUTPUTSCALE:
		{
			float scale_values[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
			trap_Cvar_SetValue( "r_pt_outputScale", scale_values[renderingOptionsInfo.pt_outputscale.curvalue] );
		}
		break;

	case ID_BACK:
		UI_PopMenu();
		break;
	}
}


/*
===============
UI_RenderingOptionsMenu_Init
===============
*/
static void UI_RenderingOptionsMenu_Init( void ) {
	int		y;
	int		pathtracing;
	int		pt_samples;
	int		pt_bounces;
	int		pt_maxdepth;
	int		pt_denoise;
	int		pt_denoiseiter;
	int		pt_temporal;
	float	pt_temporalalpha;
	int		pt_gi;
	float	pt_giintensity;
	float	pt_outputscale;
	int		i;

	memset( &renderingOptionsInfo, 0, sizeof(renderingOptionsInfo) );

	UI_RenderingOptionsMenu_Cache();
	renderingOptionsInfo.menu.wrapAround = qtrue;
	renderingOptionsInfo.menu.fullscreen = qtrue;

	renderingOptionsInfo.banner.generic.type		= MTYPE_BTEXT;
	renderingOptionsInfo.banner.generic.flags		= QMF_CENTER_JUSTIFY;
	renderingOptionsInfo.banner.generic.x			= 320;
	renderingOptionsInfo.banner.generic.y			= 16;
	renderingOptionsInfo.banner.string				= "SYSTEM SETUP";
	renderingOptionsInfo.banner.color				= color_white;
	renderingOptionsInfo.banner.style				= UI_CENTER;

	renderingOptionsInfo.framel.generic.type		= MTYPE_BITMAP;
	renderingOptionsInfo.framel.generic.name		= ART_FRAMEL;
	renderingOptionsInfo.framel.generic.flags		= QMF_INACTIVE;
	renderingOptionsInfo.framel.generic.x			= 0;  
	renderingOptionsInfo.framel.generic.y			= 78;
	renderingOptionsInfo.framel.width				= 256;
	renderingOptionsInfo.framel.height				= 329;

	renderingOptionsInfo.framer.generic.type		= MTYPE_BITMAP;
	renderingOptionsInfo.framer.generic.name		= ART_FRAMER;
	renderingOptionsInfo.framer.generic.flags		= QMF_INACTIVE;
	renderingOptionsInfo.framer.generic.x			= 376;
	renderingOptionsInfo.framer.generic.y			= 76;
	renderingOptionsInfo.framer.width				= 256;
	renderingOptionsInfo.framer.height				= 334;

	renderingOptionsInfo.graphics.generic.type		= MTYPE_PTEXT;
	renderingOptionsInfo.graphics.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	renderingOptionsInfo.graphics.generic.id		= ID_GRAPHICS;
	renderingOptionsInfo.graphics.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.graphics.generic.x			= 216;
	renderingOptionsInfo.graphics.generic.y			= 240 - 3 * PROP_HEIGHT;
	renderingOptionsInfo.graphics.string			= "GRAPHICS";
	renderingOptionsInfo.graphics.style				= UI_RIGHT;
	renderingOptionsInfo.graphics.color				= color_red;

	renderingOptionsInfo.display.generic.type		= MTYPE_PTEXT;
	renderingOptionsInfo.display.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	renderingOptionsInfo.display.generic.id			= ID_DISPLAY;
	renderingOptionsInfo.display.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.display.generic.x			= 216;
	renderingOptionsInfo.display.generic.y			= 240 - 2 * PROP_HEIGHT;
	renderingOptionsInfo.display.string				= "DISPLAY";
	renderingOptionsInfo.display.style				= UI_RIGHT;
	renderingOptionsInfo.display.color				= color_red;

	renderingOptionsInfo.sound.generic.type			= MTYPE_PTEXT;
	renderingOptionsInfo.sound.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	renderingOptionsInfo.sound.generic.id			= ID_SOUND;
	renderingOptionsInfo.sound.generic.callback		= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.sound.generic.x			= 216;
	renderingOptionsInfo.sound.generic.y			= 240 - PROP_HEIGHT;
	renderingOptionsInfo.sound.string				= "SOUND";
	renderingOptionsInfo.sound.style				= UI_RIGHT;
	renderingOptionsInfo.sound.color				= color_red;

	renderingOptionsInfo.network.generic.type		= MTYPE_PTEXT;
	renderingOptionsInfo.network.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	renderingOptionsInfo.network.generic.id			= ID_NETWORK;
	renderingOptionsInfo.network.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.network.generic.x			= 216;
	renderingOptionsInfo.network.generic.y			= 240;
	renderingOptionsInfo.network.string				= "NETWORK";
	renderingOptionsInfo.network.style				= UI_RIGHT;
	renderingOptionsInfo.network.color				= color_red;

	renderingOptionsInfo.rendering.generic.type		= MTYPE_PTEXT;
	renderingOptionsInfo.rendering.generic.flags	= QMF_RIGHT_JUSTIFY;
	renderingOptionsInfo.rendering.generic.id		= ID_RENDERING;
	renderingOptionsInfo.rendering.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.rendering.generic.x		= 216;
	renderingOptionsInfo.rendering.generic.y		= 240 + PROP_HEIGHT;
	renderingOptionsInfo.rendering.string			= "RENDERING";
	renderingOptionsInfo.rendering.style			= UI_RIGHT;
	renderingOptionsInfo.rendering.color			= color_red;

	y = 240 - 7 * (BIGCHAR_HEIGHT + 2);

	// Path Tracing
	renderingOptionsInfo.pathtracing.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pathtracing.generic.name		= "Path Tracing:";
	renderingOptionsInfo.pathtracing.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pathtracing.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pathtracing.generic.id		= ID_PATHTRACING;
	renderingOptionsInfo.pathtracing.generic.x		= 400;
	renderingOptionsInfo.pathtracing.generic.y		= y;
	renderingOptionsInfo.pathtracing.itemnames		= yesno_names;
	pathtracing = trap_Cvar_VariableValue( "r_pathtracing" );
	renderingOptionsInfo.pathtracing.curvalue		= pathtracing ? 1 : 0;
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_samples.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_samples.generic.name		= "Samples per Pixel:";
	renderingOptionsInfo.pt_samples.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_samples.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_samples.generic.id		= ID_PT_SAMPLES;
	renderingOptionsInfo.pt_samples.generic.x		= 400;
	renderingOptionsInfo.pt_samples.generic.y		= y;
	renderingOptionsInfo.pt_samples.itemnames		= pt_samples_items;
	pt_samples = trap_Cvar_VariableValue( "r_pt_samples" );
	for ( i = 0; pt_samples_items[i]; i++ ) {
		if ( atoi( pt_samples_items[i] ) == pt_samples ) {
			renderingOptionsInfo.pt_samples.curvalue = i;
			break;
		}
	}
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_bounces.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_bounces.generic.name		= "Max Bounces:";
	renderingOptionsInfo.pt_bounces.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_bounces.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_bounces.generic.id		= ID_PT_BOUNCES;
	renderingOptionsInfo.pt_bounces.generic.x		= 400;
	renderingOptionsInfo.pt_bounces.generic.y		= y;
	renderingOptionsInfo.pt_bounces.itemnames		= pt_bounces_items;
	pt_bounces = trap_Cvar_VariableValue( "r_pt_bounces" );
	for ( i = 0; pt_bounces_items[i]; i++ ) {
		if ( atoi( pt_bounces_items[i] ) == pt_bounces ) {
			renderingOptionsInfo.pt_bounces.curvalue = i;
			break;
		}
	}
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_maxdepth.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_maxdepth.generic.name		= "Max Ray Depth:";
	renderingOptionsInfo.pt_maxdepth.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_maxdepth.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_maxdepth.generic.id		= ID_PT_MAXDEPTH;
	renderingOptionsInfo.pt_maxdepth.generic.x		= 400;
	renderingOptionsInfo.pt_maxdepth.generic.y		= y;
	renderingOptionsInfo.pt_maxdepth.itemnames		= pt_maxdepth_items;
	pt_maxdepth = trap_Cvar_VariableValue( "r_pt_maxDepth" );
	for ( i = 0; pt_maxdepth_items[i]; i++ ) {
		if ( atoi( pt_maxdepth_items[i] ) == pt_maxdepth ) {
			renderingOptionsInfo.pt_maxdepth.curvalue = i;
			break;
		}
	}
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_denoise.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_denoise.generic.name		= "Denoising:";
	renderingOptionsInfo.pt_denoise.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_denoise.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_denoise.generic.id		= ID_PT_DENOISE;
	renderingOptionsInfo.pt_denoise.generic.x		= 400;
	renderingOptionsInfo.pt_denoise.generic.y		= y;
	renderingOptionsInfo.pt_denoise.itemnames		= yesno_names;
	pt_denoise = trap_Cvar_VariableValue( "r_pt_denoise" );
	renderingOptionsInfo.pt_denoise.curvalue			= pt_denoise ? 1 : 0;
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_denoiseiter.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_denoiseiter.generic.name		= "Denoise Iterations:";
	renderingOptionsInfo.pt_denoiseiter.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_denoiseiter.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_denoiseiter.generic.id		= ID_PT_DENOISEITER;
	renderingOptionsInfo.pt_denoiseiter.generic.x		= 400;
	renderingOptionsInfo.pt_denoiseiter.generic.y		= y;
	renderingOptionsInfo.pt_denoiseiter.itemnames		= pt_denoiseiter_items;
	pt_denoiseiter = trap_Cvar_VariableValue( "r_pt_denoiseIterations" );
	for ( i = 0; pt_denoiseiter_items[i]; i++ ) {
		if ( atoi( pt_denoiseiter_items[i] ) == pt_denoiseiter ) {
			renderingOptionsInfo.pt_denoiseiter.curvalue = i;
			break;
		}
	}
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_temporal.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_temporal.generic.name		= "Temporal Accumulation:";
	renderingOptionsInfo.pt_temporal.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_temporal.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_temporal.generic.id		= ID_PT_TEMPORAL;
	renderingOptionsInfo.pt_temporal.generic.x		= 400;
	renderingOptionsInfo.pt_temporal.generic.y		= y;
	renderingOptionsInfo.pt_temporal.itemnames		= yesno_names;
	pt_temporal = trap_Cvar_VariableValue( "r_pt_temporal" );
	renderingOptionsInfo.pt_temporal.curvalue			= pt_temporal ? 1 : 0;
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_temporalalpha.generic.type		= MTYPE_SLIDER;
	renderingOptionsInfo.pt_temporalalpha.generic.name		= "Temporal Alpha:";
	renderingOptionsInfo.pt_temporalalpha.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_temporalalpha.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_temporalalpha.generic.id		= ID_PT_TEMPORALALPHA;
	renderingOptionsInfo.pt_temporalalpha.generic.x			= 400;
	renderingOptionsInfo.pt_temporalalpha.generic.y			= y;
	renderingOptionsInfo.pt_temporalalpha.minvalue			= 0;
	renderingOptionsInfo.pt_temporalalpha.maxvalue			= 100;
	pt_temporalalpha = trap_Cvar_VariableValue( "r_pt_temporalAlpha" );
	renderingOptionsInfo.pt_temporalalpha.curvalue			= pt_temporalalpha * 100;
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_gi.generic.type			= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_gi.generic.name			= "Global Illumination:";
	renderingOptionsInfo.pt_gi.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_gi.generic.callback		= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_gi.generic.id			= ID_PT_GI;
	renderingOptionsInfo.pt_gi.generic.x			= 400;
	renderingOptionsInfo.pt_gi.generic.y			= y;
	renderingOptionsInfo.pt_gi.itemnames			= yesno_names;
	pt_gi = trap_Cvar_VariableValue( "r_pt_gi" );
	renderingOptionsInfo.pt_gi.curvalue				= pt_gi ? 1 : 0;
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_giintensity.generic.type		= MTYPE_SLIDER;
	renderingOptionsInfo.pt_giintensity.generic.name		= "GI Intensity:";
	renderingOptionsInfo.pt_giintensity.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_giintensity.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_giintensity.generic.id		= ID_PT_GIINTENSITY;
	renderingOptionsInfo.pt_giintensity.generic.x			= 400;
	renderingOptionsInfo.pt_giintensity.generic.y			= y;
	renderingOptionsInfo.pt_giintensity.minvalue			= 0;
	renderingOptionsInfo.pt_giintensity.maxvalue			= 200;
	pt_giintensity = trap_Cvar_VariableValue( "r_pt_giIntensity" );
	renderingOptionsInfo.pt_giintensity.curvalue			= pt_giintensity * 100;
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.pt_outputscale.generic.type		= MTYPE_SPINCONTROL;
	renderingOptionsInfo.pt_outputscale.generic.name		= "Output Scale:";
	renderingOptionsInfo.pt_outputscale.generic.flags		= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	renderingOptionsInfo.pt_outputscale.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.pt_outputscale.generic.id		= ID_PT_OUTPUTSCALE;
	renderingOptionsInfo.pt_outputscale.generic.x		= 400;
	renderingOptionsInfo.pt_outputscale.generic.y		= y;
	renderingOptionsInfo.pt_outputscale.itemnames		= outputscale_items;
	pt_outputscale = trap_Cvar_VariableValue( "r_pt_outputScale" );
	{
		float scale_values[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
		for ( i = 0; i < 7; i++ ) {
			if ( fabs( pt_outputscale - scale_values[i] ) < 0.01f ) {
				renderingOptionsInfo.pt_outputscale.curvalue = i;
				break;
			}
		}
	}
	y += BIGCHAR_HEIGHT + 2;

	renderingOptionsInfo.back.generic.type		= MTYPE_BITMAP;
	renderingOptionsInfo.back.generic.name		= ART_BACK0;
	renderingOptionsInfo.back.generic.flags		= QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	renderingOptionsInfo.back.generic.callback	= UI_RenderingOptionsMenu_Event;
	renderingOptionsInfo.back.generic.id		= ID_BACK;
	renderingOptionsInfo.back.generic.x		= 0;
	renderingOptionsInfo.back.generic.y		= 480-64;
	renderingOptionsInfo.back.width			= 128;
	renderingOptionsInfo.back.height		= 64;
	renderingOptionsInfo.back.focuspic		= ART_BACK1;

	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.banner );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.framel );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.framer );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.graphics );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.display );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.sound );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.network );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.rendering );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pathtracing );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_samples );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_bounces );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_maxdepth );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_denoise );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_denoiseiter );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_temporal );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_temporalalpha );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_gi );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_giintensity );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.pt_outputscale );
	Menu_AddItem( &renderingOptionsInfo.menu, ( void * ) &renderingOptionsInfo.back );
}


/*
===============
UI_RenderingOptionsMenu_Cache
===============
*/
static void UI_RenderingOptionsMenu_Cache( void ) {
	trap_R_RegisterShaderNoMip( ART_FRAMEL );
	trap_R_RegisterShaderNoMip( ART_FRAMER );
	trap_R_RegisterShaderNoMip( ART_BACK0 );
	trap_R_RegisterShaderNoMip( ART_BACK1 );
}


/*
===============
UI_RenderingOptionsMenu
===============
*/
void UI_RenderingOptionsMenu( void ) {
	UI_RenderingOptionsMenu_Init();
	UI_PushMenu( &renderingOptionsInfo.menu );
	Menu_SetCursorToItem( &renderingOptionsInfo.menu, &renderingOptionsInfo.pathtracing );
}

