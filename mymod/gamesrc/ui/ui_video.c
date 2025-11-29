/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#include "ui_local.h"

void GraphicsOptions_MenuInit( void );

/*
=======================================================================

DRIVER INFORMATION MENU

=======================================================================
*/


#define DRIVERINFO_FRAMEL	"menu/" MENU_ART_DIR "/frame2_l"
#define DRIVERINFO_FRAMER	"menu/" MENU_ART_DIR "/frame1_r"
#define DRIVERINFO_BACK0	"menu/" MENU_ART_DIR "/back_0"
#define DRIVERINFO_BACK1	"menu/" MENU_ART_DIR "/back_1"

static char* driverinfo_artlist[] = 
{
	DRIVERINFO_FRAMEL,
	DRIVERINFO_FRAMER,
	DRIVERINFO_BACK0,
	DRIVERINFO_BACK1,
	NULL,
};

#define ID_DRIVERINFOBACK	100

typedef struct
{
	menuframework_s	menu;
	menutext_s		banner;
	menubitmap_s	back;
	menubitmap_s	framel;
	menubitmap_s	framer;
	char			stringbuff[1024];
	char*			strings[64];
	int				numstrings;
} driverinfo_t;

static driverinfo_t	s_driverinfo;

/*
=================
DriverInfo_Event
=================
*/
static void DriverInfo_Event( void* ptr, int event )
{
	if (event != QM_ACTIVATED)
		return;

	switch (((menucommon_s*)ptr)->id)
	{
		case ID_DRIVERINFOBACK:
			UI_PopMenu();
			break;
	}
}

/*
=================
DriverInfo_MenuDraw
=================
*/
static void DriverInfo_MenuDraw( void )
{
	int	i;
	int	y;

	Menu_Draw( &s_driverinfo.menu );

	UI_DrawString( 320, 80, "VENDOR", UI_CENTER|UI_SMALLFONT, color_red );
	UI_DrawString( 320, 152, "PIXELFORMAT", UI_CENTER|UI_SMALLFONT, color_red );
	UI_DrawString( 320, 192, "EXTENSIONS", UI_CENTER|UI_SMALLFONT, color_red );

	UI_DrawString( 320, 80+16, uis.glconfig.vendor_string, UI_CENTER|UI_SMALLFONT, text_color_normal );
	UI_DrawString( 320, 96+16, uis.glconfig.version_string, UI_CENTER|UI_SMALLFONT, text_color_normal );
	UI_DrawString( 320, 112+16, uis.glconfig.renderer_string, UI_CENTER|UI_SMALLFONT, text_color_normal );
	UI_DrawString( 320, 152+16, va ("color(%d-bits) Z(%d-bits) stencil(%d-bits)", uis.glconfig.colorBits, uis.glconfig.depthBits, uis.glconfig.stencilBits), UI_CENTER|UI_SMALLFONT, text_color_normal );

	// double column
	y = 192+16;
	for (i=0; i<s_driverinfo.numstrings/2; i++) {
		UI_DrawString( 320-4, y, s_driverinfo.strings[i*2], UI_RIGHT|UI_SMALLFONT, text_color_normal );
		UI_DrawString( 320+4, y, s_driverinfo.strings[i*2+1], UI_LEFT|UI_SMALLFONT, text_color_normal );
		y += SMALLCHAR_HEIGHT;
	}

	if (s_driverinfo.numstrings & 1)
		UI_DrawString( 320, y, s_driverinfo.strings[s_driverinfo.numstrings-1], UI_CENTER|UI_SMALLFONT, text_color_normal );
}

/*
=================
DriverInfo_Cache
=================
*/
void DriverInfo_Cache( void )
{
	int	i;

	// touch all our pics
	for (i=0; ;i++)
	{
		if (!driverinfo_artlist[i])
			break;
		trap_R_RegisterShaderNoMip(driverinfo_artlist[i]);
	}
}

/*
=================
UI_DriverInfo_Menu
=================
*/
static void UI_DriverInfo_Menu( void )
{
	char*	eptr;
	int		i;
	int		len;

	// zero set all our globals
	memset( &s_driverinfo, 0 ,sizeof(driverinfo_t) );

	DriverInfo_Cache();

	s_driverinfo.menu.fullscreen = qtrue;
	s_driverinfo.menu.draw       = DriverInfo_MenuDraw;

	s_driverinfo.banner.generic.type  = MTYPE_BTEXT;
	s_driverinfo.banner.generic.x	  = 320;
	s_driverinfo.banner.generic.y	  = 16;
	s_driverinfo.banner.string		  = "DRIVER INFO";
	s_driverinfo.banner.color	      = color_white;
	s_driverinfo.banner.style	      = UI_CENTER;

	s_driverinfo.framel.generic.type  = MTYPE_BITMAP;
	s_driverinfo.framel.generic.name  = DRIVERINFO_FRAMEL;
	s_driverinfo.framel.generic.flags = QMF_INACTIVE;
	s_driverinfo.framel.generic.x	  = 0;
	s_driverinfo.framel.generic.y	  = 78;
	s_driverinfo.framel.width  	      = 256;
	s_driverinfo.framel.height  	  = 329;

	s_driverinfo.framer.generic.type  = MTYPE_BITMAP;
	s_driverinfo.framer.generic.name  = DRIVERINFO_FRAMER;
	s_driverinfo.framer.generic.flags = QMF_INACTIVE;
	s_driverinfo.framer.generic.x	  = 376;
	s_driverinfo.framer.generic.y	  = 76;
	s_driverinfo.framer.width  	      = 256;
	s_driverinfo.framer.height  	  = 334;

	s_driverinfo.back.generic.type	   = MTYPE_BITMAP;
	s_driverinfo.back.generic.name     = DRIVERINFO_BACK0;
	s_driverinfo.back.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_driverinfo.back.generic.callback = DriverInfo_Event;
	s_driverinfo.back.generic.id	   = ID_DRIVERINFOBACK;
	s_driverinfo.back.generic.x		   = 0;
	s_driverinfo.back.generic.y		   = 480-64;
	s_driverinfo.back.width  		   = 128;
	s_driverinfo.back.height  		   = 64;
	s_driverinfo.back.focuspic         = DRIVERINFO_BACK1;

  // TTimo: overflow with particularly long GL extensions (such as the gf3)
  // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=399
  // NOTE: could have pushed the size of stringbuff, but the list is already out of the screen
  // (no matter what your resolution)
  Q_strncpyz(s_driverinfo.stringbuff, uis.glconfig.extensions_string, 1024);

	// build null terminated extension strings
	eptr = s_driverinfo.stringbuff;
	while ( s_driverinfo.numstrings<40 && *eptr )
	{
		while ( *eptr == ' ' )
			*eptr++ = '\0';

		// track start of valid string
		if (*eptr && *eptr != ' ')
			s_driverinfo.strings[s_driverinfo.numstrings++] = eptr;

		while ( *eptr && *eptr != ' ' )
			eptr++;
	}

	// safety length strings for display
	for (i=0; i<s_driverinfo.numstrings; i++) {
		len = strlen(s_driverinfo.strings[i]);
		if (len > 32) {
			s_driverinfo.strings[i][len-1] = '>';
			s_driverinfo.strings[i][len]   = '\0';
		}
	}

	Menu_AddItem( &s_driverinfo.menu, &s_driverinfo.banner );
	Menu_AddItem( &s_driverinfo.menu, &s_driverinfo.framel );
	Menu_AddItem( &s_driverinfo.menu, &s_driverinfo.framer );
	Menu_AddItem( &s_driverinfo.menu, &s_driverinfo.back );

	UI_PushMenu( &s_driverinfo.menu );
}

/*
=======================================================================

GRAPHICS OPTIONS MENU

=======================================================================
*/

#define GRAPHICSOPTIONS_FRAMEL	"menu/" MENU_ART_DIR "/frame2_l"
#define GRAPHICSOPTIONS_FRAMER	"menu/" MENU_ART_DIR "/frame1_r"
#define GRAPHICSOPTIONS_BACK0	"menu/" MENU_ART_DIR "/back_0"
#define GRAPHICSOPTIONS_BACK1	"menu/" MENU_ART_DIR "/back_1"
#define GRAPHICSOPTIONS_ACCEPT0	"menu/" MENU_ART_DIR "/accept_0"
#define GRAPHICSOPTIONS_ACCEPT1	"menu/" MENU_ART_DIR "/accept_1"

