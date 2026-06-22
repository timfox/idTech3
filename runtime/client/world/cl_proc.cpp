/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client console for procedural world patterns (Voronoi, grid, hex, ...).
===========================================================================
*/

extern "C" {
#include "client.h"
#include "../../world/world_proc.h"
}

static void CL_Proc_Pattern_f( void ) {
	const char *name;

	if ( Cmd_Argc() < 2 ) {
		WorldProc_ListPatterns();
		Com_Printf( "Usage: proc_pattern <grid|checker|voronoi|hex|radial|stripe_h|stripe_v|noise>\n" );
		return;
	}
	name = Cmd_Argv( 1 );
	{
		worldProcPattern_t p = WorldProc_ParsePattern( name );
		if ( p >= WPROC_COUNT ) {
			Com_Printf( S_COLOR_YELLOW "Unknown pattern '%s'\n", name );
			WorldProc_ListPatterns();
			return;
		}
		Cvar_Set( "r_procPattern", WorldProc_PatternName( p ) );
		Com_Printf( "[world_proc] pattern -> %s\n", WorldProc_PatternName( p ) );
	}
}

static void CL_Proc_Seed_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: proc_seed <int>\n" );
		return;
	}
	Cvar_Set( "r_procSeed", Cmd_Argv( 1 ) );
	Com_Printf( "[world_proc] seed -> %s\n", Cmd_Argv( 1 ) );
}

static void CL_Proc_Scale_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: proc_scale <world_units>\n" );
		return;
	}
	Cvar_Set( "r_procScale", Cmd_Argv( 1 ) );
}

static void CL_Proc_Grid_f( void ) {
	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: proc_grid <width_cells> <height_cells>\n" );
		return;
	}
	Cvar_Set( "r_procGridW", Cmd_Argv( 1 ) );
	Cvar_Set( "r_procGridH", Cmd_Argv( 2 ) );
	Cvar_Set( "r_procPattern", "grid" );
}

static void CL_Proc_Sample_f( void ) {
	float x, y;

	if ( Cmd_Argc() >= 3 ) {
		x = (float)atof( Cmd_Argv( 1 ) );
		y = (float)atof( Cmd_Argv( 2 ) );
	} else if ( cl.snap.valid ) {
		x = cl.snap.ps.origin[0];
		y = cl.snap.ps.origin[1];
	} else {
		Com_Printf( "Usage: proc_sample [x y]  (defaults to player origin)\n" );
		return;
	}
	WorldProc_DumpSample( x, y );
}

static void CL_Proc_SampleCell_f( void ) {
	int cx, cy;
	float sectorSize;
	worldProcSample_t s;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: proc_sample_cell <cellX> <cellY>\n" );
		return;
	}
	cx = atoi( Cmd_Argv( 1 ) );
	cy = atoi( Cmd_Argv( 2 ) );
	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorSize < 256.0f ) {
		sectorSize = Cvar_VariableValue( "r_procScale" );
	}
	s = WorldProc_SampleSector( cx, cy, sectorSize );
	Com_Printf( "proc cell %d,%d -> region %d palette %d\n", cx, cy, s.regionId, s.paletteIndex );
}

static void CL_Proc_List_f( void ) {
	WorldProc_ListPatterns();
}

static void CL_Proc_Map_f( void ) {
	int cx, cy, w;
	float sectorSize;
	int x, y;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: proc_map <centerCellX> <centerCellY> [radius]\n" );
		return;
	}
	cx = atoi( Cmd_Argv( 1 ) );
	cy = atoi( Cmd_Argv( 2 ) );
	w = ( Cmd_Argc() >= 4 ) ? atoi( Cmd_Argv( 3 ) ) : 3;
	if ( w < 1 ) {
		w = 1;
	}
	if ( w > 8 ) {
		w = 8;
	}
	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorSize < 256.0f ) {
		sectorSize = Cvar_VariableValue( "r_procScale" );
	}
	Com_Printf( "proc map (region id) center %d,%d radius %d:\n", cx, cy, w );
	for ( y = cy + w; y >= cy - w; y-- ) {
		Com_Printf( "%3d ", y );
		for ( x = cx - w; x <= cx + w; x++ ) {
			int rid = WorldProc_RegionAtSector( x, y, sectorSize );
			int digit = rid % 10;
			if ( digit < 0 ) {
				digit = -digit % 10;
			}
			Com_Printf( "%c", '0' + digit );
		}
		Com_Printf( "\n" );
	}
	Com_Printf( "    " );
	for ( x = cx - w; x <= cx + w; x++ ) {
		Com_Printf( "%1d", ( x >= 0 ) ? ( x % 10 ) : ( ( -x ) % 10 ) );
	}
	Com_Printf( "\n" );
}

extern "C" void CL_Proc_Init( void ) {
	WorldProc_Init();

	Cmd_AddCommand( "proc_pattern", CL_Proc_Pattern_f );
	Cmd_AddCommand( "proc_seed", CL_Proc_Seed_f );
	Cmd_AddCommand( "proc_scale", CL_Proc_Scale_f );
	Cmd_AddCommand( "proc_grid", CL_Proc_Grid_f );
	Cmd_AddCommand( "proc_sample", CL_Proc_Sample_f );
	Cmd_AddCommand( "proc_sample_cell", CL_Proc_SampleCell_f );
	Cmd_AddCommand( "proc_list", CL_Proc_List_f );
	Cmd_AddCommand( "proc_map", CL_Proc_Map_f );

	Com_Printf( "Procedural patterns: proc_pattern voronoi, proc_map, proc_sample (r_proc 1)\n" );
}