#define ID_BACK2		101
#define ID_FULLSCREEN	102
#define ID_LIST			103
#define ID_MODE			104
#define ID_DRIVERINFO	105
#define ID_GRAPHICS		106
#define ID_DISPLAY		107
#define ID_SOUND		108
#define ID_NETWORK		109
#define ID_RATIO                110
// Advanced rendering features
#define ID_RAYTRACING		111
#define ID_RT_SAMPLES		112
#define ID_RT_MAXDEPTH		113
#define ID_RT_TEMPORAL		114
#define ID_RT_TEMPORALALPHA	115
#define ID_RT_DENOISE		116
#define ID_RT_DENOISEMODE	117
#define ID_RT_DENOISEITER	118
#define ID_RT_GI		119
#define ID_RT_GIBOUNCES		120
#define ID_RT_GIINTENSITY	121
#define ID_DLSS		122
#define ID_DLSS_QUALITY		123
#define ID_DLSS_SHARPEN		124
#define ID_POSTPROCESS_COMPUTE	125
#define ID_MESHSHADERS		126
#define ID_MESHLETSIZE		127
#define ID_VIRTUALTEXTURES	128
#define ID_VT_PAGESIZE		129
#define ID_VT_CACHESIZE		130
#define ID_CLEARCOAT		131
#define ID_ANISOTROPY		132
#define ID_SUBSURFACESCATTERING	133
#define ID_PARTICLES_GPU	134
#define ID_PARTICLES_MAX	135
#define ID_PARTICLES_CULLING	136

typedef struct {
	menuframework_s	menu;

	menutext_s		banner;
	menubitmap_s	framel;
	menubitmap_s	framer;

	menutext_s		graphics;
	menutext_s		display;
	menutext_s		sound;
	menutext_s		network;

	menulist_s		list;
        menulist_s              ratio;
	menulist_s		mode;
	menulist_s		driver;
	menuslider_s	tq;
	menulist_s  	fs;
	menulist_s  	lighting;
        menulist_s  	flares;
        menulist_s  	bloom;
	menulist_s  	allow_extensions;
	menulist_s  	texturebits;
	menulist_s  	geometry;
	menulist_s  	filter;
        menulist_s  	aniso;
	menulist_s  	drawfps;
	menutext_s		driverinfo;
	
	// Advanced rendering features
	menulist_s		raytracing;
	menuslider_s	rt_samples;
	menuslider_s	rt_maxdepth;
	menulist_s		rt_temporal;
	menuslider_s	rt_temporalalpha;
	menulist_s		rt_denoise;
	menulist_s		rt_denoisemode;
	menuslider_s	rt_denoiseiter;
	menulist_s		rt_gi;
	menuslider_s	rt_gibounces;
	menuslider_s	rt_giintensity;
	menulist_s		dlss;
	menulist_s		dlss_quality;
	menuslider_s	dlss_sharpen;
	menulist_s		postprocess_compute;
	menulist_s		meshshaders;
	menuslider_s	meshletsize;
	menulist_s		virtualtextures;
	menuslider_s	vt_pagesize;
	menuslider_s	vt_cachesize;
	menulist_s		clearcoat;
	menulist_s		anisotropy_mat;
	menulist_s		subsurfacescattering;
	menulist_s		particles_gpu;
	menuslider_s	particles_max;
	menulist_s		particles_culling;

	menubitmap_s	apply;
	menubitmap_s	back;
} graphicsoptions_t;

typedef struct
{
	int mode;
	qboolean fullscreen;
	int tq;
	int lighting;
	qboolean flares;
	qboolean bloom;
	qboolean drawfps;
	int texturebits;
	int geometry;
	int filter;
        int aniso;
	int driver;
	qboolean extensions;
} InitialVideoOptions_s;

static InitialVideoOptions_s	s_ivo;
static graphicsoptions_t		s_graphicsoptions;	

static InitialVideoOptions_s s_ivo_templates[] =
{
	{
		6, qtrue, 3, 0, qfalse,qfalse,qfalse, 2, 2, 1, 0, 0, qtrue
	},
	{
		4, qtrue, 2, 0, qfalse,qfalse,qfalse, 2, 1, 1, 0, 0, qtrue	// JDC: this was tq 3
	},
	{
		3, qtrue, 2, 0, qfalse,qfalse,qfalse, 0, 1, 0, 0, 0, qtrue
	},
	{
		2, qtrue, 1, 0, qfalse,qfalse,qfalse, 0, 0, 0, 0, 0, qtrue
	},
	{
		2, qtrue, 1, 1, qfalse,qfalse,qfalse, 0, 0, 0, 0, 0, qtrue
	},
	{
		3, qtrue, 1, 0, qfalse,qfalse,qfalse, 0, 1, 0, 0, 0, qtrue
	}
};

#define NUM_IVO_TEMPLATES ( sizeof( s_ivo_templates ) / sizeof( s_ivo_templates[0] ) )

static const char *builtinResolutions[ ] =
{
	"320x240",
	"400x300",
	"512x384",
	"640x480",
	"800x600",
	"960x720",
	"1024x768",
	"1152x864",
	"1280x1024",
	"1600x1200",
	"2048x1536",
	"856x480",
	NULL
};

static const char *knownRatios[ ][2] =
{
        { "1.25:1", "5:4"   },
        { "1.33:1", "4:3"   },
        { "1.50:1", "3:2"   },
        { "1.56:1", "14:9"  },
        { "1.60:1", "16:10" },
        { "1.67:1", "5:3"   },
        { "1.78:1", "16:9"  },
        { NULL    , NULL    }
};

#define MAX_RESOLUTIONS 32

static const char* ratios[ MAX_RESOLUTIONS ];
static char ratioBuf[ MAX_RESOLUTIONS ][ 8 ];
static int ratioToRes[ MAX_RESOLUTIONS ];
static int resToRatio[ MAX_RESOLUTIONS ];

static char resbuf[ MAX_STRING_CHARS ];
static const char* detectedResolutions[ MAX_RESOLUTIONS ];
static char currentResolution[ 20 ];

static const char** resolutions = builtinResolutions;
static qboolean resolutionsDetected = qfalse;

/*
=================
GraphicsOptions_FindBuiltinResolution
=================
*/
static int GraphicsOptions_FindBuiltinResolution( int mode )
{
	int i;

	if( !resolutionsDetected )
		return mode;

	if( mode < 0 )
		return -1;

	for( i = 0; builtinResolutions[ i ]; i++ )
	{
		if( Q_strequal( builtinResolutions[ i ], detectedResolutions[ mode ] ) )
			return i;
	}

	return -1;
}

/*
=================
GraphicsOptions_FindDetectedResolution
=================
*/
static int GraphicsOptions_FindDetectedResolution( int mode )
{
	int i;

	if( !resolutionsDetected )
		return mode;

	if( mode < 0 )
		return -1;

	for( i = 0; detectedResolutions[ i ]; i++ )
	{
		if( Q_strequal( builtinResolutions[ mode ], detectedResolutions[ i ] ) )
			return i;
	}

	return -1;
}

/*
=================
GraphicsOptions_GetAspectRatios
=================
*/
static void GraphicsOptions_GetAspectRatios( void )
{
    int i, r;

    // build ratio list from resolutions
    for( r = 0; resolutions[r]; r++ )
    {
        int w, h;
        char *x;
        char str[ sizeof(ratioBuf[0]) ];

        // calculate resolution's aspect ratio
        x = strchr( resolutions[r], 'x' )+1;
        
        
        Q_strncpyz( str, resolutions[r], x-resolutions[r] );
        w = atoi( str );
        h = atoi( x );
        Com_sprintf( str, sizeof(str), "%.2f:1", (float)w / (float)h );
        
        // rename common ratios ("1.33:1" -> "4:3")
        for( i = 0; knownRatios[i][0]; i++ ) {
            if( Q_strequal( str, knownRatios[i][0] ) ) {
                Q_strncpyz( str, knownRatios[i][1], sizeof( str ) );
                break;
            }
        }

        // add ratio to list if it is new
        // establish res/ratio relationship
        for( i = 0; ratioBuf[i][0]; i++ )
        {
            if( Q_strequal( str, ratioBuf[i] ) )
                break;
        }
        if( !ratioBuf[i][0] )
        {
            Q_strncpyz( ratioBuf[i], str, sizeof(ratioBuf[i]) );
            ratioToRes[i] = r;
        }
        ratios[r] = ratioBuf[r];
        resToRatio[r] = i;
    }
    ratios[r] = NULL;
}

/*
=================
GraphicsOptions_GetInitialVideo
=================
*/
static void GraphicsOptions_GetInitialVideo( void )
{
	s_ivo.driver      = s_graphicsoptions.driver.curvalue;
	s_ivo.mode        = s_graphicsoptions.mode.curvalue;
	s_ivo.fullscreen  = s_graphicsoptions.fs.curvalue;
	s_ivo.extensions  = s_graphicsoptions.allow_extensions.curvalue;
	s_ivo.tq          = s_graphicsoptions.tq.curvalue;
	s_ivo.lighting    = s_graphicsoptions.lighting.curvalue;
	s_ivo.flares      = s_graphicsoptions.flares.curvalue;
	s_ivo.bloom      = s_graphicsoptions.bloom.curvalue;
	s_ivo.drawfps     = s_graphicsoptions.drawfps.curvalue;
	s_ivo.geometry    = s_graphicsoptions.geometry.curvalue;
	s_ivo.filter      = s_graphicsoptions.filter.curvalue;
        s_ivo.aniso      = s_graphicsoptions.aniso.curvalue;
	s_ivo.texturebits = s_graphicsoptions.texturebits.curvalue;
}

/*
=================
GraphicsOptions_GetResolutions
=================
*/
static void GraphicsOptions_GetResolutions( void )
{
    trap_Cvar_VariableStringBuffer("r_availableModes", resbuf, sizeof(resbuf));
    if(*resbuf)
    {
        char* s = resbuf;
        unsigned int i = 0;
        while( s && i < sizeof(detectedResolutions)/sizeof(detectedResolutions[0])-1)
        {
            detectedResolutions[i++] = s;
            s = strchr(s, ' ');
            if( s )
                *s++ = '\0';
        }
        detectedResolutions[ i ] = NULL;

		// add custom resolution if not in mode list
		if ( i < ARRAY_LEN(detectedResolutions)-1 )
        {
			Com_sprintf( currentResolution, sizeof ( currentResolution ), "%dx%d", uis.glconfig.vidWidth, uis.glconfig.vidHeight );

			for( i = 0; detectedResolutions[ i ]; i++ )
			{
				if ( strcmp( detectedResolutions[ i ], currentResolution ) == 0 )
					break;
			}

			if ( detectedResolutions[ i ] == NULL )
			{
				detectedResolutions[ i++ ] = currentResolution;
				detectedResolutions[ i ] = NULL;
			}
		
		resolutions = detectedResolutions;
		resolutionsDetected = qtrue;
        }
    }
}

/*
=================
GraphicsOptions_CheckConfig
=================
*/
static void GraphicsOptions_CheckConfig( void )
{
	int i;

	for ( i = 0; i < (int)(NUM_IVO_TEMPLATES-1); i++ )
	{
		if ( s_ivo_templates[i].driver != s_graphicsoptions.driver.curvalue )
			continue;
		if ( GraphicsOptions_FindDetectedResolution(s_ivo_templates[i].mode) != s_graphicsoptions.mode.curvalue )
			continue;
		if ( (int)s_ivo_templates[i].fullscreen != s_graphicsoptions.fs.curvalue )
			continue;
		if ( s_ivo_templates[i].tq != s_graphicsoptions.tq.curvalue )
			continue;
		if ( s_ivo_templates[i].lighting != s_graphicsoptions.lighting.curvalue )
			continue;
                if ( (int)s_ivo_templates[i].flares != s_graphicsoptions.flares.curvalue )
			continue;
                if ( (int)s_ivo_templates[i].bloom != s_graphicsoptions.bloom.curvalue )
			continue;
		if ( (int)s_ivo_templates[i].drawfps != s_graphicsoptions.drawfps.curvalue )
			continue;
		if ( s_ivo_templates[i].geometry != s_graphicsoptions.geometry.curvalue )
			continue;
		if ( s_ivo_templates[i].filter != s_graphicsoptions.filter.curvalue )
			continue;
                if ( s_ivo_templates[i].aniso != s_graphicsoptions.aniso.curvalue )
			continue;
//		if ( s_ivo_templates[i].texturebits != s_graphicsoptions.texturebits.curvalue )
//			continue;
		s_graphicsoptions.list.curvalue = i;
		return;
	}

	// return 'Custom' ivo template
	s_graphicsoptions.list.curvalue = NUM_IVO_TEMPLATES - 1;
}

/*
=================
GraphicsOptions_UpdateMenuItems
=================
*/
static void GraphicsOptions_UpdateMenuItems( void )
{
	if ( s_graphicsoptions.driver.curvalue == 1 )
	{
		s_graphicsoptions.fs.curvalue = 1;
		s_graphicsoptions.fs.generic.flags |= QMF_GRAYED;
	}
	else
	{
		s_graphicsoptions.fs.generic.flags &= ~QMF_GRAYED;
	}

	if ( s_graphicsoptions.allow_extensions.curvalue == 0 )
	{
		if ( s_graphicsoptions.texturebits.curvalue == 0 )
		{
			s_graphicsoptions.texturebits.curvalue = 1;
		}
	}

	s_graphicsoptions.apply.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;

	if ( s_ivo.mode != s_graphicsoptions.mode.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( (int)s_ivo.fullscreen != s_graphicsoptions.fs.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( (int)s_ivo.extensions != s_graphicsoptions.allow_extensions.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( s_ivo.tq != s_graphicsoptions.tq.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( s_ivo.lighting != s_graphicsoptions.lighting.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
        if ( (int)s_ivo.flares != s_graphicsoptions.flares.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
        if ( (int)s_ivo.bloom != s_graphicsoptions.bloom.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( (int)s_ivo.drawfps != s_graphicsoptions.drawfps.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( s_ivo.driver != s_graphicsoptions.driver.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( s_ivo.texturebits != s_graphicsoptions.texturebits.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( s_ivo.geometry != s_graphicsoptions.geometry.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
	if ( s_ivo.filter != s_graphicsoptions.filter.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}
        if ( s_ivo.aniso != s_graphicsoptions.aniso.curvalue )
	{
		s_graphicsoptions.apply.generic.flags &= ~(QMF_HIDDEN|QMF_INACTIVE);
	}

	GraphicsOptions_CheckConfig();
}	

/*
=================
GraphicsOptions_ApplyChanges
=================
*/
static void GraphicsOptions_ApplyChanges( [[maybe_unused]] void *unused, int notification )
{
	if (notification != QM_ACTIVATED)
		return;

	switch ( s_graphicsoptions.texturebits.curvalue  )
	{
	case 0:
		trap_Cvar_SetValue( "r_texturebits", 0 );
		break;
	case 1:
		trap_Cvar_SetValue( "r_texturebits", 16 );
		break;
	case 2:
		trap_Cvar_SetValue( "r_texturebits", 32 );
		break;
	}
	trap_Cvar_SetValue( "r_picmip", 3 - s_graphicsoptions.tq.curvalue );
	trap_Cvar_SetValue( "r_allowExtensions", s_graphicsoptions.allow_extensions.curvalue );

	if( resolutionsDetected )
	{
		// search for builtin mode that matches the detected mode
		int mode;
		if ( s_graphicsoptions.mode.curvalue == -1
			|| s_graphicsoptions.mode.curvalue >= (int)(sizeof(detectedResolutions)/sizeof(detectedResolutions[0])) ) {
			s_graphicsoptions.mode.curvalue = 0;
		}
		mode = GraphicsOptions_FindBuiltinResolution( s_graphicsoptions.mode.curvalue );
		if( mode == -1 )
		{
			char w[ 16 ], h[ 16 ];
			Q_strncpyz( w, detectedResolutions[ s_graphicsoptions.mode.curvalue ], sizeof( w ) );
			*strchr( w, 'x' ) = 0;
			Q_strncpyz( h,
					strchr( detectedResolutions[ s_graphicsoptions.mode.curvalue ], 'x' ) + 1, sizeof( h ) );
			trap_Cvar_Set( "r_customwidth", w );
			trap_Cvar_Set( "r_customheight", h );
		}

		trap_Cvar_SetValue( "r_mode", mode );
	}
	else
		trap_Cvar_SetValue( "r_mode", s_graphicsoptions.mode.curvalue );

	trap_Cvar_SetValue( "r_fullscreen", s_graphicsoptions.fs.curvalue );
	trap_Cvar_SetValue( "r_colorbits", 0 );
	trap_Cvar_SetValue( "r_depthbits", 0 );
	trap_Cvar_SetValue( "r_stencilbits", 0 );
	trap_Cvar_SetValue( "r_vertexLight", s_graphicsoptions.lighting.curvalue );
	trap_Cvar_SetValue( "cg_autovertex", s_graphicsoptions.lighting.curvalue );
	trap_Cvar_SetValue( "r_flares", s_graphicsoptions.flares.curvalue );
	trap_Cvar_SetValue( "r_bloom", s_graphicsoptions.bloom.curvalue );
	trap_Cvar_SetValue( "cg_drawFPS", s_graphicsoptions.drawfps.curvalue );

	//r_ext_texture_filter_anisotropic is special
	if(s_graphicsoptions.aniso.curvalue) {
		trap_Cvar_SetValue( "r_ext_max_anisotropy", s_graphicsoptions.aniso.curvalue*2 );
		trap_Cvar_SetValue( "r_ext_texture_filter_anisotropic", qtrue );
	}
	else {
		trap_Cvar_SetValue( "r_ext_texture_filter_anisotropic", qfalse );
	}

	if ( s_graphicsoptions.geometry.curvalue == 2 )
	{
		trap_Cvar_SetValue( "r_lodBias", 0 );
		trap_Cvar_SetValue( "r_subdivisions", 4 );
	}
	else if ( s_graphicsoptions.geometry.curvalue == 1 )
	{
		trap_Cvar_SetValue( "r_lodBias", 1 );
		trap_Cvar_SetValue( "r_subdivisions", 12 );
	}
	else
	{
		trap_Cvar_SetValue( "r_lodBias", 1 );
		trap_Cvar_SetValue( "r_subdivisions", 20 );
	}

	if ( s_graphicsoptions.filter.curvalue )
	{
		trap_Cvar_Set( "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR" );
	}
	else
	{
		trap_Cvar_Set( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST" );
	}

	// Advanced rendering features
	trap_Cvar_SetValue( "r_raytracing", s_graphicsoptions.raytracing.curvalue );
	trap_Cvar_SetValue( "r_rt_samples", s_graphicsoptions.rt_samples.curvalue );
	trap_Cvar_SetValue( "r_rt_maxDepth", s_graphicsoptions.rt_maxdepth.curvalue );
	trap_Cvar_SetValue( "r_rt_temporal", s_graphicsoptions.rt_temporal.curvalue );
	trap_Cvar_SetValue( "r_rt_temporalAlpha", s_graphicsoptions.rt_temporalalpha.curvalue / 10.0f );
	trap_Cvar_SetValue( "r_rt_denoise", s_graphicsoptions.rt_denoise.curvalue );
	trap_Cvar_SetValue( "r_rt_denoiseMode", s_graphicsoptions.rt_denoisemode.curvalue );
	trap_Cvar_SetValue( "r_rt_denoiseIterations", s_graphicsoptions.rt_denoiseiter.curvalue );
	trap_Cvar_SetValue( "r_rt_gi", s_graphicsoptions.rt_gi.curvalue );
	trap_Cvar_SetValue( "r_rt_giBounces", s_graphicsoptions.rt_gibounces.curvalue );
	trap_Cvar_SetValue( "r_rt_giIntensity", s_graphicsoptions.rt_giintensity.curvalue / 10.0f );
	trap_Cvar_SetValue( "r_dlss", s_graphicsoptions.dlss.curvalue );
	trap_Cvar_SetValue( "r_dlss_quality", s_graphicsoptions.dlss_quality.curvalue );
	trap_Cvar_SetValue( "r_dlss_sharpen", s_graphicsoptions.dlss_sharpen.curvalue / 10.0f );
	trap_Cvar_SetValue( "r_postprocess_compute", s_graphicsoptions.postprocess_compute.curvalue );
	trap_Cvar_SetValue( "r_meshShaders", s_graphicsoptions.meshshaders.curvalue );
	trap_Cvar_SetValue( "r_meshletSize", s_graphicsoptions.meshletsize.curvalue * 32 );
	trap_Cvar_SetValue( "r_virtualTextures", s_graphicsoptions.virtualtextures.curvalue );
	trap_Cvar_SetValue( "r_vt_pageSize", s_graphicsoptions.vt_pagesize.curvalue * 64 );
	trap_Cvar_SetValue( "r_vt_cacheSize", s_graphicsoptions.vt_cachesize.curvalue * 128 );
	trap_Cvar_SetValue( "r_clearcoat", s_graphicsoptions.clearcoat.curvalue );
	trap_Cvar_SetValue( "r_anisotropy", s_graphicsoptions.anisotropy_mat.curvalue );
	trap_Cvar_SetValue( "r_subsurfaceScattering", s_graphicsoptions.subsurfacescattering.curvalue );
	trap_Cvar_SetValue( "r_particles_gpu", s_graphicsoptions.particles_gpu.curvalue );
	trap_Cvar_SetValue( "r_particles_max", s_graphicsoptions.particles_max.curvalue * 10000 );
	trap_Cvar_SetValue( "r_particles_culling", s_graphicsoptions.particles_culling.curvalue );

	trap_Cmd_ExecuteText( EXEC_APPEND, "vid_restart\n" );
}

/*
=================
GraphicsOptions_Event
=================
*/
static void GraphicsOptions_Event( void* ptr, int event ) {
	InitialVideoOptions_s *ivo;

	if( event != QM_ACTIVATED ) {
	 	return;
	}

	switch( ((menucommon_s*)ptr)->id ) {
        case ID_RATIO:
            s_graphicsoptions.mode.curvalue = ratioToRes[ s_graphicsoptions.ratio.curvalue ];
            [[fallthrough]]; // fall through to apply mode constraints
	case ID_MODE:
		// clamp 3dfx video modes
		if ( s_graphicsoptions.driver.curvalue == 1 )
		{
			if ( s_graphicsoptions.mode.curvalue < 2 )
				s_graphicsoptions.mode.curvalue = 2;
			else if ( s_graphicsoptions.mode.curvalue > 6 )
				s_graphicsoptions.mode.curvalue = 6;
		}
                s_graphicsoptions.ratio.curvalue = resToRatio[ s_graphicsoptions.mode.curvalue ];
		break;

	case ID_LIST:
		ivo = &s_ivo_templates[s_graphicsoptions.list.curvalue];

		s_graphicsoptions.mode.curvalue        = GraphicsOptions_FindDetectedResolution(ivo->mode);
                s_graphicsoptions.ratio.curvalue       = resToRatio[ s_graphicsoptions.mode.curvalue ];
		s_graphicsoptions.tq.curvalue          = ivo->tq;
		s_graphicsoptions.lighting.curvalue    = ivo->lighting;
		s_graphicsoptions.texturebits.curvalue = ivo->texturebits;
		s_graphicsoptions.geometry.curvalue    = ivo->geometry;
		s_graphicsoptions.filter.curvalue      = ivo->filter;
                s_graphicsoptions.aniso.curvalue       = ivo->aniso;
		s_graphicsoptions.fs.curvalue          = ivo->fullscreen;
                s_graphicsoptions.flares.curvalue      = ivo->flares;
                s_graphicsoptions.bloom.curvalue      = ivo->bloom;
                s_graphicsoptions.drawfps.curvalue      = ivo->drawfps;
		break;

	case ID_DRIVERINFO:
		UI_DriverInfo_Menu();
		break;

	case ID_BACK2:
		UI_PopMenu();
		break;

	case ID_GRAPHICS:
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
	}
}


/*
================
GraphicsOptions_TQEvent
================
*/
static void GraphicsOptions_TQEvent( [[maybe_unused]] void *ptr, int event ) {
	if( event != QM_ACTIVATED ) {
	 	return;
	}
	s_graphicsoptions.tq.curvalue = (int)(s_graphicsoptions.tq.curvalue + 0.5);
}


/*
================
GraphicsOptions_MenuDraw
================
*/
void GraphicsOptions_MenuDraw (void)
{
//APSFIX - rework this
	GraphicsOptions_UpdateMenuItems();

	Menu_Draw( &s_graphicsoptions.menu );
}

/*
=================
GraphicsOptions_SetMenuItems
=================
*/
static void GraphicsOptions_SetMenuItems( void )
{
	s_graphicsoptions.mode.curvalue =
		GraphicsOptions_FindDetectedResolution( trap_Cvar_VariableValue( "r_mode" ) );

	if ( s_graphicsoptions.mode.curvalue < 0 )
	{
		if( resolutionsDetected )
		{
			int i;
			char buf[MAX_STRING_CHARS];
			trap_Cvar_VariableStringBuffer("r_customwidth", buf, sizeof(buf)-2);
			buf[strlen(buf)+1] = 0;
			buf[strlen(buf)] = 'x';
			trap_Cvar_VariableStringBuffer("r_customheight", buf+strlen(buf), sizeof(buf)-strlen(buf));

			for(i = 0; detectedResolutions[i]; ++i)
			{
				if(Q_strequal(buf, detectedResolutions[i]))
				{
					s_graphicsoptions.mode.curvalue = i;
					break;
				}
			}
			if ( s_graphicsoptions.mode.curvalue < 0 )
				s_graphicsoptions.mode.curvalue = 0;
		}
		else
		{
			s_graphicsoptions.mode.curvalue = 3;
		}
	}
	s_graphicsoptions.fs.curvalue = trap_Cvar_VariableValue("r_fullscreen");
	s_graphicsoptions.allow_extensions.curvalue = trap_Cvar_VariableValue("r_allowExtensions");
        s_graphicsoptions.flares.curvalue = trap_Cvar_VariableValue("r_flares");
        s_graphicsoptions.bloom.curvalue = trap_Cvar_VariableValue("r_bloom");
        s_graphicsoptions.drawfps.curvalue = trap_Cvar_VariableValue("cg_drawFPS");
	
	// Advanced rendering features
	s_graphicsoptions.raytracing.curvalue = trap_Cvar_VariableValue("r_raytracing");
	s_graphicsoptions.rt_samples.curvalue = trap_Cvar_VariableValue("r_rt_samples");
	if ( s_graphicsoptions.rt_samples.curvalue < 1 ) s_graphicsoptions.rt_samples.curvalue = 1;
	if ( s_graphicsoptions.rt_samples.curvalue > 8 ) s_graphicsoptions.rt_samples.curvalue = 8;
	s_graphicsoptions.rt_maxdepth.curvalue = trap_Cvar_VariableValue("r_rt_maxDepth");
	if ( s_graphicsoptions.rt_maxdepth.curvalue < 1 ) s_graphicsoptions.rt_maxdepth.curvalue = 1;
	if ( s_graphicsoptions.rt_maxdepth.curvalue > 8 ) s_graphicsoptions.rt_maxdepth.curvalue = 8;
	s_graphicsoptions.rt_temporal.curvalue = trap_Cvar_VariableValue("r_rt_temporal");
	s_graphicsoptions.rt_temporalalpha.curvalue = (int)(trap_Cvar_VariableValue("r_rt_temporalAlpha") * 10.0f);
	if ( s_graphicsoptions.rt_temporalalpha.curvalue < 0 ) s_graphicsoptions.rt_temporalalpha.curvalue = 0;
	if ( s_graphicsoptions.rt_temporalalpha.curvalue > 10 ) s_graphicsoptions.rt_temporalalpha.curvalue = 10;
	s_graphicsoptions.rt_denoise.curvalue = trap_Cvar_VariableValue("r_rt_denoise");
	s_graphicsoptions.rt_denoisemode.curvalue = trap_Cvar_VariableValue("r_rt_denoiseMode");
	s_graphicsoptions.rt_denoiseiter.curvalue = trap_Cvar_VariableValue("r_rt_denoiseIterations");
	if ( s_graphicsoptions.rt_denoiseiter.curvalue < 1 ) s_graphicsoptions.rt_denoiseiter.curvalue = 1;
	if ( s_graphicsoptions.rt_denoiseiter.curvalue > 8 ) s_graphicsoptions.rt_denoiseiter.curvalue = 8;
	s_graphicsoptions.rt_gi.curvalue = trap_Cvar_VariableValue("r_rt_gi");
	s_graphicsoptions.rt_gibounces.curvalue = trap_Cvar_VariableValue("r_rt_giBounces");
	if ( s_graphicsoptions.rt_gibounces.curvalue < 1 ) s_graphicsoptions.rt_gibounces.curvalue = 1;
	if ( s_graphicsoptions.rt_gibounces.curvalue > 8 ) s_graphicsoptions.rt_gibounces.curvalue = 8;
	s_graphicsoptions.rt_giintensity.curvalue = (int)(trap_Cvar_VariableValue("r_rt_giIntensity") * 10.0f);
	if ( s_graphicsoptions.rt_giintensity.curvalue < 0 ) s_graphicsoptions.rt_giintensity.curvalue = 0;
	if ( s_graphicsoptions.rt_giintensity.curvalue > 20 ) s_graphicsoptions.rt_giintensity.curvalue = 20;
	s_graphicsoptions.dlss.curvalue = trap_Cvar_VariableValue("r_dlss");
	s_graphicsoptions.dlss_quality.curvalue = trap_Cvar_VariableValue("r_dlss_quality");
	s_graphicsoptions.dlss_sharpen.curvalue = (int)(trap_Cvar_VariableValue("r_dlss_sharpen") * 10.0f);
	if ( s_graphicsoptions.dlss_sharpen.curvalue < 0 ) s_graphicsoptions.dlss_sharpen.curvalue = 0;
	if ( s_graphicsoptions.dlss_sharpen.curvalue > 10 ) s_graphicsoptions.dlss_sharpen.curvalue = 10;
	s_graphicsoptions.postprocess_compute.curvalue = trap_Cvar_VariableValue("r_postprocess_compute");
	s_graphicsoptions.meshshaders.curvalue = trap_Cvar_VariableValue("r_meshShaders");
	s_graphicsoptions.meshletsize.curvalue = trap_Cvar_VariableValue("r_meshletSize") / 32;
	if ( s_graphicsoptions.meshletsize.curvalue < 1 ) s_graphicsoptions.meshletsize.curvalue = 1;
	if ( s_graphicsoptions.meshletsize.curvalue > 4 ) s_graphicsoptions.meshletsize.curvalue = 4;
	s_graphicsoptions.virtualtextures.curvalue = trap_Cvar_VariableValue("r_virtualTextures");
	s_graphicsoptions.vt_pagesize.curvalue = trap_Cvar_VariableValue("r_vt_pageSize") / 64;
	if ( s_graphicsoptions.vt_pagesize.curvalue < 2 ) s_graphicsoptions.vt_pagesize.curvalue = 2;
	if ( s_graphicsoptions.vt_pagesize.curvalue > 8 ) s_graphicsoptions.vt_pagesize.curvalue = 8;
	s_graphicsoptions.vt_cachesize.curvalue = trap_Cvar_VariableValue("r_vt_cacheSize") / 128;
	if ( s_graphicsoptions.vt_cachesize.curvalue < 1 ) s_graphicsoptions.vt_cachesize.curvalue = 1;
	if ( s_graphicsoptions.vt_cachesize.curvalue > 16 ) s_graphicsoptions.vt_cachesize.curvalue = 16;
	s_graphicsoptions.clearcoat.curvalue = trap_Cvar_VariableValue("r_clearcoat");
	s_graphicsoptions.anisotropy_mat.curvalue = trap_Cvar_VariableValue("r_anisotropy");
	s_graphicsoptions.subsurfacescattering.curvalue = trap_Cvar_VariableValue("r_subsurfaceScattering");
	s_graphicsoptions.particles_gpu.curvalue = trap_Cvar_VariableValue("r_particles_gpu");
	s_graphicsoptions.particles_max.curvalue = trap_Cvar_VariableValue("r_particles_max") / 10000;
	if ( s_graphicsoptions.particles_max.curvalue < 1 ) s_graphicsoptions.particles_max.curvalue = 1;
	if ( s_graphicsoptions.particles_max.curvalue > 50 ) s_graphicsoptions.particles_max.curvalue = 50;
	s_graphicsoptions.particles_culling.curvalue = trap_Cvar_VariableValue("r_particles_culling");
        if(trap_Cvar_VariableValue("r_ext_texture_filter_anisotropic")) {
            s_graphicsoptions.aniso.curvalue = trap_Cvar_VariableValue("r_ext_max_anisotropy")/2;
        }
	s_graphicsoptions.tq.curvalue = 3-trap_Cvar_VariableValue( "r_picmip");
	if ( s_graphicsoptions.tq.curvalue < 0 )
	{
		s_graphicsoptions.tq.curvalue = 0;
	}
	else if ( s_graphicsoptions.tq.curvalue > 3 )
	{
		s_graphicsoptions.tq.curvalue = 3;
	}

	s_graphicsoptions.lighting.curvalue = trap_Cvar_VariableValue( "r_vertexLight" ) != 0;
	switch ( ( int ) trap_Cvar_VariableValue( "r_texturebits" ) )
	{
	default:
	case 0:
		s_graphicsoptions.texturebits.curvalue = 0;
		break;
	case 16:
		s_graphicsoptions.texturebits.curvalue = 1;
		break;
	case 32:
		s_graphicsoptions.texturebits.curvalue = 2;
		break;
	}

	if ( Q_strequal( UI_Cvar_VariableString( "r_textureMode" ), "GL_LINEAR_MIPMAP_NEAREST" ) )
	{
		s_graphicsoptions.filter.curvalue = 0;
	}
	else
	{
		s_graphicsoptions.filter.curvalue = 1;
	}

	if ( trap_Cvar_VariableValue( "r_lodBias" ) > 0 )
	{
		if ( trap_Cvar_VariableValue( "r_subdivisions" ) >= 20 )
		{
			s_graphicsoptions.geometry.curvalue = 0;
		}
		else
		{
			s_graphicsoptions.geometry.curvalue = 1;
		}
	}
	else
	{
		s_graphicsoptions.geometry.curvalue = 2;
	}
}

/*
================
GraphicsOptions_MenuInit
================
*/
void GraphicsOptions_MenuInit( void )
{
	static const char *s_driver_names[] =
	{
		"Default",
		"Voodoo",
		NULL
	};

	static const char *tq_names[] =
	{
		"Default",
		"16 bit",
		"32 bit",
		NULL
	};

	static const char *s_graphics_options_names[] =
	{
		"Very High Quality",
		"High Quality",
		"Normal",
		"Fast",
		"Fastest",
		"Custom",
		NULL
	};

	static const char *lighting_names[] =
	{
		"Lightmap (Normal)",
		"Vertex (Low)",
		NULL
	};


	static const char *filter_names[] =
	{
		"Bilinear",
		"Trilinear",
		NULL
	};
        
        static const char *aniso_names[] =
	{
		"Off",
		"2x",
                "4x",
                "6x",
                "8x",
		NULL
	};
        
	static const char *quality_names[] =
	{
		"Low",
		"Medium",
		"High",
		NULL
	};
	static const char *enabled_names[] =
	{
		"Off",
		"On",
		NULL
	};
	static const char *rt_denoise_mode_names[] =
	{
		"SVGF",
		"ReLAX",
		NULL
	};
	static const char *dlss_quality_names[] =
	{
		"Performance",
		"Balanced",
		"Quality",
		"Ultra Quality",
		NULL
	};

	int y;

	// zero set all our globals
	memset( &s_graphicsoptions, 0 ,sizeof(graphicsoptions_t) );


        GraphicsOptions_GetResolutions();
        GraphicsOptions_GetAspectRatios();

	GraphicsOptions_Cache();

	s_graphicsoptions.menu.wrapAround = qtrue;
	s_graphicsoptions.menu.fullscreen = qtrue;
	s_graphicsoptions.menu.draw       = GraphicsOptions_MenuDraw;

	s_graphicsoptions.banner.generic.type  = MTYPE_BTEXT;
	s_graphicsoptions.banner.generic.x	   = 320;
	s_graphicsoptions.banner.generic.y	   = 16;
	s_graphicsoptions.banner.string  	   = "SYSTEM SETUP";
	s_graphicsoptions.banner.color         = color_white;
	s_graphicsoptions.banner.style         = UI_CENTER;

	s_graphicsoptions.framel.generic.type  = MTYPE_BITMAP;
	s_graphicsoptions.framel.generic.name  = GRAPHICSOPTIONS_FRAMEL;
	s_graphicsoptions.framel.generic.flags = QMF_INACTIVE;
	s_graphicsoptions.framel.generic.x	   = 0;
	s_graphicsoptions.framel.generic.y	   = 78;
	s_graphicsoptions.framel.width  	   = 256;
	s_graphicsoptions.framel.height  	   = 329;

	s_graphicsoptions.framer.generic.type  = MTYPE_BITMAP;
	s_graphicsoptions.framer.generic.name  = GRAPHICSOPTIONS_FRAMER;
	s_graphicsoptions.framer.generic.flags = QMF_INACTIVE;
	s_graphicsoptions.framer.generic.x	   = 376;
	s_graphicsoptions.framer.generic.y	   = 76;
	s_graphicsoptions.framer.width  	   = 256;
	s_graphicsoptions.framer.height  	   = 334;

	s_graphicsoptions.graphics.generic.type		= MTYPE_PTEXT;
	s_graphicsoptions.graphics.generic.flags	= QMF_RIGHT_JUSTIFY;
	s_graphicsoptions.graphics.generic.id		= ID_GRAPHICS;
	s_graphicsoptions.graphics.generic.callback	= GraphicsOptions_Event;
	s_graphicsoptions.graphics.generic.x		= 216;
	s_graphicsoptions.graphics.generic.y		= 240 - 2 * PROP_HEIGHT;
	s_graphicsoptions.graphics.string			= "GRAPHICS";
	s_graphicsoptions.graphics.style			= UI_RIGHT;
	s_graphicsoptions.graphics.color			= color_red;

	s_graphicsoptions.display.generic.type		= MTYPE_PTEXT;
	s_graphicsoptions.display.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_graphicsoptions.display.generic.id		= ID_DISPLAY;
	s_graphicsoptions.display.generic.callback	= GraphicsOptions_Event;
	s_graphicsoptions.display.generic.x			= 216;
	s_graphicsoptions.display.generic.y			= 240 - PROP_HEIGHT;
	s_graphicsoptions.display.string			= "DISPLAY";
	s_graphicsoptions.display.style				= UI_RIGHT;
	s_graphicsoptions.display.color				= color_red;

	s_graphicsoptions.sound.generic.type		= MTYPE_PTEXT;
	s_graphicsoptions.sound.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_graphicsoptions.sound.generic.id			= ID_SOUND;
	s_graphicsoptions.sound.generic.callback	= GraphicsOptions_Event;
	s_graphicsoptions.sound.generic.x			= 216;
	s_graphicsoptions.sound.generic.y			= 240;
	s_graphicsoptions.sound.string				= "SOUND";
	s_graphicsoptions.sound.style				= UI_RIGHT;
	s_graphicsoptions.sound.color				= color_red;

	s_graphicsoptions.network.generic.type		= MTYPE_PTEXT;
	s_graphicsoptions.network.generic.flags		= QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_graphicsoptions.network.generic.id		= ID_NETWORK;
	s_graphicsoptions.network.generic.callback	= GraphicsOptions_Event;
	s_graphicsoptions.network.generic.x			= 216;
	s_graphicsoptions.network.generic.y			= 240 + PROP_HEIGHT;
	s_graphicsoptions.network.string			= "NETWORK";
	s_graphicsoptions.network.style				= UI_RIGHT;
	s_graphicsoptions.network.color				= color_red;

	y = 240 - 7 * (BIGCHAR_HEIGHT + 2);
	s_graphicsoptions.list.generic.type     = MTYPE_SPINCONTROL;
	s_graphicsoptions.list.generic.name     = "Graphics Settings:";
	s_graphicsoptions.list.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.list.generic.x        = 400;
	s_graphicsoptions.list.generic.y        = y;
	s_graphicsoptions.list.generic.callback = GraphicsOptions_Event;
	s_graphicsoptions.list.generic.id       = ID_LIST;
	s_graphicsoptions.list.itemnames        = s_graphics_options_names;
	y += 2 * ( BIGCHAR_HEIGHT + 2 );

	s_graphicsoptions.driver.generic.type  = MTYPE_SPINCONTROL;
	s_graphicsoptions.driver.generic.name  = "GL Driver:";
	s_graphicsoptions.driver.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.driver.generic.x     = 400;
	s_graphicsoptions.driver.generic.y     = y;
	s_graphicsoptions.driver.itemnames     = s_driver_names;
	s_graphicsoptions.driver.curvalue      = (uis.glconfig.driverType == GLDRV_VOODOO);
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "r_allowExtensions"
	s_graphicsoptions.allow_extensions.generic.type     = MTYPE_SPINCONTROL;
	s_graphicsoptions.allow_extensions.generic.name	    = "GL Extensions:";
	s_graphicsoptions.allow_extensions.generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.allow_extensions.generic.x	    = 400;
	s_graphicsoptions.allow_extensions.generic.y	    = y;
	s_graphicsoptions.allow_extensions.itemnames        = enabled_names;
	y += BIGCHAR_HEIGHT+2;

        s_graphicsoptions.ratio.generic.type     = MTYPE_SPINCONTROL;
        s_graphicsoptions.ratio.generic.name     = "Aspect Ratio:";
        s_graphicsoptions.ratio.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
        s_graphicsoptions.ratio.generic.x        = 400;
        s_graphicsoptions.ratio.generic.y        = y;
        s_graphicsoptions.ratio.itemnames        = ratios;
        s_graphicsoptions.ratio.generic.callback = GraphicsOptions_Event;
        s_graphicsoptions.ratio.generic.id       = ID_RATIO;
        y += BIGCHAR_HEIGHT+2;


	// references/modifies "r_mode"
	s_graphicsoptions.mode.generic.type     = MTYPE_SPINCONTROL;
	s_graphicsoptions.mode.generic.name     = "Resolution:";
	s_graphicsoptions.mode.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.mode.generic.x        = 400;
	s_graphicsoptions.mode.generic.y        = y;
	s_graphicsoptions.mode.itemnames        = resolutions;
	s_graphicsoptions.mode.generic.callback = GraphicsOptions_Event;
	s_graphicsoptions.mode.generic.id       = ID_MODE;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "r_fullscreen"
	s_graphicsoptions.fs.generic.type     = MTYPE_SPINCONTROL;
	s_graphicsoptions.fs.generic.name	  = "Fullscreen:";
	s_graphicsoptions.fs.generic.flags	  = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.fs.generic.x	      = 400;
	s_graphicsoptions.fs.generic.y	      = y;
	s_graphicsoptions.fs.itemnames	      = enabled_names;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "r_vertexLight"
	s_graphicsoptions.lighting.generic.type  = MTYPE_SPINCONTROL;
	s_graphicsoptions.lighting.generic.name	 = "Lighting:";
	s_graphicsoptions.lighting.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.lighting.generic.x	 = 400;
	s_graphicsoptions.lighting.generic.y	 = y;
	s_graphicsoptions.lighting.itemnames     = lighting_names;
	y += BIGCHAR_HEIGHT+2;
        
        // references/modifies "r_flares"
	s_graphicsoptions.flares.generic.type     = MTYPE_SPINCONTROL;
	s_graphicsoptions.flares.generic.name	  = "Flares:";
	s_graphicsoptions.flares.generic.flags	  = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.flares.generic.x	      = 400;
	s_graphicsoptions.flares.generic.y	      = y;
	s_graphicsoptions.flares.itemnames	      = enabled_names;
	y += BIGCHAR_HEIGHT+2;
        
        // references/modifies "r_bloom"
	s_graphicsoptions.bloom.generic.type     = MTYPE_SPINCONTROL;
	s_graphicsoptions.bloom.generic.name	  = "Bloom:";
	s_graphicsoptions.bloom.generic.flags	  = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.bloom.generic.x	      = 400;
	s_graphicsoptions.bloom.generic.y	      = y;
	s_graphicsoptions.bloom.itemnames	      = enabled_names;
	y += BIGCHAR_HEIGHT+2;

	s_graphicsoptions.drawfps.generic.type	  = MTYPE_SPINCONTROL;
	s_graphicsoptions.drawfps.generic.name	  = "Draw FPS:";
	s_graphicsoptions.drawfps.generic.flags	  = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.drawfps.generic.x	      = 400;
	s_graphicsoptions.drawfps.generic.y	      = y;
	s_graphicsoptions.drawfps.itemnames	      = enabled_names;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "r_lodBias" & "subdivisions"
	s_graphicsoptions.geometry.generic.type  = MTYPE_SPINCONTROL;
	s_graphicsoptions.geometry.generic.name	 = "Geometric Detail:";
	s_graphicsoptions.geometry.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.geometry.generic.x	 = 400;
	s_graphicsoptions.geometry.generic.y	 = y;
	s_graphicsoptions.geometry.itemnames     = quality_names;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "r_picmip"
	s_graphicsoptions.tq.generic.type	= MTYPE_SLIDER;
	s_graphicsoptions.tq.generic.name	= "Texture Detail:";
	s_graphicsoptions.tq.generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.tq.generic.x		= 400;
	s_graphicsoptions.tq.generic.y		= y;
	s_graphicsoptions.tq.minvalue       = 0;
	s_graphicsoptions.tq.maxvalue       = 3;
	s_graphicsoptions.tq.generic.callback = GraphicsOptions_TQEvent;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "r_textureBits"
	s_graphicsoptions.texturebits.generic.type  = MTYPE_SPINCONTROL;
	s_graphicsoptions.texturebits.generic.name	= "Texture Quality:";
	s_graphicsoptions.texturebits.generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.texturebits.generic.x	    = 400;
	s_graphicsoptions.texturebits.generic.y	    = y;
	s_graphicsoptions.texturebits.itemnames     = tq_names;
	y += BIGCHAR_HEIGHT+2;

	// references/modifies "r_textureMode"
	s_graphicsoptions.filter.generic.type   = MTYPE_SPINCONTROL;
	s_graphicsoptions.filter.generic.name	= "Texture Filter:";
	s_graphicsoptions.filter.generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.filter.generic.x	    = 400;
	s_graphicsoptions.filter.generic.y	    = y;
	s_graphicsoptions.filter.itemnames      = filter_names;
	y += 2+BIGCHAR_HEIGHT;
        
        s_graphicsoptions.aniso.generic.type   = MTYPE_SPINCONTROL;
	s_graphicsoptions.aniso.generic.name	= "Anisotropy:";
	s_graphicsoptions.aniso.generic.flags	= QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.aniso.generic.x	    = 400;
	s_graphicsoptions.aniso.generic.y	    = y;
	s_graphicsoptions.aniso.itemnames      = aniso_names;
	y += BIGCHAR_HEIGHT+2;

	// Ray Tracing Section
	s_graphicsoptions.raytracing.generic.type     = MTYPE_SPINCONTROL;
	s_graphicsoptions.raytracing.generic.name    = "Ray Tracing:";
	s_graphicsoptions.raytracing.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.raytracing.generic.x        = 400;
	s_graphicsoptions.raytracing.generic.y        = y;
	s_graphicsoptions.raytracing.itemnames        = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_samples.generic.type     = MTYPE_SLIDER;
	s_graphicsoptions.rt_samples.generic.name     = "RT Samples:";
	s_graphicsoptions.rt_samples.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_samples.generic.x        = 400;
	s_graphicsoptions.rt_samples.generic.y        = y;
	s_graphicsoptions.rt_samples.minvalue         = 1;
	s_graphicsoptions.rt_samples.maxvalue         = 8;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_maxdepth.generic.type    = MTYPE_SLIDER;
	s_graphicsoptions.rt_maxdepth.generic.name     = "RT Max Depth:";
	s_graphicsoptions.rt_maxdepth.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_maxdepth.generic.x        = 400;
	s_graphicsoptions.rt_maxdepth.generic.y        = y;
	s_graphicsoptions.rt_maxdepth.minvalue         = 1;
	s_graphicsoptions.rt_maxdepth.maxvalue         = 8;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_temporal.generic.type    = MTYPE_SPINCONTROL;
	s_graphicsoptions.rt_temporal.generic.name   = "RT Temporal:";
	s_graphicsoptions.rt_temporal.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_temporal.generic.x       = 400;
	s_graphicsoptions.rt_temporal.generic.y       = y;
	s_graphicsoptions.rt_temporal.itemnames       = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_temporalalpha.generic.type = MTYPE_SLIDER;
	s_graphicsoptions.rt_temporalalpha.generic.name = "RT Temp Alpha:";
	s_graphicsoptions.rt_temporalalpha.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_temporalalpha.generic.x     = 400;
	s_graphicsoptions.rt_temporalalpha.generic.y     = y;
	s_graphicsoptions.rt_temporalalpha.minvalue      = 0;
	s_graphicsoptions.rt_temporalalpha.maxvalue      = 10;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_denoise.generic.type      = MTYPE_SPINCONTROL;
	s_graphicsoptions.rt_denoise.generic.name     = "RT Denoise:";
	s_graphicsoptions.rt_denoise.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_denoise.generic.x        = 400;
	s_graphicsoptions.rt_denoise.generic.y        = y;
	s_graphicsoptions.rt_denoise.itemnames        = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_denoisemode.generic.type = MTYPE_SPINCONTROL;
	s_graphicsoptions.rt_denoisemode.generic.name = "Denoise Mode:";
	s_graphicsoptions.rt_denoisemode.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_denoisemode.generic.x     = 400;
	s_graphicsoptions.rt_denoisemode.generic.y     = y;
	s_graphicsoptions.rt_denoisemode.itemnames     = rt_denoise_mode_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_denoiseiter.generic.type  = MTYPE_SLIDER;
	s_graphicsoptions.rt_denoiseiter.generic.name  = "Denoise Iter:";
	s_graphicsoptions.rt_denoiseiter.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_denoiseiter.generic.x     = 400;
	s_graphicsoptions.rt_denoiseiter.generic.y     = y;
	s_graphicsoptions.rt_denoiseiter.minvalue      = 1;
	s_graphicsoptions.rt_denoiseiter.maxvalue      = 8;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_gi.generic.type          = MTYPE_SPINCONTROL;
	s_graphicsoptions.rt_gi.generic.name           = "RT Global Illum:";
	s_graphicsoptions.rt_gi.generic.flags         = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_gi.generic.x              = 400;
	s_graphicsoptions.rt_gi.generic.y              = y;
	s_graphicsoptions.rt_gi.itemnames              = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_gibounces.generic.type    = MTYPE_SLIDER;
	s_graphicsoptions.rt_gibounces.generic.name    = "GI Bounces:";
	s_graphicsoptions.rt_gibounces.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_gibounces.generic.x       = 400;
	s_graphicsoptions.rt_gibounces.generic.y       = y;
	s_graphicsoptions.rt_gibounces.minvalue        = 1;
	s_graphicsoptions.rt_gibounces.maxvalue        = 8;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.rt_giintensity.generic.type  = MTYPE_SLIDER;
	s_graphicsoptions.rt_giintensity.generic.name   = "GI Intensity:";
	s_graphicsoptions.rt_giintensity.generic.flags  = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.rt_giintensity.generic.x      = 400;
	s_graphicsoptions.rt_giintensity.generic.y      = y;
	s_graphicsoptions.rt_giintensity.minvalue      = 0;
	s_graphicsoptions.rt_giintensity.maxvalue      = 20;
	y += BIGCHAR_HEIGHT+1;

	// DLSS Section
	s_graphicsoptions.dlss.generic.type            = MTYPE_SPINCONTROL;
	s_graphicsoptions.dlss.generic.name            = "DLSS:";
	s_graphicsoptions.dlss.generic.flags           = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.dlss.generic.x                = 400;
	s_graphicsoptions.dlss.generic.y                = y;
	s_graphicsoptions.dlss.itemnames                = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.dlss_quality.generic.type    = MTYPE_SPINCONTROL;
	s_graphicsoptions.dlss_quality.generic.name    = "DLSS Quality:";
	s_graphicsoptions.dlss_quality.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.dlss_quality.generic.x       = 400;
	s_graphicsoptions.dlss_quality.generic.y      = y;
	s_graphicsoptions.dlss_quality.itemnames       = dlss_quality_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.dlss_sharpen.generic.type    = MTYPE_SLIDER;
	s_graphicsoptions.dlss_sharpen.generic.name    = "DLSS Sharpen:";
	s_graphicsoptions.dlss_sharpen.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.dlss_sharpen.generic.x       = 400;
	s_graphicsoptions.dlss_sharpen.generic.y       = y;
	s_graphicsoptions.dlss_sharpen.minvalue        = 0;
	s_graphicsoptions.dlss_sharpen.maxvalue        = 10;
	y += BIGCHAR_HEIGHT+1;

	// Compute Post-Processing
	s_graphicsoptions.postprocess_compute.generic.type = MTYPE_SPINCONTROL;
	s_graphicsoptions.postprocess_compute.generic.name = "Compute PostProc:";
	s_graphicsoptions.postprocess_compute.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.postprocess_compute.generic.x     = 400;
	s_graphicsoptions.postprocess_compute.generic.y      = y;
	s_graphicsoptions.postprocess_compute.itemnames      = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	// Mesh Shaders
	s_graphicsoptions.meshshaders.generic.type      = MTYPE_SPINCONTROL;
	s_graphicsoptions.meshshaders.generic.name     = "Mesh Shaders:";
	s_graphicsoptions.meshshaders.generic.flags     = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.meshshaders.generic.x         = 400;
	s_graphicsoptions.meshshaders.generic.y         = y;
	s_graphicsoptions.meshshaders.itemnames         = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.meshletsize.generic.type      = MTYPE_SLIDER;
	s_graphicsoptions.meshletsize.generic.name     = "Meshlet Size:";
	s_graphicsoptions.meshletsize.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.meshletsize.generic.x         = 400;
	s_graphicsoptions.meshletsize.generic.y         = y;
	s_graphicsoptions.meshletsize.minvalue          = 1;
	s_graphicsoptions.meshletsize.maxvalue          = 4;
	y += BIGCHAR_HEIGHT+1;

	// Virtual Texturing
	s_graphicsoptions.virtualtextures.generic.type  = MTYPE_SPINCONTROL;
	s_graphicsoptions.virtualtextures.generic.name  = "Virtual Textures:";
	s_graphicsoptions.virtualtextures.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.virtualtextures.generic.x     = 400;
	s_graphicsoptions.virtualtextures.generic.y     = y;
	s_graphicsoptions.virtualtextures.itemnames     = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.vt_pagesize.generic.type     = MTYPE_SLIDER;
	s_graphicsoptions.vt_pagesize.generic.name     = "VT Page Size:";
	s_graphicsoptions.vt_pagesize.generic.flags    = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.vt_pagesize.generic.x         = 400;
	s_graphicsoptions.vt_pagesize.generic.y         = y;
	s_graphicsoptions.vt_pagesize.minvalue          = 2;
	s_graphicsoptions.vt_pagesize.maxvalue          = 8;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.vt_cachesize.generic.type     = MTYPE_SLIDER;
	s_graphicsoptions.vt_cachesize.generic.name    = "VT Cache Size:";
	s_graphicsoptions.vt_cachesize.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.vt_cachesize.generic.x        = 400;
	s_graphicsoptions.vt_cachesize.generic.y        = y;
	s_graphicsoptions.vt_cachesize.minvalue         = 1;
	s_graphicsoptions.vt_cachesize.maxvalue         = 16;
	y += BIGCHAR_HEIGHT+1;

	// Advanced Materials
	s_graphicsoptions.clearcoat.generic.type        = MTYPE_SPINCONTROL;
	s_graphicsoptions.clearcoat.generic.name       = "Clearcoat:";
	s_graphicsoptions.clearcoat.generic.flags      = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.clearcoat.generic.x          = 400;
	s_graphicsoptions.clearcoat.generic.y          = y;
	s_graphicsoptions.clearcoat.itemnames          = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.anisotropy_mat.generic.type  = MTYPE_SPINCONTROL;
	s_graphicsoptions.anisotropy_mat.generic.name  = "Material Aniso:";
	s_graphicsoptions.anisotropy_mat.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.anisotropy_mat.generic.x     = 400;
	s_graphicsoptions.anisotropy_mat.generic.y     = y;
	s_graphicsoptions.anisotropy_mat.itemnames     = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.subsurfacescattering.generic.type = MTYPE_SPINCONTROL;
	s_graphicsoptions.subsurfacescattering.generic.name = "Subsurface Scat:";
	s_graphicsoptions.subsurfacescattering.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.subsurfacescattering.generic.x     = 400;
	s_graphicsoptions.subsurfacescattering.generic.y     = y;
	s_graphicsoptions.subsurfacescattering.itemnames     = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	// GPU Particles
	s_graphicsoptions.particles_gpu.generic.type    = MTYPE_SPINCONTROL;
	s_graphicsoptions.particles_gpu.generic.name   = "GPU Particles:";
	s_graphicsoptions.particles_gpu.generic.flags  = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.particles_gpu.generic.x       = 400;
	s_graphicsoptions.particles_gpu.generic.y       = y;
	s_graphicsoptions.particles_gpu.itemnames       = enabled_names;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.particles_max.generic.type   = MTYPE_SLIDER;
	s_graphicsoptions.particles_max.generic.name   = "Max Particles:";
	s_graphicsoptions.particles_max.generic.flags   = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.particles_max.generic.x       = 400;
	s_graphicsoptions.particles_max.generic.y       = y;
	s_graphicsoptions.particles_max.minvalue        = 1;
	s_graphicsoptions.particles_max.maxvalue         = 50;
	y += BIGCHAR_HEIGHT+1;

	s_graphicsoptions.particles_culling.generic.type = MTYPE_SPINCONTROL;
	s_graphicsoptions.particles_culling.generic.name = "Particle Culling:";
	s_graphicsoptions.particles_culling.generic.flags = QMF_PULSEIFFOCUS|QMF_SMALLFONT;
	s_graphicsoptions.particles_culling.generic.x     = 400;
	s_graphicsoptions.particles_culling.generic.y     = y;
	s_graphicsoptions.particles_culling.itemnames     = enabled_names;
	y += BIGCHAR_HEIGHT+2;

	s_graphicsoptions.driverinfo.generic.type     = MTYPE_PTEXT;
	s_graphicsoptions.driverinfo.generic.flags    = QMF_CENTER_JUSTIFY|QMF_PULSEIFFOCUS;
	s_graphicsoptions.driverinfo.generic.callback = GraphicsOptions_Event;
	s_graphicsoptions.driverinfo.generic.id       = ID_DRIVERINFO;
	s_graphicsoptions.driverinfo.generic.x        = 320;
	s_graphicsoptions.driverinfo.generic.y        = y;
	s_graphicsoptions.driverinfo.string           = "Driver Info";
	s_graphicsoptions.driverinfo.style            = UI_CENTER|UI_SMALLFONT;
	s_graphicsoptions.driverinfo.color            = color_red;
	y += BIGCHAR_HEIGHT+2;

	s_graphicsoptions.back.generic.type	    = MTYPE_BITMAP;
	s_graphicsoptions.back.generic.name     = GRAPHICSOPTIONS_BACK0;
	s_graphicsoptions.back.generic.flags    = QMF_LEFT_JUSTIFY|QMF_PULSEIFFOCUS;
	s_graphicsoptions.back.generic.callback = GraphicsOptions_Event;
	s_graphicsoptions.back.generic.id	    = ID_BACK2;
	s_graphicsoptions.back.generic.x		= 0;
	s_graphicsoptions.back.generic.y		= 480-64;
	s_graphicsoptions.back.width  		    = 128;
	s_graphicsoptions.back.height  		    = 64;
	s_graphicsoptions.back.focuspic         = GRAPHICSOPTIONS_BACK1;

	s_graphicsoptions.apply.generic.type     = MTYPE_BITMAP;
	s_graphicsoptions.apply.generic.name     = GRAPHICSOPTIONS_ACCEPT0;
	s_graphicsoptions.apply.generic.flags    = QMF_RIGHT_JUSTIFY|QMF_PULSEIFFOCUS|QMF_HIDDEN|QMF_INACTIVE;
	s_graphicsoptions.apply.generic.callback = GraphicsOptions_ApplyChanges;
	s_graphicsoptions.apply.generic.x        = 640;
	s_graphicsoptions.apply.generic.y        = 480-64;
	s_graphicsoptions.apply.width  		     = 128;
	s_graphicsoptions.apply.height  		 = 64;
	s_graphicsoptions.apply.focuspic         = GRAPHICSOPTIONS_ACCEPT1;

	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.banner );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.framel );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.framer );

	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.graphics );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.display );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.sound );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.network );

	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.list );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.driver );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.allow_extensions );
        Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.ratio );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.mode );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.fs );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.lighting );
        Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.flares );
        Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.bloom );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.drawfps );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.geometry );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.tq );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.texturebits );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.filter );
        Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.aniso );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.raytracing );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_samples );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_maxdepth );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_temporal );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_temporalalpha );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_denoise );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_denoisemode );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_denoiseiter );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_gi );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_gibounces );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.rt_giintensity );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.dlss );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.dlss_quality );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.dlss_sharpen );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.postprocess_compute );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.meshshaders );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.meshletsize );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.virtualtextures );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.vt_pagesize );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.vt_cachesize );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.clearcoat );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.anisotropy_mat );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.subsurfacescattering );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.particles_gpu );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.particles_max );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.particles_culling );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.driverinfo );

	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.back );
	Menu_AddItem( &s_graphicsoptions.menu, ( void * ) &s_graphicsoptions.apply );

	GraphicsOptions_SetMenuItems();
	GraphicsOptions_GetInitialVideo();

	if ( uis.glconfig.driverType == GLDRV_ICD &&
		 uis.glconfig.hardwareType == GLHW_3DFX_2D3D )
	{
		s_graphicsoptions.driver.generic.flags |= QMF_HIDDEN|QMF_INACTIVE;
	}
}


/*
=================
GraphicsOptions_Cache
=================
*/
void GraphicsOptions_Cache( void ) {
	trap_R_RegisterShaderNoMip( GRAPHICSOPTIONS_FRAMEL );
	trap_R_RegisterShaderNoMip( GRAPHICSOPTIONS_FRAMER );
	trap_R_RegisterShaderNoMip( GRAPHICSOPTIONS_BACK0 );
	trap_R_RegisterShaderNoMip( GRAPHICSOPTIONS_BACK1 );
	trap_R_RegisterShaderNoMip( GRAPHICSOPTIONS_ACCEPT0 );
	trap_R_RegisterShaderNoMip( GRAPHICSOPTIONS_ACCEPT1 );
}


/*
=================
UI_GraphicsOptionsMenu
=================
*/
void UI_GraphicsOptionsMenu( void ) {
	GraphicsOptions_MenuInit();
	UI_PushMenu( &s_graphicsoptions.menu );
	Menu_SetCursorToItem( &s_graphicsoptions.menu, &s_graphicsoptions.graphics );
}
