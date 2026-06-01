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
// cl_main.c  -- client main loop

#include "client.h"
#include "cl_gameframe.h"
#include "cl_emoji.h"
#include "cl_osp.h"
#include "cl_voip.h"
#include "cl_mumble.h"
#include "cl_superhud.h"
#include "ui_css.h"
#include "cl_websocket.h"
#include "cl_steam.h"
#include "cl_menuvideo.h"
#include "cl_sdf_font.h"
#include "cl_demo.h"
#include "../qcommon/script_emit.h"
#ifdef USE_LUA
#include "../qcommon/lua_debug.h"
#include "g_lua_bindings.h"
#endif
#include <limits.h>
#ifdef USE_FLUX
#include "flux.h"
#endif
#include "cl_pipeline.h"
#ifdef USE_SPEC_ENERGY
#include "cl_spec_energy.h"
#endif
#if ( defined( USE_FLUX ) || defined( USE_TRELLIS ) || defined( USE_SPEC_ENERGY ) ) && USE_SDL
#include <SDL2/SDL_thread.h>
#elif defined( USE_FLUX ) || defined( USE_TRELLIS ) || defined( USE_SPEC_ENERGY )
typedef struct SDL_Thread SDL_Thread;
#endif

cvar_t	*cl_noprint;
cvar_t	*cl_debugMove;
cvar_t	*cl_motd;

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
static cvar_t *cl_renderer;
static cvar_t *cl_renderer_force;
static qboolean isValidRenderer( const char *s );
#endif

cvar_t	*rcon_client_password;
cvar_t	*rconAddress;

cvar_t	*cl_timeout;
cvar_t	*cl_autoNudge;
cvar_t	*cl_timeNudge;
cvar_t	*cl_showTimeDelta;

cvar_t	*cl_shownet;
cvar_t	*cl_autoRecordDemo;
cvar_t	*cl_drawRecording;

cvar_t	*cl_aviFrameRate;
cvar_t	*cl_aviMotionJpeg;
cvar_t	*cl_forceavidemo;
cvar_t	*cl_aviPipeFormat;

cvar_t	*cl_activeAction;

cvar_t	*cl_motdString;

cvar_t	*cl_allowDownload;
#ifdef USE_CURL
cvar_t	*cl_mapAutoDownload;
#endif
cvar_t	*cl_conXOffset;
cvar_t	*cl_conColor;
cvar_t	*cl_inGameVideo;

cvar_t	*cl_serverStatusResendTime;

cvar_t	*cl_lanForcePackets;

cvar_t	*cl_guidServerUniq;

cvar_t	*cl_dlURL;
cvar_t	*cl_dlDirectory;

cvar_t	*cl_reconnectArgs;

void CL_JsNotifyMenuChanged( int menu ) {
	const char *menuName = "unknown";

	switch ( menu ) {
		case UIMENU_NONE:
			menuName = "none";
			break;
		case UIMENU_MAIN:
			menuName = "main";
			break;
		case UIMENU_INGAME:
			menuName = "ingame";
			break;
		case UIMENU_NEED_CD:
			menuName = "need_cd";
			break;
		case UIMENU_BAD_CD_KEY:
			menuName = "bad_cd_key";
			break;
		case UIMENU_TEAM:
			menuName = "team";
			break;
		case UIMENU_POSTGAME:
			menuName = "postgame";
			break;
		default:
			break;
	}

	Com_ScriptEmitEvent( "menu_changed", menuName, NULL, menu, 0 );
	Com_ScriptSetCurrentMenu( menu );
}

static qboolean CL_SetActiveMenuByName( const char *name ) {
	int menu = -1;

	if ( !uivm || !name || !name[0] ) {
		return qfalse;
	}

	if ( !Q_stricmp( name, "none" ) || !Q_stricmp( name, "close" ) ) {
		menu = UIMENU_NONE;
	} else if ( !Q_stricmp( name, "main" ) || !Q_stricmp( name, "menu" ) || !Q_stricmp( name, "home" ) ) {
		menu = UIMENU_MAIN;
	} else if ( !Q_stricmp( name, "ingame" ) || !Q_stricmp( name, "pause" ) ) {
		menu = UIMENU_INGAME;
	} else if ( !Q_stricmp( name, "need_cd" ) || !Q_stricmp( name, "needcd" ) ) {
		menu = UIMENU_NEED_CD;
	} else if ( !Q_stricmp( name, "bad_cd_key" ) || !Q_stricmp( name, "badcdkey" ) ) {
		menu = UIMENU_BAD_CD_KEY;
	} else if ( !Q_stricmp( name, "team" ) ) {
		menu = UIMENU_TEAM;
	} else if ( !Q_stricmp( name, "postgame" ) ) {
		menu = UIMENU_POSTGAME;
	}

	if ( menu < 0 ) {
		return qfalse;
	}

	VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, menu );
	CL_JsNotifyMenuChanged( menu );
	return qtrue;
}

static void CL_Open_f( void ) {
	char target[MAX_TOKEN_CHARS];

	if ( Cmd_Argc() < 2 ) {
		if ( !CL_SetActiveMenuByName( "main" ) ) {
			Com_Printf( "open: UI is not available\n" );
		}
		return;
	}

	Q_strncpyz( target, Cmd_Argv( 1 ), sizeof( target ) );
	Q_CleanStr( target );

	if ( !target[0] ) {
		Com_Printf( "open: empty target\n" );
		return;
	}

	/* First handle direct menu ids locally. */
	if ( CL_SetActiveMenuByName( target ) ) {
		Cvar_Set( "ui_open_tab", "" );
		return;
	}

	/* Preserve the VM-driven UI path for custom menu commands/scripts. */
	if ( uivm && UI_GameCommand() ) {
		return;
	}

	/* Keep legacy JS/C# "open <tab>" flows alive by advertising a menu_changed event. */
	Com_ScriptEmitEvent( "menu_changed", target, NULL, -1, 0 );

	/* Common tabs live under main menu in most UI scripts. Set ui_open_tab
	 * so the UI can switch to the requested tab when main menu opens. */
	if ( !Q_stricmp( target, "credits" ) || !Q_stricmp( target, "audio" ) || !Q_stricmp( target, "gameplay" ) ) {
		Cvar_Set( "ui_open_tab", target );
		CL_SetActiveMenuByName( "main" );
		return;
	}

	Cvar_Set( "ui_open_tab", "" );
	Com_Printf( "open: unhandled target '%s'\n", target );
}

#ifdef USE_TRELLIS
cvar_t	*cl_trellis_enable;
cvar_t	*cl_trellis_async;
cvar_t	*cl_trellis_auto_import;
cvar_t	*cl_trellis_chain;
cvar_t	*cl_trellis_repo;
cvar_t	*cl_trellis_python;
cvar_t	*cl_trellis_conda;
cvar_t	*cl_trellis_cmd;
cvar_t	*cl_trellis_hf_model;
cvar_t	*cl_trellis_decimation;
cvar_t	*cl_trellis_texture_size;
cvar_t	*cl_trellis_timeout;

typedef enum {
	TRELLIS_JOB_IDLE,
	TRELLIS_JOB_RUNNING,
	TRELLIS_JOB_COMPLETED,
	TRELLIS_JOB_FAILED
} trellis_job_status_t;

typedef struct {
	trellis_job_status_t status;
	qboolean notified;
	int start_time;
	int timeout_seconds;
	char repo[MAX_OSPATH];
	char image_full[MAX_OSPATH];
	char image_vfs[MAX_QPATH];
	char output_full[MAX_OSPATH];
	char output_vfs[MAX_QPATH];
	char hf_model[256];
	char dec_str[32];
	char tex_str[32];
	char error_msg[1024];
	qhandle_t model_handle;
	SDL_Thread *thread;
} trellis_job_t;

static trellis_job_t trellis_job;
#if defined( USE_FLUX )
static qboolean trellis_chain_armed;
#endif

static const char *CL_TrellisDefaultCmd( void ) {
	return "conda run -n %N --no-capture-output %P \"%E/trellis_image_to_glb.py\" --repo \"%R\" --image \"%I\" --output \"%O\" --model \"%M\" --decimation %D --texture-size %T %A";
}

static qboolean CL_TrellisFileExists( const char *path ) {
	FILE *f;

	if ( !path || !path[0] ) {
		return qfalse;
	}
	f = fopen( path, "rb" );
	if ( f ) {
		fclose( f );
		return qtrue;
	}
	return qfalse;
}

static void CL_TrellisResolvePath( const char *base, const char *in, char *out, size_t out_size ) {
	if ( !in || !in[0] || !out || out_size == 0 ) {
		if ( out && out_size > 0 ) {
			out[0] = '\0';
		}
		return;
	}
	if ( in[0] == '/' || ( in[0] && in[1] == ':' ) ) {
		Q_strncpyz( out, in, out_size );
		return;
	}
	if ( base && base[0] ) {
		Com_sprintf( out, out_size, "%s/%s", base, in );
	} else {
		Q_strncpyz( out, in, out_size );
	}
}

static void CL_TrellisEnsureOutputDir( const char *output_full ) {
	char *slash;
	char dir[MAX_OSPATH];
	char mkdir_cmd[1024];

	slash = strrchr( output_full, '/' );
	if ( !slash ) {
		return;
	}
	if ( (size_t)( slash - output_full ) >= sizeof( dir ) ) {
		return;
	}
	memcpy( dir, output_full, (size_t)( slash - output_full ) );
	dir[slash - output_full] = '\0';
	Com_sprintf( mkdir_cmd, sizeof( mkdir_cmd ), "mkdir -p \"%s\"", dir );
	system( mkdir_cmd );
}

static qboolean CL_TrellisRunExpanded( const char *tmpl, const cl_pipeline_expand_t *ex, qboolean quiet ) {
	char cmd[8192];

	if ( !CL_PipelineExpandTemplate( cmd, sizeof( cmd ), tmpl, ex ) ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "TRELLIS: expanded command too long or bad path characters\n" );
		}
		return qfalse;
	}
	if ( !quiet ) {
		Com_Printf( "TRELLIS: executing: %s\n", cmd );
	}
	if ( system( cmd ) != 0 ) {
		if ( !quiet ) {
			Com_Printf( S_COLOR_RED "TRELLIS: shell returned non-zero\n" );
		}
		return qfalse;
	}
	return qtrue;
}

static qboolean CL_TrellisBuildExpandFromJob( cl_pipeline_expand_t *ex ) {
	const char *base;
	const char *py;
	const char *conda;

	if ( !ex ) {
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	py = ( cl_trellis_python && cl_trellis_python->string[0] ) ? cl_trellis_python->string : "python3";
	conda = ( cl_trellis_conda && cl_trellis_conda->string[0] ) ? cl_trellis_conda->string : "trellis2";
	Com_Memset( ex, 0, sizeof( *ex ) );
	ex->repo = trellis_job.repo;
	ex->base = base;
	ex->engine = base;
	ex->py = py;
	ex->conda = conda;
	ex->image = trellis_job.image_full;
	ex->output = trellis_job.output_full;
	ex->model = trellis_job.hf_model;
	ex->decimation = trellis_job.dec_str;
	ex->texture_size = trellis_job.tex_str;
	ex->args = "";
	return qtrue;
}

static qhandle_t CL_TrellisImportVfs( const char *vfs_path ) {
	if ( !vfs_path || !vfs_path[0] || !re.RegisterModel ) {
		return 0;
	}
	return re.RegisterModel( vfs_path );
}

static void CL_TrellisSetJobResult( qboolean success ) {
	if ( success ) {
		trellis_job.status = TRELLIS_JOB_COMPLETED;
	} else {
		trellis_job.status = TRELLIS_JOB_FAILED;
	}
}

/*
CL_TrellisFinalizeOnMain
Must run on the main thread (RegisterModel / renderer).
*/
static void CL_TrellisFinalizeOnMain( void ) {
	if ( trellis_job.status != TRELLIS_JOB_COMPLETED ) {
		return;
	}
	if ( cl_trellis_auto_import && cl_trellis_auto_import->integer ) {
		trellis_job.model_handle = CL_TrellisImportVfs( trellis_job.output_vfs );
		if ( trellis_job.model_handle ) {
			Com_Printf( S_COLOR_GREEN "TRELLIS: auto-imported '%s' (handle %d)\n",
				trellis_job.output_vfs, trellis_job.model_handle );
		} else {
			Com_Printf( S_COLOR_YELLOW "TRELLIS: GLB written but auto-import failed; try trellis_view %s\n",
				trellis_job.output_vfs );
		}
	} else {
		Com_Printf( S_COLOR_GREEN "TRELLIS: wrote %s — trellis_view %s\n",
			trellis_job.output_vfs, trellis_job.output_vfs );
	}
}

static void CL_TrellisJoinThread( void ) {
#if USE_SDL
	if ( trellis_job.thread ) {
		int rc = 0;
		SDL_WaitThread( trellis_job.thread, &rc );
		trellis_job.thread = NULL;
		(void)rc;
	}
#endif
}

#if USE_SDL
static int CL_TrellisGenerationThread( void *data ) {
	trellis_job_t *job = (trellis_job_t *)data;
	cl_pipeline_expand_t ex;
	const char *tmpl;

	if ( !job ) {
		return -1;
	}
	if ( !CL_TrellisBuildExpandFromJob( &ex ) ) {
		Q_strncpyz( job->error_msg, "Failed to build TRELLIS command", sizeof( job->error_msg ) );
		job->status = TRELLIS_JOB_FAILED;
		return -1;
	}
	tmpl = ( cl_trellis_cmd && cl_trellis_cmd->string[0] ) ? cl_trellis_cmd->string : CL_TrellisDefaultCmd();
	if ( !CL_TrellisRunExpanded( tmpl, &ex, qtrue ) ) {
		Q_strncpyz( job->error_msg, "TRELLIS generation subprocess failed", sizeof( job->error_msg ) );
		job->status = TRELLIS_JOB_FAILED;
		return -1;
	}
	if ( !CL_TrellisFileExists( job->output_full ) ) {
		Q_strncpyz( job->error_msg, "TRELLIS output GLB missing after subprocess", sizeof( job->error_msg ) );
		job->status = TRELLIS_JOB_FAILED;
		return -1;
	}
	CL_TrellisSetJobResult( qtrue );
	return 0;
}
#endif

static qboolean CL_TrellisStartJob( const char *image_rel, const char *output_rel_optional ) {
	cl_pipeline_expand_t ex;
	const char *tmpl;
	const char *base;
	const char *repo;
	const char *hf_model;

	if ( trellis_job.status == TRELLIS_JOB_RUNNING ) {
		Com_Printf( S_COLOR_YELLOW "TRELLIS: generation already in progress (trellis_status / trellis_cancel)\n" );
		return qfalse;
	}
	repo = cl_trellis_repo ? cl_trellis_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "TRELLIS: set cl_trellis_repo to your TRELLIS.2 checkout\n" );
		return qfalse;
	}
	base = Sys_DefaultBasePath();
	if ( !base ) {
		Com_Printf( S_COLOR_RED "TRELLIS: no engine base path\n" );
		return qfalse;
	}
	Com_Memset( &trellis_job, 0, sizeof( trellis_job ) );
	trellis_job.start_time = Com_Milliseconds();
	trellis_job.timeout_seconds = cl_trellis_timeout ? cl_trellis_timeout->integer : 3600;
	Q_strncpyz( trellis_job.repo, repo, sizeof( trellis_job.repo ) );
	Q_strncpyz( trellis_job.image_vfs, image_rel, sizeof( trellis_job.image_vfs ) );
	CL_TrellisResolvePath( base, image_rel, trellis_job.image_full, sizeof( trellis_job.image_full ) );
	if ( !CL_TrellisFileExists( trellis_job.image_full ) ) {
		Com_Printf( S_COLOR_RED "TRELLIS: input image not found: %s\n", trellis_job.image_full );
		return qfalse;
	}
	if ( output_rel_optional && output_rel_optional[0] ) {
		Q_strncpyz( trellis_job.output_vfs, output_rel_optional, sizeof( trellis_job.output_vfs ) );
	} else {
		Com_sprintf( trellis_job.output_vfs, sizeof( trellis_job.output_vfs ),
			"models/trellis/trellis_%d.glb", Com_Milliseconds() );
	}
	if ( !strstr( trellis_job.output_vfs, ".glb" ) && !strstr( trellis_job.output_vfs, ".gltf" ) ) {
		Q_strcat( trellis_job.output_vfs, sizeof( trellis_job.output_vfs ), ".glb" );
	}
	CL_TrellisResolvePath( base, trellis_job.output_vfs, trellis_job.output_full, sizeof( trellis_job.output_full ) );
	CL_TrellisEnsureOutputDir( trellis_job.output_full );
	hf_model = ( cl_trellis_hf_model && cl_trellis_hf_model->string[0] ) ?
		cl_trellis_hf_model->string : "microsoft/TRELLIS.2-4B";
	Q_strncpyz( trellis_job.hf_model, hf_model, sizeof( trellis_job.hf_model ) );
	Com_sprintf( trellis_job.dec_str, sizeof( trellis_job.dec_str ), "%d",
		cl_trellis_decimation ? cl_trellis_decimation->integer : 500000 );
	Com_sprintf( trellis_job.tex_str, sizeof( trellis_job.tex_str ), "%d",
		cl_trellis_texture_size ? cl_trellis_texture_size->integer : 2048 );

	if ( cl_trellis_async && cl_trellis_async->integer ) {
#if USE_SDL
		trellis_job.status = TRELLIS_JOB_RUNNING;
		trellis_job.thread = SDL_CreateThread( CL_TrellisGenerationThread, "TRELLIS_Generation", &trellis_job );
		if ( !trellis_job.thread ) {
			trellis_job.status = TRELLIS_JOB_IDLE;
			Com_Printf( S_COLOR_RED "TRELLIS: failed to create background thread\n" );
			return qfalse;
		}
		Com_Printf( "TRELLIS: started background generation from %s -> %s\n",
			trellis_job.image_vfs, trellis_job.output_vfs );
		Com_Printf( "TRELLIS: Use trellis_status / trellis_cancel; model auto-imports when cl_trellis_auto_import 1\n" );
		return qtrue;
#else
		Com_Printf( S_COLOR_YELLOW "TRELLIS: async requires SDL threads; running synchronously\n" );
#endif
	}

	trellis_job.status = TRELLIS_JOB_RUNNING;
	if ( !CL_TrellisBuildExpandFromJob( &ex ) ) {
		trellis_job.status = TRELLIS_JOB_IDLE;
		return qfalse;
	}
	tmpl = ( cl_trellis_cmd && cl_trellis_cmd->string[0] ) ? cl_trellis_cmd->string : CL_TrellisDefaultCmd();
	if ( !CL_TrellisRunExpanded( tmpl, &ex, qfalse ) ||
		!CL_TrellisFileExists( trellis_job.output_full ) ) {
		Q_strncpyz( trellis_job.error_msg, "TRELLIS generation failed", sizeof( trellis_job.error_msg ) );
		trellis_job.status = TRELLIS_JOB_FAILED;
		Com_Printf( S_COLOR_RED "TRELLIS: generation failed\n" );
		return qfalse;
	}
	CL_TrellisSetJobResult( qtrue );
	CL_TrellisFinalizeOnMain();
	trellis_job.notified = qtrue;
	return qtrue;
}

static void CL_TrellisFrame( void ) {
	if ( trellis_job.status == TRELLIS_JOB_RUNNING && trellis_job.timeout_seconds > 0 ) {
		int runtime = ( Com_Milliseconds() - trellis_job.start_time ) / 1000;
		if ( runtime > trellis_job.timeout_seconds ) {
			Com_Printf( S_COLOR_YELLOW "TRELLIS: generation exceeded %d s (still running); trellis_cancel to stop\n",
				trellis_job.timeout_seconds );
			trellis_job.timeout_seconds = 0;
		}
		return;
	}
	if ( trellis_job.status != TRELLIS_JOB_COMPLETED && trellis_job.status != TRELLIS_JOB_FAILED ) {
		return;
	}
	if ( trellis_job.notified ) {
		return;
	}
	CL_TrellisJoinThread();
	trellis_job.notified = qtrue;
	if ( trellis_job.status == TRELLIS_JOB_COMPLETED ) {
		CL_TrellisFinalizeOnMain();
	} else {
		Com_Printf( S_COLOR_RED "TRELLIS: generation failed: %s\n", trellis_job.error_msg );
	}
}

#endif

#ifdef USE_FLUX
// FLUX image generation cvars
cvar_t	*cl_flux_enable;
cvar_t	*cl_flux_async;        // 0 = synchronous (blocking), 1 = asynchronous (background)
cvar_t	*cl_flux_external;     // 0 = in-process (unstable), 1 = external CLI (recommended)
cvar_t	*cl_flux_model;        // Model variant: flux1-schnell, flux1-dev, flux2-dev
cvar_t	*cl_flux_device;       // Compute device: auto, cpu, gpu, gpu:N (specific GPU)
cvar_t	*cl_flux_width;
cvar_t	*cl_flux_height;
cvar_t	*cl_flux_steps;
cvar_t	*cl_flux_seed;
/* FonTS (ICCV 2025): external Python DiT pipeline; see docs/FONTS.md */
cvar_t	*cl_fonts_enable;
cvar_t	*cl_fonts_repo;
cvar_t	*cl_fonts_python;
cvar_t	*cl_fonts_cmd;

// FLUX job system for background generation
typedef enum {
	FLUX_JOB_IDLE,
	FLUX_JOB_RUNNING,
	FLUX_JOB_COMPLETED,
	FLUX_JOB_FAILED
} flux_job_status_t;

// Helper to check if file exists
static qboolean CL_FluxFileExists(const char *path) {
	FILE *f = fopen(path, "r");
	if (f) {
		fclose(f);
		return qtrue;
	}
	return qfalse;
}

static qboolean CL_FluxFindCliPath(char *out, size_t out_size) {
	const char *base_path = Sys_DefaultBasePath();
	if (!base_path || !out || out_size == 0) {
		return qfalse;
	}

	Com_sprintf(out, out_size, "%s/flux_cli", base_path);
	if (CL_FluxFileExists(out)) {
		return qtrue;
	}

	Com_sprintf(out, out_size, "%s/flux_cli.x86_64", base_path);
	if (CL_FluxFileExists(out)) {
		return qtrue;
	}

	out[0] = '\0';
	return qfalse;
}

static qboolean CL_FluxGenerateExternal(const char *model_path, const char *prompt,
										const char *output_path, int width, int height,
										int steps, int seed,
										char *error_msg, size_t error_msg_size) {
	char cli_path[MAX_OSPATH];
	char model_full[MAX_OSPATH];
	char output_full[MAX_OSPATH];
	char prompt_escaped[2048];
	char cmd[4096];
	const char *base_path = Sys_DefaultBasePath();

	if (!base_path) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "Failed to get base path", error_msg_size);
		}
		return qfalse;
	}

	if (!CL_FluxFindCliPath(cli_path, sizeof(cli_path))) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "flux_cli not found in release directory", error_msg_size);
		}
		return qfalse;
	}

	if (!CL_ShellEscapeArg(prompt, prompt_escaped, sizeof(prompt_escaped))) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "Failed to escape prompt for shell", error_msg_size);
		}
		return qfalse;
	}

	Com_sprintf(model_full, sizeof(model_full), "%s/%s", base_path, model_path);
	Com_sprintf(output_full, sizeof(output_full), "%s/%s", base_path, output_path);

	Com_sprintf(cmd, sizeof(cmd),
		"cd \"%s\" && \"%s\" -d \"%s\" -p \"%s\" -o \"%s\" -W %d -H %d -s %d -S %d -q",
		base_path, cli_path, model_full, prompt_escaped, output_full,
		width, height, steps, seed);

	Com_Printf("FLUX: External generation command: %s\n", cmd);
	if (system(cmd) != 0) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "External FLUX generation failed", error_msg_size);
		}
		return qfalse;
	}

	if (!CL_FluxFileExists(output_full)) {
		if (error_msg && error_msg_size > 0) {
			Q_strncpyz(error_msg, "External FLUX did not produce output image", error_msg_size);
		}
		return qfalse;
	}

	return qtrue;
}

// Get model directory path based on selected model variant
static const char *CL_FluxGetModelPath(const char *model_variant) {
	// Model variants can have their own directories for better organization
	if (!model_variant || !*model_variant) {
		return "flux"; // Default fallback
	}

	// Map model variants to directory names
	if (Q_stricmp(model_variant, "flux1-schnell") == 0) {
		return "flux1-schnell";
	} else if (Q_stricmp(model_variant, "flux1-dev") == 0) {
		return "flux1-dev";
	} else if (Q_stricmp(model_variant, "flux2-dev") == 0) {
		// FLUX.2 models can be in "flux2-dev" or fallback to "flux"
		// Check if flux2-dev exists, otherwise use flux
		char test_path[MAX_OSPATH];
		Com_sprintf(test_path, sizeof(test_path), "%s/flux2-dev/vae/diffusion_pytorch_model.safetensors", Sys_DefaultBasePath());
		if (CL_FluxFileExists(test_path)) {
			return "flux2-dev";
		} else {
			return "flux"; // Fallback to default flux directory
		}
	} else {
		Com_Printf(S_COLOR_YELLOW "FLUX: Unknown model variant '%s', using default flux directory\n", model_variant);
		return "flux";
	}
}

typedef struct {
	flux_job_status_t status;
	int start_time;
	int timeout_seconds;
	char model_path[1024];
	char prompt[1024];
	char output_path[1024];
	int width, height, steps, seed;
	flux_image *result;
	char error_msg[1024];
	SDL_Thread *thread;
} flux_job_t;

static flux_job_t flux_job;
#endif

// common cvars for GLimp modules
cvar_t	*vid_xpos;			// X coordinate of window position
cvar_t	*vid_ypos;			// Y coordinate of window position
cvar_t	*r_noborder;

cvar_t *r_allowSoftwareGL;	// don't abort out if the pixelformat claims software
cvar_t *r_swapInterval;
cvar_t *r_glDriver;
cvar_t *r_displayRefresh;
cvar_t *r_fullscreen;
cvar_t *r_mode;
cvar_t *r_vid_driver;
cvar_t *r_modeFullscreen;
cvar_t *r_customwidth;
cvar_t *r_customheight;
cvar_t *r_customPixelAspect;

cvar_t *r_colorbits;
// these also shared with renderers:
cvar_t *cl_stencilbits;
cvar_t *cl_depthbits;
cvar_t *cl_drawBuffer;

clientActive_t		cl;
clientConnection_t	clc;
clientStatic_t		cls;
vm_t				*cgvm = NULL;

netadr_t			rcon_address;

char				cl_oldGame[ MAX_QPATH ];
qboolean			cl_oldGameSet;
static	qboolean	noGameRestart = qfalse;

#ifdef USE_CURL
download_t			download;
#endif

// Structure containing functions exported from refresh DLL
refexport_t	re;
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
static void	*rendererLib;
#endif

static ping_t cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s
{
	char string[BIG_INFO_STRING];
	netadr_t address;
	int time, startTime;
	qboolean pending;
	qboolean print;
	qboolean retrieved;
} serverStatus_t;

static serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];

static void CL_CheckForResend( void );
static void CL_ShowIP_f( void );
static void CL_ServerStatus_f( void );
#ifdef USE_FLUX
static void CL_FluxGenerate_f( void );
static void CL_FluxStatus_f( void );
static void CL_FluxCancel_f( void );
static void CL_FluxDevices_f( void );
static void CL_FluxReload_f( void );
static void CL_FluxShow_f( void );
static void CL_FluxView_f( void );
static void CL_FontsPipeline_f( void );
#endif
#ifdef USE_TRELLIS
static void CL_TrellisGenerate_f( void );
static void CL_TrellisPipeline_f( void );
static void CL_TrellisImport_f( void );
static void CL_TrellisStatus_f( void );
static void CL_TrellisCancel_f( void );
static void CL_TrellisShow_f( void );
static void CL_TrellisView_f( void );
#if defined( USE_FLUX )
static void CL_TrellisFromPrompt_f( void );
#endif
#endif
static void CL_ServerStatusResponse( const netadr_t *from, msg_t *msg );
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg );

#ifdef USE_CURL
static void CL_Download_f( void );
#endif
static void CL_LocalServers_f( void );
static void CL_GlobalServers_f( void );
static void CL_Ping_f( void );

static void CL_InitRef( void );
static void CL_ShutdownRef( refShutdownCode_t code );
static void CL_InitGLimp_Cvars( void );

/*
===============
CL_CDDialog

Called by Com_Error when a cd is needed
===============
*/
void CL_CDDialog( void ) {
	cls.cddialog = qtrue;	// start it next frame
}


/*
==================
CL_SetPlayerName_f
==================
*/
static void CL_SetPlayerName_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: %s <name>\n", Cmd_Argv( 0 ) );
		return;
	}

	Cvar_Set( "name", Cmd_ArgsFrom( 1 ) );
}

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is guaranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand( const char *cmd, qboolean isDisconnectCmd ) {
	int		index;
	int		unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

	if ( clc.serverAddress.type == NA_BAD )
		return;

	// if we would be losing an old command that hasn't been acknowledged,
	// we must drop the connection
	// also leave one slot open for the disconnect command in this case.

	if ((isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS) ||
		(!isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS))
	{
		if( com_errorEntered )
			return;
		else
			Com_Error(ERR_DROP, "Client command overflow");
	}

	clc.reliableSequence++;
	index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.reliableCommands[ index ], cmd, sizeof( clc.reliableCommands[ index ] ) );
}


//======================================================================

/*
=====================
CL_ShutdownVMs
=====================
*/
static void CL_ShutdownVMs( void )
{
	CL_ShutdownCGame();
	CL_ShutdownUI();
}


/*
=====================
Called by Com_GameRestart, CL_FlushMemory and SV_SpawnServer

CL_ShutdownAll
=====================
*/
void CL_ShutdownAll( void ) {

#ifdef USE_CURL
	CL_cURL_Shutdown();
#endif

	// clear and mute all sounds until next registration
	S_DisableSounds();

	// shutdown VMs
	CL_ShutdownVMs();

	// shutdown the renderer
	if ( re.Shutdown ) {
		if ( CL_GameSwitch() ) {
			CL_ShutdownRef( REF_DESTROY_WINDOW ); // shutdown renderer & GLimp
		} else {
			re.Shutdown( REF_KEEP_CONTEXT ); // don't destroy window or context
		}
	}

	cls.rendererStarted = qfalse;
	cls.soundRegistered = qfalse;

	SCR_Done();
}


/*
=================
CL_ClearMemory
=================
*/
void CL_ClearMemory( void ) {
	// if not running a server clear the whole hunk
	if ( !com_sv_running->integer ) {
		// clear the whole hunk
		Hunk_Clear();
		// clear collision map data
		CM_ClearMap();
	} else {
		// clear all the client data on the hunk
		Hunk_ClearToMark();
	}
}


/*
=================
CL_FlushMemory

Called by CL_Disconnect_f, CL_DownloadsComplete
Also called by Com_Error
=================
*/
void CL_FlushMemory( void ) {

	// shutdown all the client stuff
	CL_ShutdownAll();

	CL_ClearMemory();

	CL_StartHunkUsers();
}


/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
void CL_MapLoading( void ) {
	if ( com_dedicated->integer ) {
		cls.state = CA_DISCONNECTED;
		Key_SetCatcher( KEYCATCH_CONSOLE );
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	Con_Close();
	Key_SetCatcher( 0 );

	// if we are already connected to the local host, stay connected
	if ( cls.state >= CA_CONNECTED && !Q_stricmp( cls.servername, "localhost" ) ) {
		cls.state = CA_CONNECTED;		// so the connect screen is drawn
		Com_Memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		Com_Memset( clc.serverMessage, 0, sizeof( clc.serverMessage ) );
		Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
		clc.lastPacketSentTime = cls.realtime - 9999;  // send packet immediately
		cls.framecount++;
		SCR_UpdateScreen();
	} else {
		// clear nextmap so the cinematic shutdown doesn't execute it
		Cvar_Set( "nextmap", "" );
		CL_Disconnect( qtrue );
		Q_strncpyz( cls.servername, "localhost", sizeof(cls.servername) );
		cls.state = CA_CHALLENGING;		// so the connect screen is drawn
		Key_SetCatcher( 0 );
		cls.framecount++;
		SCR_UpdateScreen();
		clc.connectTime = -RETRANSMIT_TIMEOUT;
		NET_StringToAdr( cls.servername, &clc.serverAddress, NA_UNSPEC );
		// we don't need a challenge on the localhost
		CL_CheckForResend();
	}
}


/*
=====================
CL_ClearState

Called before parsing a gamestate
=====================
*/
void CL_ClearState( void ) {

//	S_StopAllSounds();

	Com_Memset( &cl, 0, sizeof( cl ) );
}


/*
====================
CL_UpdateGUID

update cl_guid using QKEY_FILE and optional prefix
====================
*/
static void CL_UpdateGUID( const char *prefix, int prefix_len )
{
#ifdef USE_Q3KEY
	fileHandle_t f;
	int len;

	len = FS_SV_FOpenFileRead( QKEY_FILE, &f );
	FS_FCloseFile( f );

	if( len != QKEY_SIZE )
		Cvar_Set( "cl_guid", "" );
	else
		Cvar_Set( "cl_guid", Com_MD5File( QKEY_FILE, QKEY_SIZE,
			prefix, prefix_len ) );
#else
	Cvar_Set( "cl_guid", Com_MD5Buf( &cl_cdkey[0], sizeof(cl_cdkey), prefix, prefix_len));
#endif
}


/*
=====================
CL_ResetOldGame
=====================
*/
void CL_ResetOldGame( void )
{
	cl_oldGameSet = qfalse;
	cl_oldGame[0] = '\0';
}


/*
=====================
CL_RestoreOldGame

change back to previous fs_game
=====================
*/
static qboolean CL_RestoreOldGame( void )
{
	if ( cl_oldGameSet )
	{
		cl_oldGameSet = qfalse;
		Cvar_Set( "fs_game", cl_oldGame );
		FS_ConditionalRestart( clc.checksumFeed, qtrue );
		return qtrue;
	}
	return qfalse;
}


/*
=====================
CL_Disconnect

Called when a connection, demo, or cinematic is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Com_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
qboolean CL_Disconnect( qboolean showMainMenu ) {
	static qboolean cl_disconnecting = qfalse;
	qboolean cl_restarted = qfalse;

	if ( !com_cl_running || !com_cl_running->integer ) {
		return cl_restarted;
	}

	if ( cl_disconnecting ) {
		return cl_restarted;
	}

	cl_disconnecting = qtrue;

	// Stop demo recording
	if ( clc.demorecording ) {
		CL_StopRecord_f();
	}

	// Stop demo playback
	if ( clc.demofile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( clc.demofile );
		clc.demofile = FS_INVALID_HANDLE;
	}

	// Finish downloads
	if ( clc.download != FS_INVALID_HANDLE ) {
		FS_FCloseFile( clc.download );
		clc.download = FS_INVALID_HANDLE;
	}
	*clc.downloadTempName = *clc.downloadName = '\0';
	Cvar_Set( "cl_downloadName", "" );

	// Stop recording any video
	if ( CL_VideoRecording() ) {
		// Finish rendering current frame
		cls.framecount++;
		SCR_UpdateScreen();
		CL_CloseAVI( qfalse );
	}

	if ( cgvm ) {
		// do that right after we rendered last video frame
		CL_ShutdownCGame();
	}

	SCR_StopCinematic();
	S_StopAllSounds();
	Key_ClearStates();

	if ( uivm && showMainMenu ) {
		VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_NONE );
		CL_JsNotifyMenuChanged( UIMENU_NONE );
	}

	// Remove pure paks
	FS_PureServerSetLoadedPaks( "", "" );
	FS_PureServerSetReferencedPaks( "", "" );

	FS_ClearPakReferences( FS_GENERAL_REF | FS_UI_REF | FS_CGAME_REF );

	if ( CL_GameSwitch() ) {
		// keep current gamestate and connection
		cl_disconnecting = qfalse;
		return qfalse;
	}

	// send a disconnect message to the server
	// send it a few times in case one is dropped
	if ( cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC && !clc.demoplaying ) {
		CL_AddReliableCommand( "disconnect", qtrue );
		CL_WritePacket( 2 );
	}

	CL_ClearState();

	// wipe the client connection
	Com_Memset( &clc, 0, sizeof( clc ) );

	cls.state = CA_DISCONNECTED;

	// allow cheats locally
	Cvar_Set( "sv_cheats", "1" );

	// not connected to a pure server anymore
	cl_connectedToPureServer = 0;

	CL_UpdateGUID( NULL, 0 );

	// Cmd_RemoveCommand( "callvote" );
	Cmd_RemoveCgameCommands();

	if ( noGameRestart )
		noGameRestart = qfalse;
	else
		cl_restarted = CL_RestoreOldGame();

	cl_disconnecting = qfalse;

	return cl_restarted;
}


/*
===================
CL_ForwardCommandToServer

adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void CL_ForwardCommandToServer( const char *string ) {
	const char *cmd;

	cmd = Cmd_Argv( 0 );

	// ignore key up commands
	if ( cmd[0] == '-' ) {
		return;
	}

	// no userinfo updates from command line
	if ( !strcmp( cmd, "userinfo" ) ) {
		return;
	}

	if ( clc.demoplaying || cls.state < CA_CONNECTED || cmd[0] == '+' ) {
		Com_Printf( "Unknown command \"%s" S_COLOR_WHITE "\"\n", cmd );
		return;
	}

	if ( Cmd_Argc() > 1 ) {
		CL_AddReliableCommand( string, qfalse );
	} else {
		CL_AddReliableCommand( cmd, qfalse );
	}
}


/*
===================
CL_RequestMotd

===================
*/
#if 0
static void CL_RequestMotd( void ) {
	char		info[MAX_INFO_STRING];

	if ( !cl_motd->integer ) {
		return;
	}
	Com_Printf( "Resolving %s\n", UPDATE_SERVER_NAME );
	if ( !NET_StringToAdr( UPDATE_SERVER_NAME, &cls.updateServer, NA_IP ) ) {
		Com_Printf( "Couldn't resolve address\n" );
		return;
	}
	cls.updateServer.port = BigShort( PORT_UPDATE );
	Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", UPDATE_SERVER_NAME,
		cls.updateServer.ip[0], cls.updateServer.ip[1],
		cls.updateServer.ip[2], cls.updateServer.ip[3],
		BigShort( cls.updateServer.port ) );

	info[0] = 0;
	// NOTE TTimo xoring against Com_Milliseconds, otherwise we may not have a true randomization
	// only srand I could catch before here is tr_noise.c l:26 srand(1001)
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=382
	// NOTE: the Com_Milliseconds xoring only affects the lower 16-bit word,
	//   but I decided it was enough randomization
	Com_sprintf( cls.updateChallenge, sizeof( cls.updateChallenge ), "%i", ((rand() << 16) ^ rand()) ^ Com_Milliseconds());

	Info_SetValueForKey( info, "challenge", cls.updateChallenge );
	Info_SetValueForKey( info, "renderer", cls.glconfig.renderer_string );
	Info_SetValueForKey( info, "version", com_version->string );

	NET_OutOfBandPrint( NS_CLIENT, &cls.updateServer, "getmotd \"%s\"\n", info );
}
#endif

/*
===================
CL_RequestAuthorization

Authorization server protocol
-----------------------------

All commands are text in Q3 out of band packets (leading 0xff 0xff 0xff 0xff).

Whenever the client tries to get a challenge from the server it wants to
connect to, it also blindly fires off a packet to the authorize server:

getKeyAuthorize <challenge> <cdkey>

cdkey may be "demo"


#OLD The authorize server returns a:
#OLD
#OLD keyAthorize <challenge> <accept | deny>
#OLD
#OLD A client will be accepted if the cdkey is valid and it has not been used by any other IP
#OLD address in the last 15 minutes.


The server sends a:

getIpAuthorize <challenge> <ip>

The authorize server returns a:

ipAuthorize <challenge> <accept | deny | demo | unknown >

A client will be accepted if a valid cdkey was sent by that ip (only) in the last 15 minutes.
If no response is received from the authorize server after two tries, the client will be let
in anyway.
===================
*/
#ifndef STANDALONE
static void CL_RequestAuthorization( void ) {
	char	nums[64];
	int		i, j, l;
	cvar_t	*fs;

	if ( !cls.authorizeServer.port ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &cls.authorizeServer, NA_IP ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}

		cls.authorizeServer.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			cls.authorizeServer.ipv._4[0], cls.authorizeServer.ipv._4[1],
			cls.authorizeServer.ipv._4[2], cls.authorizeServer.ipv._4[3],
			BigShort( cls.authorizeServer.port ) );
	}
	if ( cls.authorizeServer.type == NA_BAD ) {
		return;
	}

	// only grab the alphanumeric values from the cdkey, to avoid any dashes or spaces
	j = 0;
	l = strlen( cl_cdkey );
	if ( l > 32 ) {
		l = 32;
	}
	for ( i = 0 ; i < l ; i++ ) {
		if ( ( cl_cdkey[i] >= '0' && cl_cdkey[i] <= '9' )
				|| ( cl_cdkey[i] >= 'a' && cl_cdkey[i] <= 'z' )
				|| ( cl_cdkey[i] >= 'A' && cl_cdkey[i] <= 'Z' )
			 ) {
			nums[j] = cl_cdkey[i];
			j++;
		}
	}
	nums[j] = 0;

	fs = Cvar_Get( "cl_anonymous", "0", CVAR_INIT | CVAR_SYSTEMINFO );

	NET_OutOfBandPrint(NS_CLIENT, &cls.authorizeServer, "getKeyAuthorize %i %s", fs->integer, nums );
}
#endif


/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/

/*
==================
CL_ForwardToServer_f
==================
*/
static void CL_ForwardToServer_f( void ) {
	if ( cls.state != CA_ACTIVE || clc.demoplaying ) {
		Com_Printf ("Not connected to a server.\n");
		return;
	}

	if ( Cmd_Argc() <= 1 || strcmp( Cmd_Argv( 1 ), "userinfo" ) == 0 )
		return;

	// don't forward the first argument
	CL_AddReliableCommand( Cmd_ArgsFrom( 1 ), qfalse );
}


/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f( void ) {
	SCR_StopCinematic();
	Cvar_Set( "ui_singlePlayerActive", "0" );
	if ( cls.state != CA_DISCONNECTED && cls.state != CA_CINEMATIC ) {
		if ( (uivm && uivm->callLevel) || (cgvm && cgvm->callLevel) ) {
			Com_Error( ERR_DISCONNECT, "Disconnected from server" );
		} else {
			// clear any previous "server full" type messages
			clc.serverMessage[0] = '\0';
			if ( com_sv_running && com_sv_running->integer ) {
				// if running a local server, kill it
				SV_Shutdown( "Disconnected from server" );
			} else {
				Com_Printf( "Disconnected from %s\n", cls.servername );
			}
			Cvar_Set( "com_errorMessage", "" );
			if ( !CL_Disconnect( qfalse ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			if ( uivm ) {
				VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
				CL_JsNotifyMenuChanged( UIMENU_MAIN );
			}
		}
	}
}


/*
================
CL_Reconnect_f
================
*/
static void CL_Reconnect_f( void ) {
	if ( cl_reconnectArgs->string[0] == '\0' || Q_stricmp( cl_reconnectArgs->string, "localhost" ) == 0 )
		return;
	Cvar_Set( "ui_singlePlayerActive", "0" );
	Cbuf_AddText( va( "connect %s\n", cl_reconnectArgs->string ) );
}


/*
================
CL_Connect_f
================
*/
static void CL_Connect_f( void ) {
	netadrtype_t family;
	netadr_t	addr;
	char	buffer[ sizeof( cls.servername ) ];  // same length as cls.servername
	char	args[ sizeof( cls.servername ) + MAX_CVAR_VALUE_STRING ];
	const char	*server;
	const char	*serverString;
	int		len;
	int		argc;

	argc = Cmd_Argc();
	family = NA_UNSPEC;

	if ( argc != 2 && argc != 3 ) {
		Com_Printf( "usage: connect [-4|-6] <server>\n");
		return;
	}

	if ( argc == 2 ) {
		server = Cmd_Argv(1);
	} else {
		if( !strcmp( Cmd_Argv(1), "-4" ) )
			family = NA_IP;
#ifdef USE_IPV6
		else if( !strcmp( Cmd_Argv(1), "-6" ) )
			family = NA_IP6;
		else
			Com_Printf( S_COLOR_YELLOW "warning: only -4 or -6 as address type understood.\n" );
#else
			Com_Printf( S_COLOR_YELLOW "warning: only -4 as address type understood.\n" );
#endif
		server = Cmd_Argv(2);
	}

	Q_strncpyz( buffer, server, sizeof( buffer ) );
	server = buffer;

	// skip leading "q3a:/" in connection string
	if ( !Q_stricmpn( server, "q3a:/", 5 ) ) {
		server += 5;
	}

	// skip all slash prefixes
	while ( *server == '/' ) {
		server++;
	}

	len = strlen( server );
	if ( len <= 0 ) {
		return;
	}

	// some programs may add ending slash
	if ( buffer[len-1] == '/' ) {
		buffer[len-1] = '\0';
	}

	if ( !*server ) {
		return;
	}

	// try resolve remote server first
	if ( !NET_StringToAdr( server, &addr, family ) ) {
		Com_Printf( S_COLOR_YELLOW "Bad server address - %s\n", server );
		return;
	}

	// save arguments for reconnect
	Q_strncpyz( args, Cmd_ArgsFrom( 1 ), sizeof( args ) );

	Cvar_Set( "ui_singlePlayerActive", "0" );

	// clear any previous "server full" type messages
	clc.serverMessage[0] = '\0';

	// if running a local server, kill it
	if ( com_sv_running->integer && !strcmp( server, "localhost" ) ) {
		SV_Shutdown( "Server quit" );
	}

	// make sure a local server is killed
	Cvar_Set( "sv_killserver", "1" );
	SV_Frame( 0 );

	noGameRestart = qtrue;
	CL_Disconnect( qtrue );
	Con_Close();

	Q_strncpyz( cls.servername, server, sizeof( cls.servername ) );

	// copy resolved address
	clc.serverAddress = addr;

	if (clc.serverAddress.port == 0) {
		clc.serverAddress.port = BigShort( PORT_SERVER );
	}

	serverString = NET_AdrToStringwPort( &clc.serverAddress );

	Com_Printf( "%s resolved to %s\n", cls.servername, serverString );

	if ( cl_guidServerUniq->integer )
		CL_UpdateGUID( serverString, strlen( serverString ) );
	else
		CL_UpdateGUID( NULL, 0 );

	// if we aren't playing on a lan, we need to authenticate
	// with the cd key
	if ( NET_IsLocalAddress( &clc.serverAddress ) ) {
		cls.state = CA_CHALLENGING;
	} else {
		cls.state = CA_CONNECTING;

		// Set a client challenge number that ideally is mirrored back by the server.
		//clc.challenge = ((rand() << 16) ^ rand()) ^ Com_Milliseconds();
		Com_RandomBytes( (byte*)&clc.challenge, sizeof( clc.challenge ) );
	}

	Key_SetCatcher( 0 );
	clc.connectTime = -99999;	// CL_CheckForResend() will fire immediately
	clc.connectPacketCount = 0;

	Cvar_Set( "cl_reconnectArgs", args );

	// server connection string
	Cvar_Set( "cl_currentServerAddress", server );
}

#define MAX_RCON_MESSAGE (MAX_STRING_CHARS+4)

/*
==================
CL_CompleteRcon
==================
*/
static void CL_CompleteRcon(const char *args, int argNum )
{
	if ( argNum >= 2 )
	{
		// Skip "rcon "
		const char *p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteCommand( p, qtrue, qtrue );
	}
}


/*
=====================
CL_Rcon_f

Send the rest of the command line over as
an unconnected command.
=====================
*/
static void CL_Rcon_f( void ) {
	char message[MAX_RCON_MESSAGE];
	const char *sp;
	int len;

	if ( !rcon_client_password->string[0] ) {
		Com_Printf( "You must set 'rconpassword' before\n"
			"issuing an rcon command.\n" );
		return;
	}

	if ( cls.state >= CA_CONNECTED ) {
		rcon_address = clc.netchan.remoteAddress;
	} else {
		if ( !rconAddress->string[0] ) {
			Com_Printf( "You must either be connected,\n"
				"or set the 'rconAddress' cvar\n"
				"to issue rcon commands\n" );
			return;
		}
		if ( !NET_StringToAdr( rconAddress->string, &rcon_address, NA_UNSPEC ) ) {
			return;
		}
		if ( rcon_address.port == 0 ) {
			rcon_address.port = BigShort( PORT_SERVER );
		}
	}

	message[0] = -1;
	message[1] = -1;
	message[2] = -1;
	message[3] = -1;
	message[4] = '\0';

	// we may need to quote password if it contains spaces
	sp = strchr( rcon_client_password->string, ' ' );

	len = Com_sprintf( message+4, sizeof( message )-4,
		sp ? "rcon \"%s\" %s" : "rcon %s %s",
		rcon_client_password->string,
		Cmd_Cmd() + 5 ) + 4 + 1; // including OOB marker and '\0'

	NET_SendPacket( NS_CLIENT, len, message, &rcon_address );
}


/*
=================
CL_SendPureChecksums
=================
*/
static void CL_SendPureChecksums( void ) {
	char cMsg[ MAX_STRING_CHARS-1 ];
	int len;

	if ( !cl_connectedToPureServer || clc.demoplaying )
		return;

	// if we are pure we need to send back a command with our referenced pk3 checksums
	len = Com_sprintf( cMsg, sizeof( cMsg ), "cp %d ", cl.serverId );
	Q_strncpyz( cMsg + len, FS_ReferencedPakPureChecksums( sizeof( cMsg ) - len - 1 ), sizeof( cMsg ) - len );

	CL_AddReliableCommand( cMsg, qfalse );
}


/*
=================
CL_ResetPureClientAtServer
=================
*/
static void CL_ResetPureClientAtServer( void ) {
	CL_AddReliableCommand( "vdr", qfalse );
}


/*
=================
CL_ReloadTtf_f

Rebuild FreeType HUD/console atlases after r_fontDpi / r_fontHint / r_fontMipmap
(or r_font / r_fontSize) changes without a full client restart. Safer than relying
on renderer registration cache alone; use vid_restart if anything looks stale.
=================
*/
static void CL_ReloadTtf_f( void ) {
	if ( !re.RegisterFont ) {
		Com_Printf( S_COLOR_YELLOW "reloadTtf: renderer not loaded.\n" );
		return;
	}
	if ( re.ClearTrueTypeFontCache ) {
		re.ClearTrueTypeFontCache();
	} else {
		Com_Printf( S_COLOR_YELLOW "reloadTtf: renderer API too old (missing ClearTrueTypeFontCache); try vid_restart keep_window\n" );
	}
	CL_RegisterBuiltInTrueTypeFonts();
	Com_Printf( "reloadTtf: re-registered built-in TrueType fonts\n" );
}


/*
=================
CL_Vid_Restart

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
static void CL_Vid_Restart( refShutdownCode_t shutdownCode ) {

	// Settings may have changed so stop recording now
	if ( CL_VideoRecording() )
		CL_CloseAVI( qfalse );

	if ( clc.demorecording )
		CL_StopRecord_f();

	// clear and mute all sounds until next registration
	S_DisableSounds();

	// shutdown VMs
	CL_ShutdownVMs();

	// shutdown the renderer and clear the renderer interface
	CL_ShutdownRef( shutdownCode ); // REF_KEEP_CONTEXT, REF_KEEP_WINDOW, REF_DESTROY_WINDOW

	// client is no longer pure until new checksums are sent
	CL_ResetPureClientAtServer();

	// clear pak references
	FS_ClearPakReferences( FS_UI_REF | FS_CGAME_REF );

	// reinitialize the filesystem if the game directory or checksum has changed
	if ( !clc.demoplaying ) // -EC-
		FS_ConditionalRestart( clc.checksumFeed, qfalse );

	cls.soundRegistered = qfalse;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );

	CL_ClearMemory();

	// startup all the client stuff
	CL_StartHunkUsers();

	// start the cgame if connected
	if ( ( cls.state > CA_CONNECTED && cls.state != CA_CINEMATIC ) || cls.startCgame ) {
		cls.cgameStarted = qtrue;
		CL_InitCGame();
		// send pure checksums
		CL_SendPureChecksums();
	}

	cls.startCgame = qfalse;
}


/*
=================
CL_Vid_Restart_f

Wrapper for CL_Vid_Restart
=================
*/
static void CL_Vid_Restart_f( void ) {

	if ( Q_stricmp( Cmd_Argv( 1 ), "keep_window" ) == 0 || Q_stricmp( Cmd_Argv( 1 ), "fast" ) == 0 ) {
		// fast path: keep window
		CL_Vid_Restart( REF_KEEP_WINDOW );
	} else {
		if ( cls.lastVidRestart ) {
			if ( abs( cls.lastVidRestart - Sys_Milliseconds() ) < 500 ) {
				// hack for OSP mod: do not allow vid restart right after cgame init
				return;
			}
		}
		CL_Vid_Restart( REF_DESTROY_WINDOW );
	}
}


/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame and game must also be forced to restart because
handles will be invalid
=================
*/
static void CL_Snd_Restart_f( void )
{
	S_Shutdown();

	// sound will be reinitialized by vid_restart
	CL_Vid_Restart( REF_KEEP_CONTEXT /*REF_KEEP_WINDOW*/ );
}


/*
==================
CL_PK3List_f
==================
*/
static void CL_OpenedPK3List_f( void ) {
	Com_Printf("Opened PK3 Names: %s\n", FS_LoadedPakNames());
}


/*
==================
CL_PureList_f
==================
*/
static void CL_ReferencedPK3List_f( void ) {
	Com_Printf( "Referenced PK3 Names: %s\n", FS_ReferencedPakNames() );
}


/*
==================
CL_Configstrings_f
==================
*/
static void CL_Configstrings_f( void ) {
	int		i;
	int		ofs;

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server.\n");
		return;
	}

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		ofs = cl.gameState.stringOffsets[ i ];
		if ( !ofs ) {
			continue;
		}
		Com_Printf( "%4i: %s\n", i, cl.gameState.stringData + ofs );
	}
}


/*
==============
CL_Clientinfo_f
==============
*/
static void CL_Clientinfo_f( void ) {
	Com_Printf( "--------- Client Information ---------\n" );
	Com_Printf( "state: %i\n", cls.state );
	Com_Printf( "Server: %s\n", cls.servername );
	Com_Printf ("User info settings:\n");
	Info_Print( Cvar_InfoString( CVAR_USERINFO, NULL ) );
	Com_Printf( "--------------------------------------\n" );
}


/*
==============
CL_Serverinfo_f
==============
*/
static void CL_Serverinfo_f( void ) {
	int		ofs;

	ofs = cl.gameState.stringOffsets[ CS_SERVERINFO ];
	if ( !ofs )
		return;

	Com_Printf( "Server info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


/*
===========
CL_Systeminfo_f
===========
*/
static void CL_Systeminfo_f( void ) {
	int ofs;

	ofs = cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	if ( !ofs )
		return;

	Com_Printf( "System info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


static void CL_CompleteCallvote(const char *args, int argNum )
{
	if( argNum >= 2 )
	{
		// Skip "callvote "
		const char *p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteCommand( p, qtrue, qtrue );
	}
}


//====================================================================

/*
=================
CL_DownloadsComplete

Called when all downloading has been completed
=================
*/
static void CL_DownloadsComplete( void ) {

#ifdef USE_CURL
	// if we downloaded with cURL
	if ( clc.cURLUsed ) {
		clc.cURLUsed = qfalse;
		CL_cURL_Shutdown();
		if ( clc.cURLDisconnected ) {
			if ( clc.downloadRestart ) {
				FS_Restart( clc.checksumFeed );
				clc.downloadRestart = qfalse;
			}
			clc.cURLDisconnected = qfalse;
			CL_Reconnect_f();
			return;
		}
	}
#endif

	// if we downloaded files we need to restart the file system
	if ( clc.downloadRestart ) {
		clc.downloadRestart = qfalse;

		FS_Restart(clc.checksumFeed); // We possibly downloaded a pak, restart the file system to load it

		// inform the server so we get new gamestate info
		CL_AddReliableCommand( "donedl", qfalse );

		// by sending the donedl command we request a new gamestate
		// so we don't want to load stuff yet
		return;
	}

	// let the client game init and load data
	cls.state = CA_LOADING;

	// Pump the loop, this may change gamestate!
	Com_EventLoop();

	// if the gamestate was changed by calling Com_EventLoop
	// then we loaded everything already and we don't want to do it again.
	if ( cls.state != CA_LOADING ) {
		return;
	}

	// flush client memory and start loading stuff
	// this will also (re)load the UI
	// if this is a local client then only the client part of the hunk
	// will be cleared, note that this is done after the hunk mark has been set
	//if ( !com_sv_running->integer )
	CL_FlushMemory();

	// initialize the CGame
	cls.cgameStarted = qtrue;
	CL_InitCGame();

	if ( clc.demofile == FS_INVALID_HANDLE ) {
		Cmd_AddCommand( "callvote", NULL );
		Cmd_SetCommandCompletionFunc( "callvote", CL_CompleteCallvote );
	}

	// set pure checksums
	CL_SendPureChecksums();

	CL_WritePacket( 2 );
}


/*
=================
CL_BeginDownload

Requests a file to download from the server.  Stores it in the current
game directory.
=================
*/
static void CL_BeginDownload( const char *localName, const char *remoteName ) {

	Com_DPrintf("***** CL_BeginDownload *****\n"
				"Localname: %s\n"
				"Remotename: %s\n"
				"****************************\n", localName, remoteName);

	Q_strncpyz ( clc.downloadName, localName, sizeof(clc.downloadName) );
	Com_sprintf( clc.downloadTempName, sizeof(clc.downloadTempName), "%s.tmp", localName );

	// Set so UI gets access to it
	Cvar_Set( "cl_downloadName", remoteName );
	Cvar_Set( "cl_downloadSize", "0" );
	Cvar_Set( "cl_downloadCount", "0" );
	Cvar_SetIntegerValue( "cl_downloadTime", cls.realtime );

	clc.downloadBlock = 0; // Starting new file
	clc.downloadCount = 0;

	CL_AddReliableCommand( va("download %s", remoteName), qfalse );
}


/*
=================
CL_NextDownload

A download completed or failed
=================
*/
void CL_NextDownload( void )
{
	char *s;
	char *remoteName, *localName;
	qboolean useCURL = qfalse;

	// A download has finished, check whether this matches a referenced checksum
	if(*clc.downloadName)
	{
		const char *zippath = FS_BuildOSPath(Cvar_VariableString("fs_homepath"), clc.downloadName, NULL );

		if(!FS_CompareZipChecksum(zippath))
			Com_Error(ERR_DROP, "Incorrect checksum for file: %s", clc.downloadName);
	}

	*clc.downloadTempName = *clc.downloadName = '\0';
	Cvar_Set("cl_downloadName", "");

	// We are looking to start a download here
	if (*clc.downloadList) {
		s = clc.downloadList;

		// format is:
		//  @remotename@localname@remotename@localname, etc.

		if (*s == '@')
			s++;
		remoteName = s;

		if ( (s = strchr(s, '@')) == NULL ) {
			CL_DownloadsComplete();
			return;
		}

		*s++ = '\0';
		localName = s;
		if ( (s = strchr(s, '@')) != NULL )
			*s++ = '\0';
		else
			s = localName + strlen(localName); // point at the null byte

#ifdef USE_CURL
		if(!(cl_allowDownload->integer & DLF_NO_REDIRECT)) {
			if(clc.sv_allowDownload & DLF_NO_REDIRECT) {
				Com_Printf("WARNING: server does not "
					"allow download redirection "
					"(sv_allowDownload is %d)\n",
					clc.sv_allowDownload);
			}
			else if(!*clc.sv_dlURL) {
				Com_Printf("WARNING: server allows "
					"download redirection, but does not "
					"have sv_dlURL set\n");
			}
			else if(!CL_cURL_Init()) {
				Com_Printf("WARNING: could not load "
					"cURL library\n");
			}
			else {
				CL_cURL_BeginDownload(localName, va("%s/%s",
					clc.sv_dlURL, remoteName));
				useCURL = qtrue;
			}
		}
		else if(!(clc.sv_allowDownload & DLF_NO_REDIRECT)) {
			Com_Printf("WARNING: server allows download "
				"redirection, but it disabled by client "
				"configuration (cl_allowDownload is %d)\n",
				cl_allowDownload->integer);
		}
#endif /* USE_CURL */

		if( !useCURL ) {
		if( (cl_allowDownload->integer & DLF_NO_UDP) ) {
				Com_Error(ERR_DROP, "UDP Downloads are "
					"disabled on your client. "
					"(cl_allowDownload is %d)",
					cl_allowDownload->integer);
				return;
			}
			else {
				CL_BeginDownload( localName, remoteName );
			}
		}
		clc.downloadRestart = qtrue;

		// move over the rest
		memmove( clc.downloadList, s, strlen(s) + 1 );

		return;
	}

	CL_DownloadsComplete();
}


/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
=================
*/
void CL_InitDownloads( void ) {

	if ( !(cl_allowDownload->integer & DLF_ENABLE) )
	{
		char missingfiles[ MAXPRINTMSG ];

		// autodownload is disabled on the client
		// but it's possible that some referenced files on the server are missing
		if ( FS_ComparePaks( missingfiles, sizeof( missingfiles ), qfalse ) )
		{
			// NOTE TTimo I would rather have that printed as a modal message box
			// but at this point while joining the game we don't know whether we will successfully join or not
			Com_Printf( "\nWARNING: You are missing some files referenced by the server:\n%s"
				"You might not be able to join the game\n"
				"Go to the setting menu to turn on autodownload, or get the file elsewhere\n\n", missingfiles );
		}
	}
	else if ( FS_ComparePaks( clc.downloadList, sizeof( clc.downloadList ) , qtrue ) ) {

		Com_Printf( "Need paks: %s\n", clc.downloadList );

		if ( *clc.downloadList ) {
			// if autodownloading is not enabled on the server
			cls.state = CA_CONNECTED;

			*clc.downloadTempName = *clc.downloadName = '\0';
			Cvar_Set( "cl_downloadName", "" );

			CL_NextDownload();
			return;
		}

	}

#ifdef USE_CURL
	if ( cl_mapAutoDownload->integer && ( !(clc.sv_allowDownload & DLF_ENABLE) || clc.demoplaying ) )
	{
		const char *info, *mapname, *bsp;

		// get map name and BSP file name
		info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		mapname = Info_ValueForKey( info, "mapname" );
		bsp = va( "maps/%s.bsp", mapname );

		if ( FS_FOpenFileRead( bsp, NULL, qfalse ) == -1 )
		{
			if ( CL_Download( "dlmap", mapname, qtrue ) )
			{
				cls.state = CA_CONNECTED; // prevent continue loading and shows the ui download progress screen
				return;
			}
		}
	}
#endif // USE_CURL

	CL_DownloadsComplete();
}


/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
static void CL_CheckForResend( void ) {
	int		port, len;
	char	info[BIG_INFO_STRING]; // Cvar_InfoString(CVAR_USERINFO) can use BIG_INFO_STRING
	char	data[MAX_INFO_STRING];
	qboolean	notOverflowed;
	qboolean	infoTruncated;

	// don't send anything if playing back a demo
	if ( clc.demoplaying ) {
		return;
	}

	// resend if we haven't gotten a reply yet
	if ( cls.state != CA_CONNECTING && cls.state != CA_CHALLENGING ) {
		return;
	}

	if ( cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT ) {
		return;
	}

	clc.connectTime = cls.realtime;	// for retransmit requests
	clc.connectPacketCount++;

	switch ( cls.state ) {
	case CA_CONNECTING:
		// requesting a challenge .. IPv6 users always get in as authorize server supports no ipv6.
#ifndef STANDALONE
		if (!Cvar_VariableIntegerValue("com_standalone") && clc.serverAddress.type == NA_IP && !Sys_IsLANAddress( &clc.serverAddress ) )
			CL_RequestAuthorization();
#endif
		// The challenge request shall be followed by a client challenge so no malicious server can hijack this connection.
		NET_OutOfBandPrint( NS_CLIENT, &clc.serverAddress, "getchallenge %d %s", clc.challenge, GAMENAME_FOR_MASTER );
		break;

	case CA_CHALLENGING:
		// sending back the challenge
		port = Cvar_VariableIntegerValue( "net_qport" );

		infoTruncated = qfalse;
		Q_strncpyz( info, Cvar_InfoString( CVAR_USERINFO, &infoTruncated ), sizeof( info ) );
		if ( strlen( info ) > MAX_USERINFO_LENGTH ) {
			info[ MAX_USERINFO_LENGTH ] = '\0';
			notOverflowed = qfalse;
		}

		// remove some non-important keys that may cause overflow during connection
		if ( strlen( info ) > MAX_USERINFO_LENGTH - 64 ) {
			infoTruncated |= Info_RemoveKey( info, "xp_name" ) ? qtrue : qfalse;
			infoTruncated |= Info_RemoveKey( info, "xp_country" ) ? qtrue : qfalse;
		}

		len = strlen( info );
		if ( len > MAX_USERINFO_LENGTH ) {
			notOverflowed = qfalse;
		} else {
			notOverflowed = qtrue;
		}

		if ( com_protocol->integer != DEFAULT_PROTOCOL_VERSION ) {
			notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "protocol",
				com_protocol->string );
		} else {
			notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "protocol",
				clc.compat ? XSTRING( OLD_PROTOCOL_VERSION ) : XSTRING( NEW_PROTOCOL_VERSION ) );
		}

		notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "qport",
			va( "%i", port ) );

		notOverflowed &= Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "challenge",
			va( "%i", clc.challenge ) );

		// for now - this will be used to inform server about q3msgboom fix
		// this is optional key so will not trigger oversize warning
		Info_SetValueForKey_s( info, MAX_USERINFO_LENGTH, "client", Q3_VERSION );

		if ( !notOverflowed ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: oversize userinfo, you might be not able to join remote server!\n" );
		}

		len = Com_sprintf( data, sizeof( data ), "connect \"%s\"", info );
		// NOTE TTimo don't forget to set the right data length!
		NET_OutOfBandCompress( NS_CLIENT, &clc.serverAddress, (byte *) &data[0], len );
		// the most current userinfo has been sent, so watch for any
		// newer changes to userinfo variables
		cvar_modifiedFlags &= ~CVAR_USERINFO;

		// ... but force re-send if userinfo was truncated in any way
		if ( infoTruncated || !notOverflowed ) {
			cvar_modifiedFlags |= CVAR_USERINFO;
		}
		break;

	default:
		Com_Error( ERR_FATAL, "CL_CheckForResend: bad cls.state" );
	}
}


/*
===================
CL_MotdPacket
===================
*/
static void CL_MotdPacket( const netadr_t *from ) {
	const char *challenge;
	const char *info;

	// if not from our server, ignore it
	if ( !NET_CompareAdr( from, &cls.updateServer ) ) {
		return;
	}

	info = Cmd_Argv(1);

	// check challenge
	challenge = Info_ValueForKey( info, "challenge" );
	if ( strcmp( challenge, cls.updateChallenge ) ) {
		return;
	}

	challenge = Info_ValueForKey( info, "motd" );

	Q_strncpyz( cls.updateInfoString, info, sizeof( cls.updateInfoString ) );
	Cvar_Set( "cl_motdString", challenge );
}


/*
===================
CL_InitServerInfo
===================
*/
static void CL_InitServerInfo( serverInfo_t *server, const netadr_t *address ) {
	server->adr = *address;
	server->clients = 0;
	server->hostName[0] = '\0';
	server->mapName[0] = '\0';
	server->maxClients = 0;
	server->maxPing = 0;
	server->minPing = 0;
	server->ping = -1;
	server->game[0] = '\0';
	server->gameType = 0;
	server->netType = 0;
	server->punkbuster = 0;
	server->g_humanplayers = 0;
	server->g_needpass = 0;
}

#define MAX_SERVERSPERPACKET	256

typedef struct hash_chain_s {
	netadr_t             addr;
	struct hash_chain_s *next;
} hash_chain_t;

static hash_chain_t *hash_table[1024];
static hash_chain_t hash_list[MAX_GLOBAL_SERVERS];
static unsigned int hash_count = 0;

static unsigned int hash_func( const netadr_t *addr ) {

	const byte		*ip = NULL;
	unsigned int	size;
	unsigned int	i;
	unsigned int	hash = 0;

	switch ( addr->type ) {
		case NA_IP:  ip = addr->ipv._4; size = 4;  break;
#ifdef USE_IPV6
		case NA_IP6: ip = addr->ipv._6; size = 16; break;
#endif
		default: size = 0; break;
	}

	for ( i = 0; i < size; i++ )
		hash = hash * 101 + (int)( *ip++ );

	hash = hash ^ ( hash >> 16 );

	return (hash & 1023);
}

static void hash_insert( const netadr_t *addr )
{
	hash_chain_t **tab, *cur;
	unsigned int hash;
	if ( hash_count >= MAX_GLOBAL_SERVERS )
		return;
	hash = hash_func( addr );
	tab = &hash_table[ hash ];
	cur = &hash_list[ hash_count++ ];
	cur->addr = *addr;
	if ( cur != *tab )
		cur->next = *tab;
	else
		cur->next = NULL;
	*tab = cur;
}

static void hash_reset( void )
{
	hash_count = 0;
	memset( hash_list, 0, sizeof( hash_list ) );
	memset( hash_table, 0, sizeof( hash_table ) );
}

static hash_chain_t *hash_find( const netadr_t *addr )
{
	hash_chain_t *cur;
	cur = hash_table[ hash_func( addr ) ];
	while ( cur != NULL ) {
		if ( NET_CompareAdr( addr, &cur->addr ) )
			return cur;
		cur = cur->next;
	}
	return NULL;
}


/*
===================
CL_ServersResponsePacket
===================
*/
static void CL_ServersResponsePacket( const netadr_t* from, msg_t *msg, qboolean extended ) {
	int				i, count, total;
	netadr_t addresses[MAX_SERVERSPERPACKET];
	int				numservers;
	byte*			buffptr;
	byte*			buffend;
	serverInfo_t	*server;

	//Com_Printf("CL_ServersResponsePacket\n"); // moved down

	if (cls.numglobalservers == -1) {
		// state to detect lack of servers or lack of response
		cls.numglobalservers = 0;
		cls.numGlobalServerAddresses = 0;
		hash_reset();
	}

	// parse through server response string
	numservers = 0;
	buffptr    = msg->data;
	buffend    = buffptr + msg->cursize;

	// advance to initial token
	do
	{
		if(*buffptr == '\\' || (extended && *buffptr == '/'))
			break;

		buffptr++;
	} while (buffptr < buffend);

	while (buffptr + 1 < buffend)
	{
		// IPv4 address
		if (*buffptr == '\\')
		{
			buffptr++;

			if ( (size_t)(buffend - buffptr) < sizeof(addresses[numservers].ipv._4) + sizeof(addresses[numservers].port) + 1)
				break;

			for(i = 0; (size_t) i < sizeof(addresses[numservers].ipv._4); i++)
				addresses[numservers].ipv._4[i] = *buffptr++;

			addresses[numservers].type = NA_IP;
		}
#ifdef USE_IPV6
		// IPv6 address, if it's an extended response
		else if (extended && *buffptr == '/')
		{
			buffptr++;

			if ( (size_t)(buffend - buffptr) < sizeof(addresses[numservers].ipv._6) + sizeof(addresses[numservers].port) + 1)
				break;

			for(i = 0; (size_t) i < sizeof(addresses[numservers].ipv._6); i++)
				addresses[numservers].ipv._6[i] = *buffptr++;

			addresses[numservers].type = NA_IP6;
			addresses[numservers].scope_id = from->scope_id;
		}
#endif
		else
			// syntax error!
			break;

		// parse out port
		addresses[numservers].port = (*buffptr++) << 8;
		addresses[numservers].port += *buffptr++;
		addresses[numservers].port = BigShort( addresses[numservers].port );

		// syntax check
		if (*buffptr != '\\' && *buffptr != '/')
			break;

		numservers++;
		if (numservers >= MAX_SERVERSPERPACKET)
			break;
	}

	count = cls.numglobalservers;

	for (i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++) {

		// Tequila: It's possible to have sent many master server requests. Then
		// we may receive many times the same addresses from the master server.
		// We just avoid to add a server if it is still in the global servers list.
		if ( hash_find( &addresses[i] ) )
			continue;

		hash_insert( &addresses[i] );

		// build net address
		server = &cls.globalServers[count];

		CL_InitServerInfo( server, &addresses[i] );
		// advance to next slot
		count++;
	}

	// if getting the global list
	if ( count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS )
	{
		// if we couldn't store the servers in the main list anymore
		for (; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++)
		{
			// just store the addresses in an additional list
			cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
		}
	}

	cls.numglobalservers = count;
	total = count + cls.numGlobalServerAddresses;

	Com_Printf( "getserversResponse:%3d servers parsed (total %d)\n", numservers, total);
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc

return true only for commands indicating that our server is alive
or connection sequence is going into the right way
=================
*/
static qboolean CL_ConnectionlessPacket( const netadr_t *from, msg_t *msg ) {
	qboolean fromserver;
	const char *s;
	const char *c;
	int challenge = 0;

	MSG_BeginReadingOOB( msg );
	MSG_ReadLong( msg );	// skip the -1

	s = MSG_ReadStringLine( msg );

	Cmd_TokenizeString( s );

	c = Cmd_Argv(0);

	if ( com_developer->integer ) {
		Com_Printf( "CL packet %s: %s\n", NET_AdrToStringwPort( from ), s );
	}

	// challenge from the server we are connecting to
	if ( !Q_stricmp(c, "challengeResponse" ) ) {

		if ( cls.state != CA_CONNECTING ) {
			Com_DPrintf( "Unwanted challenge response received. Ignored.\n" );
			return qfalse;
		}

		c = Cmd_Argv( 2 );
		if ( *c != '\0' )
			challenge = atoi( c );

		clc.compat = qtrue;
		s = Cmd_Argv( 3 ); // analyze server protocol version
		if ( *s != '\0' ) {
			int sv_proto = atoi( s );
			if ( sv_proto > OLD_PROTOCOL_VERSION ) {
				if ( sv_proto == NEW_PROTOCOL_VERSION || sv_proto == com_protocol->integer ) {
					clc.compat = qfalse;
				} else {
					int cl_proto = com_protocol->integer;
					if ( cl_proto == DEFAULT_PROTOCOL_VERSION ) {
						// we support new protocol features by default
						cl_proto = NEW_PROTOCOL_VERSION;
					}
					Com_Printf( S_COLOR_YELLOW "Warning: Server reports protocol version %d, "
						"we have %d. Trying legacy protocol %d.\n",
						sv_proto, cl_proto, OLD_PROTOCOL_VERSION );
				}
			}
		}

		if ( clc.compat )
		{
			if ( !NET_CompareAdr( from, &clc.serverAddress ) )
			{
				// This challenge response is not coming from the expected address.
				// Check whether we have a matching client challenge to prevent
				// connection hi-jacking.
				if ( *c == '\0' || challenge != clc.challenge )
				{
					Com_DPrintf( "Challenge response received from unexpected source. Ignored.\n" );
					return qfalse;
				}
			}
		}
		else
		{
			if ( *c == '\0' || challenge != clc.challenge )
			{
				Com_Printf( "Bad challenge for challengeResponse. Ignored.\n" );
				return qfalse;
			}
		}

		// start sending connect instead of challenge request packets
		clc.challenge = atoi(Cmd_Argv(1));
		cls.state = CA_CHALLENGING;
		clc.connectPacketCount = 0;
		clc.connectTime = -99999;

		// take this address as the new server address.  This allows
		// a server proxy to hand off connections to multiple servers
		clc.serverAddress = *from;
		Com_DPrintf( "challengeResponse: %d\n", clc.challenge );
		return qtrue;
	}

	// server connection
	if ( !Q_stricmp(c, "connectResponse") ) {
		if ( cls.state >= CA_CONNECTED ) {
			Com_Printf( "Dup connect received. Ignored.\n" );
			return qfalse;
		}
		if ( cls.state != CA_CHALLENGING ) {
			Com_Printf( "connectResponse packet while not connecting. Ignored.\n" );
			return qfalse;
		}
		if ( !NET_CompareAdr( from, &clc.serverAddress ) ) {
			Com_Printf( "connectResponse from wrong address. Ignored.\n" );
			return qfalse;
		}

		if ( !clc.compat ) {
			// first argument: challenge response
			c = Cmd_Argv( 1 );
			if ( *c != '\0' ) {
				challenge = atoi( c );
			} else {
				Com_Printf( "Bad connectResponse received. Ignored.\n" );
				return qfalse;
			}

			if ( challenge != clc.challenge ) {
				Com_Printf( "ConnectResponse with bad challenge received. Ignored.\n" );
				return qfalse;
			}

			if ( com_protocolCompat ) {
				// enforce dm68-compatible stream for legacy/unknown servers
				clc.compat = qtrue;
			}

			// second (optional) argument: actual protocol version used on server-side
			c = Cmd_Argv( 2 );
			if ( *c != '\0' ) {
				int protocol = atoi( c );
				if ( protocol > 0 ) {
					if ( protocol <= OLD_PROTOCOL_VERSION ) {
						clc.compat = qtrue;
					} else {
						clc.compat = qfalse;
					}
				}
			}
		}

		Netchan_Setup( NS_CLIENT, &clc.netchan, from, Cvar_VariableIntegerValue( "net_qport" ), clc.challenge, clc.compat );

		cls.state = CA_CONNECTED;
		clc.lastPacketSentTime = cls.realtime - 9999; // send first packet immediately
		return qtrue;
	}

	// server responding to an info broadcast
	if ( !Q_stricmp(c, "infoResponse") ) {
		CL_ServerInfoPacket( from, msg );
		return qfalse;
	}

	// server responding to a get playerlist
	if ( !Q_stricmp(c, "statusResponse") ) {
		CL_ServerStatusResponse( from, msg );
		return qfalse;
	}

	// echo request from server
	if ( !Q_stricmp(c, "echo") ) {
		// NOTE: we may have to add exceptions for auth and update servers
		if ( (fromserver = NET_CompareAdr( from, &clc.serverAddress )) != qfalse || NET_CompareAdr( from, &rcon_address ) ) {
			NET_OutOfBandPrint( NS_CLIENT, from, "%s", Cmd_Argv(1) );
		}
		return fromserver;
	}

	// cd check
	if ( !Q_stricmp(c, "keyAuthorize") ) {
		// we don't use these now, so dump them on the floor
		return qfalse;
	}

	// global MOTD from id
	if ( !Q_stricmp(c, "motd") ) {
		CL_MotdPacket( from );
		return qfalse;
	}

	// print string from server
	if ( !Q_stricmp(c, "print") ) {
		// NOTE: we may have to add exceptions for auth and update servers
		if ( (fromserver = NET_CompareAdr( from, &clc.serverAddress )) != qfalse || NET_CompareAdr( from, &rcon_address ) ) {
			s = MSG_ReadString( msg );
			Q_strncpyz( clc.serverMessage, s, sizeof( clc.serverMessage ) );
			Com_Printf( "%s", s );
		}
		return fromserver;
	}

	// list of servers sent back by a master server (classic)
	if ( !Q_strncmp(c, "getserversResponse", 18) ) {
		CL_ServersResponsePacket( from, msg, qfalse );
		return qfalse;
	}

	// list of servers sent back by a master server (extended)
	if ( !Q_strncmp(c, "getserversExtResponse", 21) ) {
		CL_ServersResponsePacket( from, msg, qtrue );
		return qfalse;
	}

	Com_DPrintf( "Unknown connectionless packet command.\n" );
	return qfalse;
}


/*
=================
CL_PacketEvent

A packet has arrived from the main event loop
=================
*/
void CL_PacketEvent( const netadr_t *from, msg_t *msg ) {
	int		headerBytes;

	if ( msg->cursize < 5 ) {
		Com_DPrintf( "%s: Runt packet\n", NET_AdrToStringwPort( from ) );
		return;
	}

	if ( *(int *)msg->data == -1 ) {
		if ( CL_ConnectionlessPacket( from, msg ) )
			clc.lastPacketTime = cls.realtime;
		return;
	}

	if ( cls.state < CA_CONNECTED ) {
		return;		// can't be a valid sequenced packet
	}

	//
	// packet from server
	//
	if ( !NET_CompareAdr( from, &clc.netchan.remoteAddress ) ) {
		if ( com_developer->integer ) {
			Com_Printf( "%s:sequenced packet without connection\n",
				NET_AdrToStringwPort( from ) );
		}
		/* Note: Could send explicit client disconnect. */
		return;
	}

	if ( !CL_Netchan_Process( &clc.netchan, msg ) ) {
		return;		// out of order, duplicated, etc
	}

	// the header is different lengths for reliable and unreliable messages
	headerBytes = msg->readcount;

	// track the last message received so it can be returned in
	// client messages, allowing the server to detect a dropped
	// gamestate
	clc.serverMessageSequence = LittleLong( *(int32_t *)msg->data );

	clc.lastPacketTime = cls.realtime;
	CL_ParseServerMessage( msg );

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if ( clc.demorecording && !clc.demowaiting && !clc.demoplaying ) {
		CL_Demo_WriteServerPacket( msg, headerBytes );
	}
}


/*
==================
CL_CheckTimeout
==================
*/
static void CL_CheckTimeout( void ) {
	//
	// check timeout
	//
	if ( ( !CL_CheckPaused() || !sv_paused->integer )
		&& cls.state >= CA_CONNECTED && cls.state != CA_CINEMATIC
		&& cls.realtime - clc.lastPacketTime > cl_timeout->integer * 1000 ) {
		if ( ++cl.timeoutcount > 5 ) { // timeoutcount saves debugger
			Com_Printf( "\nServer connection timed out.\n" );
			Cvar_Set( "com_errorMessage", "Server connection timed out." );
			if ( !CL_Disconnect( qfalse ) ) { // restart client if not done already
				CL_FlushMemory();
			}
			if ( uivm ) {
				VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
				CL_JsNotifyMenuChanged( UIMENU_MAIN );
			}
			return;
		}
	} else {
		cl.timeoutcount = 0;
	}
}


/*
==================
CL_CheckPaused
Check whether client has been paused.
==================
*/
qboolean CL_CheckPaused( void )
{
	// if cl_paused->modified is set, the cvar has only been changed in
	// this frame. Keep paused in this frame to ensure the server doesn't
	// lag behind.
	if(cl_paused->integer || cl_paused->modified)
		return qtrue;

	return qfalse;
}


/*
==================
CL_NoDelay
==================
*/
qboolean CL_NoDelay( void )
{
	if ( CL_VideoRecording() || ( com_timedemo->integer && clc.demofile != FS_INVALID_HANDLE ) )
		return qtrue;

	return qfalse;
}


/*
==================
CL_CheckUserinfo
==================
*/
static void CL_CheckUserinfo( void ) {

	// don't add reliable commands when not yet connected
	if ( cls.state < CA_CONNECTED )
		return;

	// don't overflow the reliable command buffer when paused
	if ( CL_CheckPaused() )
		return;

	// send a reliable userinfo update if needed
	if ( cvar_modifiedFlags & CVAR_USERINFO )
	{
		qboolean infoTruncated = qfalse;
		const char *info;
		char truncated[ MAX_USERINFO_LENGTH + 1 ];

		cvar_modifiedFlags &= ~CVAR_USERINFO;

		info = Cvar_InfoString( CVAR_USERINFO, &infoTruncated );
		Q_strncpyz( truncated, info, sizeof( truncated ) );
		if ( strlen( info ) > MAX_USERINFO_LENGTH || infoTruncated ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: oversize userinfo, you might be not able to play on remote server!\n" );
		}

		CL_AddReliableCommand( va( "userinfo \"%s\"", truncated ), qfalse );
	}
}


/*
==================
CL_Steam_UpdateRichPresence
Updates Steam rich presence based on client state.
==================
*/
static void CL_Steam_UpdateRichPresence( void ) {
	if ( !Steam_IsInitialized() )
		return;
	if ( cls.state == CA_ACTIVE ) {
		if ( clc.demoplaying )
			Steam_SetRichPresence( "status", "Playing demo" );
		else if ( cls.servername[0] )
			Steam_SetRichPresence( "status", cls.servername );
		else
			Steam_SetRichPresence( "status", "In game" );
	} else if ( cls.state == CA_CONNECTING || cls.state == CA_CHALLENGING ) {
		Steam_SetRichPresence( "status", "Connecting..." );
	} else {
		Steam_SetRichPresence( "status", "In menu" );
	}
}

#if defined( USE_TRELLIS ) || defined( USE_SPEC_ENERGY )
void CL_GenerativeFrame( void );
#endif

/*
==================
CL_Frame
==================
*/
void CL_Frame( int msec, int realMsec ) {

#ifdef USE_CURL
	if ( download.cURL ) {
		Com_DL_Perform( &download );
	}
#endif

	if ( !com_cl_running->integer ) {
		return;
	}

#if defined( USE_TRELLIS ) || defined( USE_SPEC_ENERGY )
	CL_GenerativeFrame();
#endif

	// save the msec before checking pause
	cls.realFrametime = realMsec;

#ifdef USE_CURL
	if ( clc.downloadCURLM ) {
		CL_cURL_PerformDownload();
		// we can't process frames normally when in disconnected
		// download mode since the ui vm expects cls.state to be
		// CA_CONNECTED
		if ( clc.cURLDisconnected ) {
			cls.frametime = msec;
			cls.realtime += msec;
			cls.framecount++;
			SCR_UpdateScreen();
			S_Update( realMsec );
			Con_RunConsole();
			return;
		}
	}
#endif

	if ( cls.cddialog ) {
		// bring up the cd error dialog if needed
		cls.cddialog = qfalse;
		VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_NEED_CD );
		CL_JsNotifyMenuChanged( UIMENU_NEED_CD );
	} else	if ( cls.state == CA_DISCONNECTED && !( Key_GetCatcher( ) & KEYCATCH_UI )
		&& !com_sv_running->integer && uivm ) {
		// if disconnected, bring up the menu
		S_StopAllSounds();
		VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
		CL_JsNotifyMenuChanged( UIMENU_MAIN );
	}

	// if recording an avi, lock to a fixed fps
	if ( CL_VideoRecording() && msec ) {
		// save the current screen
		if ( cls.state == CA_ACTIVE || cl_forceavidemo->integer ) {
			float fps, frameDuration;

			if ( com_timescale->value > 0.0001f )
				fps = MIN( cl_aviFrameRate->value / com_timescale->value, 1000.0f );
			else
				fps = 1000.0f;

			frameDuration = MAX( 1000.0f / fps, 1.0f ) + clc.aviVideoFrameRemainder;

			CL_TakeVideoFrame();

			msec = (int)frameDuration;
			clc.aviVideoFrameRemainder = frameDuration - msec;

			realMsec = msec; // sync sound duration
		}
	}

	if ( cl_autoRecordDemo->integer && !clc.demoplaying ) {
		if ( cls.state == CA_ACTIVE && !clc.demorecording ) {
			// If not recording a demo, and we should be, start one
			qtime_t	now;
			const char	*nowString;
			char		*p;
			char		mapName[ MAX_QPATH ];
			char		serverName[ MAX_OSPATH ];

			Com_RealTime( &now );
			nowString = va( "%04d%02d%02d%02d%02d%02d",
					1900 + now.tm_year,
					1 + now.tm_mon,
					now.tm_mday,
					now.tm_hour,
					now.tm_min,
					now.tm_sec );

			Q_strncpyz( serverName, cls.servername, MAX_OSPATH );
			// Replace the ":" in the address as it is not a valid
			// file name character
			p = strchr( serverName, ':' );
			if ( p ) {
				*p = '.';
			}

			Q_strncpyz( mapName, COM_SkipPath( cl.mapname ), sizeof( cl.mapname ) );
			COM_StripExtension(mapName, mapName, sizeof(mapName));

			Cbuf_ExecuteText( EXEC_NOW,
					va( "record %s-%s-%s", nowString, serverName, mapName ) );
		}
		else if ( cls.state != CA_ACTIVE && clc.demorecording ) {
			// Recording, but not CA_ACTIVE, so stop recording
			CL_StopRecord_f();
		}
	}

	// decide the simulation time
	cls.frametime = msec;
	cls.realtime += msec;

	if ( cl_timegraph->integer ) {
		SCR_DebugGraph( msec * 0.25f );
	}

	// see if we need to update any userinfo
	CL_CheckUserinfo();

	// if we haven't gotten a packet in a long time, drop the connection
	if ( !clc.demoplaying ) {
		CL_CheckTimeout();
	}

	// send intentions now
	CL_SendCmd();

	// resend a connection request if necessary
	CL_CheckForResend();

	// decide on the serverTime to render
	CL_SetCGameTime();

	// update the screen
	cls.framecount++;
	SCR_UpdateScreen();

	// advance cinematic before audio so ROQ/video samples are mixed this frame
	SCR_RunCinematic();

	// update audio
	S_Update( realMsec );

	// tick all gameplay subsystems (physics, navigation, particles, director, music)
	CL_GameFrame( (float)msec * 0.001f );

	CL_VoIP_Frame();
	WS_Frame();
	Steam_Frame();
	CL_Steam_UpdateRichPresence();

	Con_RunConsole();
}


//============================================================================

/*
================
CL_RefPrintf
================
*/
static void FORMAT_PRINTF(2, 3) QDECL CL_RefPrintf( printParm_t level, const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	switch ( level ) {
		default: Com_Printf( "%s", msg ); break;
		case PRINT_DEVELOPER: Com_DPrintf( "%s", msg ); break;
		case PRINT_WARNING: Com_Printf( S_COLOR_WARNING "%s", msg ); break;
		case PRINT_ERROR: Com_Printf( S_COLOR_ERROR "%s", msg ); break;
	}
}


/*
============
CL_ShutdownRef
============
*/
static void CL_ShutdownRef( refShutdownCode_t code ) {

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	if ( cl_renderer->modified ) {
		code = REF_UNLOAD_DLL;
	}
#endif

	// clear and mute all sounds until next registration
	// S_DisableSounds();

	if ( code >= REF_DESTROY_WINDOW ) { // +REF_UNLOAD_DLL
		// shutdown sound system before renderer
		// because it may depend from window handle
		S_Shutdown();
	}

	SCR_Done();

	cls.builtInTtfActive = qfalse;
	Com_Memset( &cls.builtInHudFont, 0, sizeof( cls.builtInHudFont ) );
	Com_Memset( &cls.builtInConsoleFont, 0, sizeof( cls.builtInConsoleFont ) );
	cls.builtInHudRefLinePx = 0;
	cls.builtInConsoleRefLinePx = 0;

	if ( re.Shutdown ) {
		re.Shutdown( code );
	}

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	if ( rendererLib ) {
		Sys_UnloadLibrary( rendererLib );
		rendererLib = NULL;
	}
#endif

	Com_Memset( &re, 0, sizeof( re ) );

	cls.rendererStarted = qfalse;
}


/*
============
CL_InitRenderer
============
*/
static void CL_InitRenderer( void ) {

	// fixup renderer -EC-
	if ( !re.BeginRegistration ) {
		CL_InitRef();
	}

	// this sets up the renderer and calls R_Init
	re.BeginRegistration( &cls.glconfig );

	// load character sets
	cls.charSetShader = re.RegisterShader( "gfx/2d/bigchars" );
	cls.whiteShader = re.RegisterShader( "white" );
	cls.consoleShader = re.RegisterShader( "console" );

	Con_CheckResize();

	g_console_field_width = ((cls.glconfig.vidWidth / smallchar_width)) - 2;
	g_consoleField.widthInChars = g_console_field_width;

	// for 640x480 virtualized screen
	cls.biasY = 0;
	cls.biasX = 0;
	if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
		// wide screen, scale by height
		cls.scale = cls.glconfig.vidHeight * (1.0/480.0);
		cls.biasX = 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * (640.0/480.0) ) );
	} else {
		// no wide screen, scale by width
		cls.scale = cls.glconfig.vidWidth * (1.0/640.0);
		cls.biasY = 0.5 * ( cls.glconfig.vidHeight - ( cls.glconfig.vidWidth * (480.0/640) ) );
	}

	SCR_Init();

	CL_RegisterBuiltInTrueTypeFonts();
}


/*
============================
CL_StartHunkUsers

After the server has cleared the hunk, these will need to be restarted
This is the only place that any of these functions are called from
============================
*/
void CL_StartHunkUsers( void ) {

	if ( !com_cl_running || !com_cl_running->integer ) {
		return;
	}

	if ( cls.state >= CA_LOADING ) {
		// try to apply map-depending configuration from cvar cl_mapConfig_<mapname> cvars
		const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		const char *mapname = Info_ValueForKey( info, "mapname" );
		if ( mapname && *mapname != '\0' ) {
			const char *cmd = Cvar_VariableString( va( "cl_mapConfig_%s", mapname ) );
			if ( cmd && *cmd != '\0' ) {
				Cbuf_AddText( cmd );
				Cbuf_AddText( "\n" );
			} else {
				// apply mapname "default" if present
				cmd = Cvar_VariableString( va( "cl_mapConfig_%s", "default" ) );
				if ( cmd && *cmd != '\0' ) {
					Cbuf_AddText( cmd );
					Cbuf_AddText( "\n" );
				}
			}

			{
				const char *postCfg = va( "maps/%s.post.cfg", mapname );
				if ( FS_FileExists( postCfg ) ) {
					Cbuf_AddText( va( "exec %s\n", postCfg ) );
				} else if ( FS_FileExists( "maps/default.post.cfg" ) ) {
					Cbuf_AddText( "exec maps/default.post.cfg\n" );
				}
			}
		}
	}

	if ( !cls.rendererStarted ) {
		cls.rendererStarted = qtrue;
		CL_InitRenderer();
	}

	if ( !cls.soundStarted ) {
		cls.soundStarted = qtrue;
		S_Init();
	}

	if ( !cls.soundRegistered ) {
		cls.soundRegistered = qtrue;
		S_BeginRegistration();
	}

	if ( !cls.uiStarted ) {
		cls.uiStarted = qtrue;
		CL_InitUI();
	}
}


/*
============
CL_RefMalloc
============
*/
static void *CL_RefMalloc( int size ) {
	return Z_TagMalloc( size, TAG_RENDERER );
}


/*
============
CL_RefFreeAll
============
*/
static void CL_RefFreeAll( void ) {
	Z_FreeTags( TAG_RENDERER );
}


/*
============
CL_ScaledMilliseconds
============
*/
int CL_ScaledMilliseconds( void ) {
	return Sys_Milliseconds()*com_timescale->value;
}


/*
============
CL_IsMinimized
============
*/
static qboolean CL_IsMininized( void ) {
	return gw_minimized;
}

static int CL_GetState( void ) {
	return cls.state;
}


/*
============
CL_SetScaling

Sets console chars height
============
*/
static void CL_SetScaling( float factor, int captureWidth, int captureHeight ) {

	if ( cls.con_factor != factor ) {
		// rescale console
		con_scale->modified = qtrue;
	}

	cls.con_factor = factor;

	// set custom capture resolution
	cls.captureWidth = captureWidth;
	cls.captureHeight = captureHeight;
}


/*
============
CL_InitRef
============
*/
static void CL_InitRef( void ) {
	refimport_t	rimp;
	refexport_t	*ret;
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	GetRefAPI_t		getRefAPI;
	char			dllName[ MAX_OSPATH ], *ospath;
#endif

	CL_InitGLimp_Cvars();

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	cl_renderer_force = Cvar_Get( "cl_renderer_force", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_renderer_force, "When 1, skip Vulkan availability check on ARM (try Vulkan even if probe fails). Use with +set cl_renderer vulkan." );
#endif

	Com_Printf( "----- Initializing Renderer ----\n" );

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	/* "renderer" is an alias for cl_renderer (e.g. +set renderer vulkan) */
	const char *rendererName = cl_renderer->string;
	{
		const char *alt = Cvar_VariableString( "renderer" );
		if ( alt && alt[0] && isValidRenderer( alt ) ) {
			rendererName = alt;
			Cvar_Set( "cl_renderer", alt );  /* sync so config saves correctly */
		}
	}

#if defined(USE_VULKAN_API) && (defined(__arm__) || defined(__aarch64__))
	/* SDL may lack Vulkan support on ARM (e.g. RPi system SDL); fall back to OpenGL unless forced */
	if ( Q_stricmp( rendererName, "vulkan" ) == 0 && !GLimp_VulkanAvailable() )
	{
		if ( cl_renderer_force && cl_renderer_force->integer )
			Com_Printf( "[VK] cl_renderer_force 1: attempting Vulkan despite probe failure\n" );
		else
		{
			Com_Printf( "[VK] Vulkan not available in SDL, falling back to OpenGL\n" );
			Cvar_Set( "cl_renderer", "opengl" );
			Cvar_Set( "renderer", "opengl" );
			rendererName = "opengl";
		}
	}
#endif

#if defined (__linux__) && defined(__i386__)
#define REND_ARCH_STRING "x86"
#else
#define REND_ARCH_STRING ""
#endif

	{
		/* sanitize renderer name: strip surrounding single/double quotes if present */
		const char *raw = rendererName;
		char clean[64];
		size_t rawlen = strlen(raw);
		if ( rawlen >= 2 && ((raw[0] == '\"' && raw[rawlen-1] == '\"') || (raw[0] == '\'' && raw[rawlen-1] == '\'')) ) {
			size_t n = rawlen - 2;
			if ( n >= sizeof(clean) ) n = sizeof(clean) - 1;
			memcpy(clean, raw + 1, n);
			clean[n] = '\0';
		} else {
			Q_strncpyz(clean, raw, sizeof(clean));
		}
		if ( REND_ARCH_STRING[0] != '\0' ) {
			Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING DLL_EXT, clean );
		} else {
			Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s" DLL_EXT, clean );
		}
	}
	ospath = FS_BuildOSPath( Sys_DefaultBasePath(), dllName, NULL );
	Sys_ClearLoadLibraryStickyError();
	rendererLib = Sys_LoadLibrary( ospath );
	if ( !rendererLib )
	{
		Com_Printf( S_COLOR_YELLOW "Failed to load renderer from %s: %s\n", ospath, Sys_GetLoadLibraryError() );
		Cvar_ForceReset( "cl_renderer" );
		Cvar_ForceReset( "renderer" );
		/* sanitize renderer name for the retry as well */
		{
			const char *raw = cl_renderer->string;
			char clean[64];
			size_t rawlen = strlen(raw);
			if ( rawlen >= 2 && ((raw[0] == '\"' && raw[rawlen-1] == '\"') || (raw[0] == '\'' && raw[rawlen-1] == '\'')) ) {
				size_t n = rawlen - 2;
				if ( n >= sizeof(clean) ) n = sizeof(clean) - 1;
				memcpy(clean, raw + 1, n);
				clean[n] = '\0';
			} else {
				Q_strncpyz(clean, raw, sizeof(clean));
			}
			if ( REND_ARCH_STRING[0] != '\0' ) {
				Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s_" REND_ARCH_STRING DLL_EXT, clean );
			} else {
				Com_sprintf( dllName, sizeof( dllName ), RENDERER_PREFIX "_%s" DLL_EXT, clean );
			}
		}
		ospath = FS_BuildOSPath( Sys_DefaultBasePath(), dllName, NULL );
		Sys_ClearLoadLibraryStickyError();
		rendererLib = Sys_LoadLibrary( ospath );
		if ( !rendererLib )
		{
			Com_Error( ERR_FATAL, "Failed to load renderer %s: %s", dllName, Sys_GetLoadLibraryError() );
		}
	}

	{
		void *sym = Sys_LoadFunction( rendererLib, "GetRefAPI" );
		Com_Memcpy( &getRefAPI, &sym, sizeof( getRefAPI ) );
	}
	if( !getRefAPI )
	{
#ifdef _WIN32
		Com_Error( ERR_FATAL, "Can't load symbol GetRefAPI from renderer DLL (check exports / arch match): %s",
			Sys_GetLoadLibraryError() );
#else
		Com_Error( ERR_FATAL, "Can't load symbol GetRefAPI" );
#endif
		return;
	}

	cl_renderer->modified = qfalse;
#endif

	Com_Memset( &rimp, 0, sizeof( rimp ) );

	rimp.Cmd_AddCommand = Cmd_AddCommand;
	rimp.Cmd_RemoveCommand = Cmd_RemoveCommand;
	rimp.Cmd_Argc = Cmd_Argc;
	rimp.Cmd_Argv = Cmd_Argv;
	rimp.Cmd_ExecuteText = Cbuf_ExecuteText;
	rimp.Printf = CL_RefPrintf;
	rimp.Error = Com_Error;
	rimp.Milliseconds = CL_ScaledMilliseconds;
	rimp.Microseconds = Sys_Microseconds;
	rimp.Malloc = CL_RefMalloc;
	rimp.FreeAll = CL_RefFreeAll;
	rimp.Free = Z_Free;
#ifdef HUNK_DEBUG
	rimp.Hunk_AllocDebug = Hunk_AllocDebug;
#else
	rimp.Hunk_Alloc = Hunk_Alloc;
#endif
	rimp.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
	rimp.Hunk_FreeTempMemory = Hunk_FreeTempMemory;

	rimp.CM_ClusterPVS = CM_ClusterPVS;
	rimp.CM_DrawDebugSurface = CM_DrawDebugSurface;

	rimp.FS_ReadFile = FS_ReadFile;
	rimp.FS_FreeFile = FS_FreeFile;
	rimp.FS_WriteFile = FS_WriteFile;
	rimp.FS_FreeFileList = FS_FreeFileList;
	rimp.FS_ListFiles = FS_ListFiles;
	//rimp.FS_FileIsInPAK = FS_FileIsInPAK;
	rimp.FS_FileExists = FS_FileExists;

	rimp.Cvar_Get = Cvar_Get;
	rimp.Cvar_Set = Cvar_Set;
	rimp.Cvar_SetValue = Cvar_SetValue;
	rimp.Cvar_CheckRange = Cvar_CheckRange;
	rimp.Cvar_SetDescription = Cvar_SetDescription;
	rimp.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
	rimp.Cvar_VariableString = Cvar_VariableString;
	rimp.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;

	rimp.Cvar_SetGroup = Cvar_SetGroup;
	rimp.Cvar_CheckGroup = Cvar_CheckGroup;
	rimp.Cvar_ResetGroup = Cvar_ResetGroup;

	// cinematic stuff

	rimp.CIN_UploadCinematic = CIN_UploadCinematic;
	rimp.CIN_PlayCinematic = CIN_PlayCinematic;
	rimp.CIN_RunCinematic = CIN_RunCinematic;

	rimp.CL_WriteAVIVideoFrame = CL_WriteAVIVideoFrame;
	rimp.CL_SaveJPGToBuffer = CL_SaveJPGToBuffer;
	rimp.CL_SaveJPG = CL_SaveJPG;
	rimp.CL_LoadJPG = CL_LoadJPG;

	rimp.CL_IsMinimized = CL_IsMininized;
	rimp.CL_GetState = CL_GetState;
	rimp.CL_SetScaling = CL_SetScaling;

	rimp.Sys_SetClipboardBitmap = Sys_SetClipboardBitmap;
	rimp.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;
	rimp.Com_RealTime = Com_RealTime;

	rimp.GLimp_InitGamma = GLimp_InitGamma;
	rimp.GLimp_SetGamma = GLimp_SetGamma;

	/* OpenGL API: set when static OpenGL build, or when Vulkan build with dlopen (either renderer can load) */
#if defined(USE_OPENGL_API)
	rimp.GLimp_Init = GLimp_Init;
	rimp.GLimp_Shutdown = GLimp_Shutdown;
	rimp.GL_GetProcAddress = GL_GetProcAddress;
	rimp.GLimp_EndFrame = GLimp_EndFrame;
#elif defined(USE_VULKAN_API)
	/* Vulkan build: OpenGL renderer can be loaded at runtime (e.g. ARM fallback) */
	rimp.GLimp_Init = GLimp_Init;
	rimp.GLimp_Shutdown = GLimp_Shutdown;
	rimp.GL_GetProcAddress = GL_GetProcAddress;
	rimp.GLimp_EndFrame = GLimp_EndFrame;
#endif

	// Vulkan API
#ifdef USE_VULKAN_API
	rimp.VKimp_Init = VKimp_Init;
	rimp.VKimp_Shutdown = VKimp_Shutdown;
	rimp.VK_GetInstanceProcAddr = VK_GetInstanceProcAddr;
	rimp.VK_CreateSurface = VK_CreateSurface;
#endif

#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
	ret = getRefAPI( REF_API_VERSION, &rimp );
#else
	ret = GetRefAPI( REF_API_VERSION, &rimp );
#endif

	Com_Printf( "-------------------------------\n");

	if ( !ret ) {
		Com_Error (ERR_FATAL, "Couldn't initialize refresh" );
	}

	re = *ret;

	// unpause so the cgame definitely gets a snapshot and renders a frame
	Cvar_Set( "cl_paused", "0" );
}


//===========================================================================================


static void CL_SetModel_f( void ) {
	const char *arg;
	char name[ MAX_CVAR_VALUE_STRING ];

	arg = Cmd_Argv( 1 );
	if ( arg[0] ) {
		Cvar_Set( "model", arg );
		Cvar_Set( "headmodel", arg );
	} else {
		Cvar_VariableStringBuffer( "model", name, sizeof( name ) );
		Com_Printf( "model is set to %s\n", name );
	}
}

static void CL_FovAlias_f( void ) {
	if ( Cmd_Argc() > 1 ) {
		Cvar_Set( "cg_fov", Cmd_Argv( 1 ) );
		return;
	}

	Com_Printf( "cg_fov is \"%s\"\n", Cvar_VariableString( "cg_fov" ) );
}


//===========================================================================================


/*
===============
CL_Video_f

video
video [filename]
===============
*/
static void CL_Video_f( void )
{
	char filename[ MAX_OSPATH ];
	const char *ext;
	qboolean pipe;
	int i;

	if( !clc.demoplaying )
	{
		Com_Printf( "The %s command can only be used when playing back demos\n", Cmd_Argv( 0 ) );
		return;
	}

	pipe = ( Q_stricmp( Cmd_Argv( 0 ), "video-pipe" ) == 0 );

	if ( pipe )
		ext = "mp4";
	else
		ext = "avi";

	if ( Cmd_Argc() == 2 )
	{
		// explicit filename
		Com_sprintf( filename, sizeof( filename ), "videos/%s", Cmd_Argv( 1 ) );

		// override video file extension
		if ( pipe )
		{
			char *sep = strrchr( filename, '/' ); // last path separator
			char *e = strrchr( filename, '.' );

			if ( e && e > sep && *(e+1) != '\0' ) {
				ext = e + 1;
				*e = '\0';
			}
		}
	}
	else
	{
		 // scan for a free filename
		for ( i = 0; i <= 9999; i++ )
		{
			Com_sprintf( filename, sizeof( filename ), "videos/video%04d.%s", i, ext );
			if ( !FS_FileExists( filename ) )
				break; // file doesn't exist
		}

		if ( i > 9999 )
		{
			Com_Printf( S_COLOR_RED "ERROR: no free file names to create video\n" );
			return;
		}

		// without extension
		Com_sprintf( filename, sizeof( filename ), "videos/video%04d", i );
	}


	clc.aviSoundFrameRemainder = 0.0f;
	clc.aviVideoFrameRemainder = 0.0f;

	Q_strncpyz( clc.videoName, filename, sizeof( clc.videoName ) );
	clc.videoIndex = 0;

	CL_OpenAVIForWriting( va( "%s.%s", clc.videoName, ext ), pipe, qfalse );
}


/*
===============
CL_StopVideo_f
===============
*/
static void CL_StopVideo_f( void )
{
	CL_CloseAVI( qfalse );
}


/*
====================
CL_CompleteRecordName
====================
*/
static void CL_CompleteVideoName(const char *args, int argNum )
{
	(void)args;
	if ( argNum == 2 )
	{
		Field_CompleteFilename( "videos", ".avi", qtrue, FS_MATCH_EXTERN | FS_MATCH_STICK );
	}
}


/*
===============
CL_GenerateQKey

test to see if a valid QKEY_FILE exists.  If one does not, try to generate
it by filling it with 2048 bytes of random data.
===============
*/
#ifdef USE_Q3KEY
static void CL_GenerateQKey(void)
{
	int len = 0;
	unsigned char buff[ QKEY_SIZE ];
	fileHandle_t f;

	len = FS_SV_FOpenFileRead( QKEY_FILE, &f );
	FS_FCloseFile( f );
	if( len == QKEY_SIZE ) {
		Com_Printf( "QKEY found.\n" );
		return;
	}
	else {
		if( len > 0 ) {
			Com_Printf( "QKEY file size != %d, regenerating\n",
				QKEY_SIZE );
		}

		Com_Printf( "QKEY building random string\n" );
		Com_RandomBytes( buff, sizeof(buff) );

		f = FS_SV_FOpenFileWrite( QKEY_FILE );
		if( !f ) {
			Com_Printf( "QKEY could not open %s for write\n",
				QKEY_FILE );
			return;
		}
		FS_Write( buff, sizeof(buff), f );
		FS_FCloseFile( f );
		Com_Printf( "QKEY generated\n" );
	}
}
#endif


/*
** CL_GetModeInfo
*/
typedef struct vidmode_s
{
	const char	*description;
	int			width, height;
	float		pixelAspect;		// pixel width / height
} vidmode_t;

static const vidmode_t cl_vidModes[] =
{
	{ "Mode  0: 320x240",			320,	240,	1 },
	{ "Mode  1: 400x300",			400,	300,	1 },
	{ "Mode  2: 512x384",			512,	384,	1 },
	{ "Mode  3: 640x480",			640,	480,	1 },
	{ "Mode  4: 800x600",			800,	600,	1 },
	{ "Mode  5: 960x720",			960,	720,	1 },
	{ "Mode  6: 1024x768",			1024,	768,	1 },
	{ "Mode  7: 1152x864",			1152,	864,	1 },
	{ "Mode  8: 1280x1024 (5:4)",	1280,	1024,	1 },
	{ "Mode  9: 1600x1200",			1600,	1200,	1 },
	{ "Mode 10: 2048x1536",			2048,	1536,	1 },
	{ "Mode 11: 856x480 (wide)",	856,	480,	1 },
	// extra modes:
	{ "Mode 12: 1280x960",			1280,	960,	1 },
	{ "Mode 13: 1280x720",			1280,	720,	1 },
	{ "Mode 14: 1280x800 (16:10)",	1280,	800,	1 },
	{ "Mode 15: 1366x768",			1366,	768,	1 },
	{ "Mode 16: 1440x900 (16:10)",	1440,	900,	1 },
	{ "Mode 17: 1600x900",			1600,	900,	1 },
	{ "Mode 18: 1680x1050 (16:10)",	1680,	1050,	1 },
	{ "Mode 19: 1920x1080",			1920,	1080,	1 },
	{ "Mode 20: 1920x1200 (16:10)",	1920,	1200,	1 },
	{ "Mode 21: 2560x1080 (21:9)",	2560,	1080,	1 },
	{ "Mode 22: 3440x1440 (21:9)",	3440,	1440,	1 },
	{ "Mode 23: 3840x2160",			3840,	2160,	1 },
	{ "Mode 24: 4096x2160 (4K)",	4096,	2160,	1 }
};
static const int s_numVidModes = ARRAY_LEN( cl_vidModes );

qboolean CL_GetModeInfo( int *width, int *height, float *windowAspect, int mode, const char *modeFS, int dw, int dh, qboolean fullscreen )
{
	const	vidmode_t *vm;
	float	pixelAspect;

	// set dedicated fullscreen mode
	if ( fullscreen && *modeFS )
		mode = atoi( modeFS );

	if ( mode < -2 )
		return qfalse;

	if ( mode >= s_numVidModes )
		return qfalse;

	// fix unknown desktop resolution
	if ( mode == -2 && (dw == 0 || dh == 0) )
		mode = 3;

	if ( mode == -2 ) { // desktop resolution
		*width = dw;
		*height = dh;
		pixelAspect = r_customPixelAspect->value;
	} else if ( mode == -1 ) { // custom resolution
		*width = r_customwidth->integer;
		*height = r_customheight->integer;
		pixelAspect = r_customPixelAspect->value;
	} else { // predefined resolution
		vm = &cl_vidModes[ mode ];
		*width  = vm->width;
		*height = vm->height;
		pixelAspect = vm->pixelAspect;
	}

	*windowAspect = (float)*width / ( *height * pixelAspect );

	return qtrue;
}


/*
** CL_ModeList_f
*/
static void CL_ModeList_f( void )
{
	int i;

	Com_Printf( "\n" );
	for ( i = 0; i < s_numVidModes; i++ )
	{
		Com_Printf( "%s\n", cl_vidModes[ i ].description );
	}
	Com_Printf( "\n" );
}


#if defined(USE_IMGUI) && defined(USE_VULKAN_API)
/*
Toggle Vulkan ImGui debug inspector (\\r_imgui).
*/
static void CL_ToggleImgui_f( void )
{
	cvar_t *cv;
	int on;

	if ( com_dedicated && com_dedicated->integer ) {
		return;
	}

	cv = Cvar_Get( "r_imgui", "1", CVAR_ARCHIVE_ND );
	on = cv->integer ? 0 : 1;
	Cvar_SetValue( "r_imgui", (float)on );
	Com_Printf( "ImGui inspector %s (r_imgui=%d)\n", on ? "enabled" : "disabled", on );
}
#endif


#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
static qboolean isValidRenderer( const char *s ) {
	while ( *s ) {
		if ( !((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '1' && *s <= '9')) )
			return qfalse;
		++s;
	}
	return qtrue;
}
#endif


static void CL_InitGLimp_Cvars( void )
{
	// shared with GLimp
	r_allowSoftwareGL = Cvar_Get( "r_allowSoftwareGL", "0", CVAR_LATCH );
	Cvar_SetDescription( r_allowSoftwareGL, "Toggle the use of the default software OpenGL driver supplied by the Operating System." );
	r_swapInterval = Cvar_Get( "r_swapInterval", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( r_swapInterval, "V-blanks to wait before swapping buffers.\n 0: No V-Sync\n 1: Synced to the monitor's refresh rate." );
	r_glDriver = Cvar_Get( "r_glDriver", OPENGL_DRIVER_NAME, CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( r_glDriver, "Specifies the OpenGL driver to use, will revert back to default if driver name set is invalid." );

	r_displayRefresh = Cvar_Get( "r_displayRefresh", "0", CVAR_LATCH );
	Cvar_CheckRange( r_displayRefresh, "0", "500", CV_INTEGER );
	Cvar_SetDescription( r_displayRefresh, "Override monitor refresh rate in fullscreen mode:\n   0 - use current monitor refresh rate\n > 0 - use custom refresh rate" );

	vid_xpos = Cvar_Get( "vid_xpos", "3", CVAR_ARCHIVE );
	Cvar_CheckRange( vid_xpos, NULL, NULL, CV_INTEGER );
	Cvar_SetDescription( vid_xpos, "Saves/sets window X-coordinate when windowed, requires \\vid_restart." );
	vid_ypos = Cvar_Get( "vid_ypos", "22", CVAR_ARCHIVE );
	Cvar_CheckRange( vid_ypos, NULL, NULL, CV_INTEGER );
	Cvar_SetDescription( vid_ypos, "Saves/sets window Y-coordinate when windowed, requires \\vid_restart." );

	r_noborder = Cvar_Get( "r_noborder", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( r_noborder, "0", "1", CV_INTEGER );
	Cvar_SetDescription( r_noborder, "Setting to 1 will remove window borders and title bar in windowed mode, hold ALT to drag & drop it with opened console." );

#if defined(__arm__) || defined(__aarch64__)
	/* ARM/RPi: x11 default avoids Vulkan KMSDRM issues (SDL#3997) */
	r_vid_driver = Cvar_Get( "r_vid_driver", "x11", CVAR_ARCHIVE_ND | CVAR_LATCH );
#else
	r_vid_driver = Cvar_Get( "r_vid_driver", "auto", CVAR_ARCHIVE_ND | CVAR_LATCH );
#endif
	Cvar_SetDescription( r_vid_driver, "SDL video driver: auto, x11, wayland, kmsdrm. On ARM/Raspberry Pi with Vulkan, use x11 if you get 'Couldn't get a visual'. Requires vid_restart." );

#if defined(__arm__) || defined(__aarch64__)
	/* RPi5: r_mode -2 (desktop) often fails; -1 with 640x480 is more reliable */
	r_mode = Cvar_Get( "r_mode", "-1", CVAR_ARCHIVE | CVAR_LATCH );
	r_customwidth = Cvar_Get( "r_customWidth", "640", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = Cvar_Get( "r_customHeight", "480", CVAR_ARCHIVE | CVAR_LATCH );
#else
	r_mode = Cvar_Get( "r_mode", "-2", CVAR_ARCHIVE | CVAR_LATCH );
	r_customwidth = Cvar_Get( "r_customWidth", "1600", CVAR_ARCHIVE | CVAR_LATCH );
	r_customheight = Cvar_Get( "r_customHeight", "1024", CVAR_ARCHIVE | CVAR_LATCH );
#endif
	Cvar_CheckRange( r_mode, "-2", va( "%i", s_numVidModes-1 ), CV_INTEGER );
	Cvar_SetDescription( r_mode, "Set video mode:\n -2 - use current desktop resolution\n -1 - use \\r_customWidth and \\r_customHeight\n  0..N - enter \\modelist for details" );
#ifdef _DEBUG
	r_modeFullscreen = Cvar_Get( "r_modeFullscreen", "", CVAR_ARCHIVE | CVAR_LATCH );
#else
	r_modeFullscreen = Cvar_Get( "r_modeFullscreen", "-2", CVAR_ARCHIVE | CVAR_LATCH );
#endif
	Cvar_SetDescription( r_modeFullscreen, "Dedicated fullscreen mode, set to \"\" to use \\r_mode in all cases." );
	r_fullscreen = Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( r_fullscreen, "Fullscreen mode. Set to 0 for windowed mode." );
	r_customPixelAspect = Cvar_Get( "r_customPixelAspect", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( r_customPixelAspect, "Enables custom aspect of the screen, with \\r_mode -1." );
	Cvar_CheckRange( r_customwidth, "4", NULL, CV_INTEGER );
	Cvar_SetDescription( r_customwidth, "Custom width to use with \\r_mode -1." );
	Cvar_CheckRange( r_customheight, "4", NULL, CV_INTEGER );
	Cvar_SetDescription( r_customheight, "Custom height to use with \\r_mode -1." );

	r_colorbits = Cvar_Get( "r_colorbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( r_colorbits, "0", "32", CV_INTEGER );
	Cvar_SetDescription( r_colorbits, "Sets color bit depth, set to 0 to use desktop settings." );

	// shared with renderer:
	cl_stencilbits = Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( cl_stencilbits, "0", "8", CV_INTEGER );
	Cvar_SetDescription( cl_stencilbits, "Stencil buffer size, required to be 8 for stencil shadows." );
	cl_depthbits = Cvar_Get( "r_depthbits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( cl_depthbits, "0", "32", CV_INTEGER );
	Cvar_SetDescription( cl_depthbits, "Sets precision of Z-buffer." );

	cl_drawBuffer = Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	Cvar_SetDescription( cl_drawBuffer, "Specifies buffer to draw from: GL_FRONT or GL_BACK." );
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
#if defined(RENDERER_DEFAULT)
	cl_renderer = Cvar_Get( "cl_renderer", XSTRING( RENDERER_DEFAULT ), CVAR_ARCHIVE | CVAR_LATCH );
#elif defined(USE_VULKAN_API)
	cl_renderer = Cvar_Get( "cl_renderer", "vulkan", CVAR_ARCHIVE | CVAR_LATCH );
#else
	cl_renderer = Cvar_Get( "cl_renderer", "opengl", CVAR_ARCHIVE | CVAR_LATCH );
#endif
	Cvar_SetDescription( cl_renderer, "Sets your desired renderer, requires \\vid_restart." );

	if ( !isValidRenderer( cl_renderer->string ) ) {
		Cvar_ForceReset( "cl_renderer" );
	}
#endif
}


/*
====================
CL_Init
====================
*/
void CL_Init( void ) {
	const char *s;
	cvar_t *cv;

	Com_Printf( "----- Client Initialization -----\n" );

	Con_Init();

	CL_ClearState();
	cls.state = CA_DISCONNECTED;	// no longer CA_UNINITIALIZED

	CL_ResetOldGame();

	cls.realtime = 0;

	CL_InitInput();

	//
	// register client variables
	//
	cl_noprint = Cvar_Get( "cl_noprint", "0", 0 );
	Cvar_SetDescription( cl_noprint, "Disable printing of information in the console." );
	cl_motd = Cvar_Get( "cl_motd", "1", 0 );
	Cvar_SetDescription( cl_motd, "Toggle the display of the 'Message of the day'. When Quake 3 Arena starts a map up, it sends the GL_RENDERER string to the Message Of The Day server at id. This responds back with a message of the day to the client." );

	cl_timeout = Cvar_Get( "cl_timeout", "200", 0 );
	Cvar_CheckRange( cl_timeout, "5", NULL, CV_INTEGER );
	Cvar_SetDescription( cl_timeout, "Duration of receiving nothing from server for client to decide it must be disconnected (in seconds)." );

	cl_autoNudge = Cvar_Get( "cl_autoNudge", "0", CVAR_TEMP );
	Cvar_CheckRange( cl_autoNudge, "0", "1", CV_FLOAT );
	Cvar_SetDescription( cl_autoNudge, "Automatic time nudge that uses your average ping as the time nudge, values:\n  0 - use fixed \\cl_timeNudge\n (0..1] - factor of median average ping to use as timenudge\n" );
	cl_timeNudge = Cvar_Get( "cl_timeNudge", "0", CVAR_TEMP );
	Cvar_CheckRange( cl_timeNudge, "-30", "30", CV_INTEGER );
	Cvar_SetDescription( cl_timeNudge, "Allows more or less latency to be added in the interest of better smoothness or better responsiveness." );

	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_TEMP );
	Cvar_SetDescription( cl_shownet, "Toggle the display of current network status." );
	cl_showTimeDelta = Cvar_Get ("cl_showTimeDelta", "0", CVAR_TEMP );
	Cvar_SetDescription( cl_showTimeDelta, "Prints the time delta of each packet to the console (the time delta between server updates)." );
	rcon_client_password = Cvar_Get ("rconPassword", "", CVAR_TEMP );
	Cvar_SetDescription( rcon_client_password, "Sets a remote console password so clients may change server settings without direct access to the server console." );
	cl_activeAction = Cvar_Get( "activeAction", "", CVAR_TEMP );
	Cvar_SetDescription( cl_activeAction, "Contents of this variable will be executed upon first frame of play.\nNote: It is cleared every time it is executed." );

	cl_autoRecordDemo = Cvar_Get ("cl_autoRecordDemo", "0", CVAR_ARCHIVE);
	Cvar_SetDescription( cl_autoRecordDemo, "Auto-record demos when starting or joining a game." );
	cl_drawRecording = Cvar_Get("cl_drawRecording", "1", CVAR_ARCHIVE);
	Cvar_SetDescription( cl_drawRecording, "Hide (0) or shorten (1) \"RECORDING\" HUD message when recording demo." );

	cl_aviFrameRate = Cvar_Get ("cl_aviFrameRate", "25", CVAR_ARCHIVE);
	Cvar_CheckRange( cl_aviFrameRate, "1", "1000", CV_INTEGER );
	Cvar_SetDescription( cl_aviFrameRate, "The framerate used for capturing video." );
	cl_aviMotionJpeg = Cvar_Get ("cl_aviMotionJpeg", "1", CVAR_ARCHIVE);
	Cvar_SetDescription( cl_aviMotionJpeg, "Enable/disable the MJPEG codec for avi output." );
	cl_forceavidemo = Cvar_Get ("cl_forceavidemo", "0", 0);
	Cvar_SetDescription( cl_forceavidemo, "Forces all demo recording into a sequence of screenshots in TGA format." );

	cl_aviPipeFormat = Cvar_Get( "cl_aviPipeFormat",
		"-preset medium -crf 23 -c:v libx264 -flags +cgop -pix_fmt yuvj420p "
		"-bf 2 -c:a aac -strict -2 -b:a 160k -movflags faststart",
		CVAR_ARCHIVE );
	Cvar_SetDescription( cl_aviPipeFormat, "Encoder parameters used for \\video-pipe." );

	rconAddress = Cvar_Get ("rconAddress", "", 0);
	Cvar_SetDescription( rconAddress, "The IP address of the remote console you wish to connect to." );

	cl_allowDownload = Cvar_Get( "cl_allowDownload", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_allowDownload, "Enables downloading of content needed in server. Valid bitmask flags:\n 1: Downloading enabled\n 2: Do not use HTTP/FTP downloads\n 4: Do not use UDP downloads" );
#ifdef USE_CURL
	cl_mapAutoDownload = Cvar_Get( "cl_mapAutoDownload", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_mapAutoDownload, "Automatic map download for play and demo playback (via automatic \\dlmap call)." );
#ifdef USE_CURL_DLOPEN
	cl_cURLLib = Cvar_Get( "cl_cURLLib", DEFAULT_CURL_LIB, 0 );
	Cvar_SetDescription( cl_cURLLib, "Filename of cURL library to load." );
#endif
#endif

	cl_conXOffset = Cvar_Get ("cl_conXOffset", "0", 0);
	Cvar_SetDescription( cl_conXOffset, "Console notifications X-offset." );
	cl_conColor = Cvar_Get( "cl_conColor", "", 0 );
	Cvar_SetDescription( cl_conColor, "Console background color, set as R G B A values from 0-255, use with \\seta to save in config." );

#ifdef MACOS_X
	// In game video is REALLY slow in Mac OS X right now due to driver slowness
	cl_inGameVideo = Cvar_Get( "r_inGameVideo", "0", CVAR_ARCHIVE_ND );
#else
	cl_inGameVideo = Cvar_Get( "r_inGameVideo", "1", CVAR_ARCHIVE_ND );
#endif
	Cvar_SetDescription( cl_inGameVideo, "Controls whether in-game video should be drawn." );

	cl_serverStatusResendTime = Cvar_Get ("cl_serverStatusResendTime", "750", 0);
	Cvar_SetDescription( cl_serverStatusResendTime, "Time between re-sending server status requests if no response is received (in milliseconds)." );

	// init cg_autoswitch so the ui will have it correctly even
	// if the cgame hasn't been started
	Cvar_Get ("cg_autoswitch", "1", CVAR_ARCHIVE);

	cl_motdString = Cvar_Get( "cl_motdString", "", CVAR_ROM );
	Cvar_SetDescription( cl_motdString, "Message of the day string from id's master server, it is a read only variable." );

	cv = Cvar_Get( "cl_maxPing", "800", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cv, "100", "999", CV_INTEGER );
	Cvar_SetDescription( cv, "Specify the maximum allowed ping to a server." );

	cl_lanForcePackets = Cvar_Get( "cl_lanForcePackets", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_lanForcePackets, "Bypass \\cl_maxpackets for LAN games, send packets every frame." );

	cl_guidServerUniq = Cvar_Get( "cl_guidServerUniq", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_guidServerUniq, "Makes cl_guid unique for each server." );

	cl_dlURL = Cvar_Get( "cl_dlURL", "http://ws.q3df.org/maps/download/%1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_dlURL, "Cvar must point to download location." );

	cl_dlDirectory = Cvar_Get( "cl_dlDirectory", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_dlDirectory, "0", "1", CV_INTEGER );
	s = va( "Save downloads initiated by \\dlmap and \\download commands in:\n"
		" 0 - current game directory\n"
		" 1 - basegame (%s) directory\n", FS_GetBaseGameDir() );
	Cvar_SetDescription( cl_dlDirectory, s );

	cl_reconnectArgs = Cvar_Get( "cl_reconnectArgs", "", CVAR_ARCHIVE_ND | CVAR_NOTABCOMPLETE );

	// userinfo
	Cvar_Get ("name", "UnnamedPlayer", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("snaps", "40", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("model", "sarge", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("headmodel", "sarge", CVAR_USERINFO | CVAR_ARCHIVE_ND );
 	Cvar_Get ("team_model", "sarge", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("team_headmodel", "sarge", CVAR_USERINFO | CVAR_ARCHIVE_ND );
//	Cvar_Get ("g_redTeam", "Stroggs", CVAR_SERVERINFO | CVAR_ARCHIVE);
//	Cvar_Get ("g_blueTeam", "Pagans", CVAR_SERVERINFO | CVAR_ARCHIVE);
	Cvar_Get ("color1", "4", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("color2", "5", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("handicap", "100", CVAR_USERINFO | CVAR_ARCHIVE_ND );
//	Cvar_Get ("teamtask", "0", CVAR_USERINFO );
	Cvar_Get ("sex", "male", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE_ND );

	Cvar_Get ("password", "", CVAR_USERINFO | CVAR_NORESTART);
	Cvar_Get ("cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE );


	// cgame might not be initialized before menu is used
	Cvar_Get ("cg_viewsize", "100", CVAR_ARCHIVE_ND );
	// Make sure cg_stereoSeparation is zero as that variable is deprecated and should not be used anymore.
	Cvar_Get ("cg_stereoSeparation", "0", CVAR_ROM);

#ifdef USE_FLUX
	//
	// register FLUX image generation cvars
	//
	cl_flux_enable = Cvar_Get( "cl_flux_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_flux_enable, "Enable FLUX.2 image generation features. Requires model files to be present." );

	cl_flux_async = Cvar_Get( "cl_flux_async", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_async, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_flux_async, "FLUX generation mode: 0=synchronous (blocking), 1=asynchronous (background)." );

	cl_flux_external = Cvar_Get( "cl_flux_external", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_external, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_flux_external, "Use external flux_cli process to avoid in-process crashes (recommended)." );

	cl_flux_model = Cvar_Get( "cl_flux_model", "flux1-schnell", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_flux_model, "FLUX model variant: flux1-schnell (fast), flux1-dev (balanced), flux2-dev (high quality)." );

	cl_flux_device = Cvar_Get( "cl_flux_device", "auto", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_flux_device, "FLUX compute device: auto (default), cpu (force CPU), gpu (force GPU), gpu:0/1/etc (specific GPU)." );

	cl_flux_width = Cvar_Get( "cl_flux_width", "256", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_width, "64", "1792", CV_INTEGER );
	Cvar_SetDescription( cl_flux_width, "Width of generated images in pixels. Must be multiple of 16." );

	cl_flux_height = Cvar_Get( "cl_flux_height", "256", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_height, "64", "1792", CV_INTEGER );
	Cvar_SetDescription( cl_flux_height, "Height of generated images in pixels. Must be multiple of 16." );

	cl_flux_steps = Cvar_Get( "cl_flux_steps", "2", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_steps, "1", "50", CV_INTEGER );
	Cvar_SetDescription( cl_flux_steps, "Number of denoising steps for image generation. Higher = better quality but slower." );

	cl_flux_seed = Cvar_Get( "cl_flux_seed", "-1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_flux_seed, "-1", "2147483647", CV_INTEGER );
	Cvar_SetDescription( cl_flux_seed, "Random seed for reproducible image generation. -1 for random seed." );

	cl_fonts_enable = Cvar_Get( "cl_fonts_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_fonts_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_fonts_enable,
		"Enable FonTS (ICCV 2025) external Python pipeline hook. Requires separate FonTS repo + GPU env; see docs/FONTS.md." );
	cl_fonts_repo = Cvar_Get( "cl_fonts_repo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_fonts_repo, "Absolute path to a checkout of github.com/ArtmeScienceLab/FonTS (used as %R in cl_fonts_cmd)." );
	cl_fonts_python = Cvar_Get( "cl_fonts_python", "python3", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_fonts_python, "Python interpreter for fonts_pipeline (substituted as %P in cl_fonts_cmd)." );
	cl_fonts_cmd = Cvar_Get( "cl_fonts_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_fonts_cmd,
		"Shell template for fonts_pipeline: use %R FonTS repo, %B engine base path, %P python, %A args from console; %% for literal %. Blocking system() call." );
#endif

#ifdef USE_TRELLIS
	cl_trellis_enable = Cvar_Get( "cl_trellis_enable", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_enable,
		"Enable Microsoft TRELLIS.2 image-to-3D runtime generation (mirrors FLUX workflow). See docs/TRELLIS.md." );
	cl_trellis_async = Cvar_Get( "cl_trellis_async", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_async, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_async,
		"TRELLIS mode: 0=blocking console, 1=background thread (recommended for runtime use)." );
	cl_trellis_auto_import = Cvar_Get( "cl_trellis_auto_import", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_auto_import, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_auto_import,
		"When 1, register the output .glb via RegisterModel when generation completes." );
#if defined( USE_FLUX )
	cl_trellis_chain = Cvar_Get( "cl_trellis_chain", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_chain, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_chain,
		"When 1, trellis_from_prompt (or manual FLUX then chain) runs TRELLIS on FLUX output automatically." );
#endif
	cl_trellis_timeout = Cvar_Get( "cl_trellis_timeout", "3600", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_timeout, "60", "86400", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_timeout, "Warn after this many seconds if a background TRELLIS job is still running." );
	cl_trellis_repo = Cvar_Get( "cl_trellis_repo", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_repo,
		"Absolute path to a TRELLIS.2 git checkout (%%R in cl_trellis_cmd / trellis_generate)." );
	cl_trellis_python = Cvar_Get( "cl_trellis_python", "python3", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_python, "Python interpreter (%%P) when not using conda run." );
	cl_trellis_conda = Cvar_Get( "cl_trellis_conda", "trellis2", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_conda, "Conda environment name (%%N) for default trellis_generate command." );
	cl_trellis_cmd = Cvar_Get( "cl_trellis_cmd", "", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_cmd,
		"Optional shell template for trellis_pipeline / trellis_generate. Tokens: %%R repo, %%B base, %%E release dir, %%P python, %%N conda, %%I image, %%O output glb, %%M HF model, %%D decimation, %%T texture size, %%A extra args." );
	cl_trellis_hf_model = Cvar_Get( "cl_trellis_hf_model", "microsoft/TRELLIS.2-4B", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_trellis_hf_model, "Hugging Face model id passed to the TRELLIS.2 wrapper (%%M)." );
	cl_trellis_decimation = Cvar_Get( "cl_trellis_decimation", "500000", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_decimation, "1000", "10000000", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_decimation, "GLB decimation target for o_voxel export (%%D)." );
	cl_trellis_texture_size = Cvar_Get( "cl_trellis_texture_size", "2048", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_trellis_texture_size, "256", "8192", CV_INTEGER );
	Cvar_SetDescription( cl_trellis_texture_size, "GLB texture atlas size for export (%%T)." );
#endif

	//
	// register client commands
	//
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("configstrings", CL_Configstrings_f);
	Cmd_AddCommand ("clientinfo", CL_Clientinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);
	Cmd_AddCommand ("vid_restart", CL_Vid_Restart_f);
	Cmd_AddCommand ("reloadTtf", CL_ReloadTtf_f );
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);
	CL_Demo_InitCommands();
	Cmd_AddCommand ("cinematic", CL_PlayCinematic_f);
	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);
	Cmd_AddCommand ("localservers", CL_LocalServers_f);
	Cmd_AddCommand ("globalservers", CL_GlobalServers_f);
	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_SetCommandCompletionFunc( "rcon", CL_CompleteRcon );
	Cmd_AddCommand ("ping", CL_Ping_f );
	Cmd_AddCommand ("serverstatus", CL_ServerStatus_f );
	Cmd_AddCommand ("showip", CL_ShowIP_f );
	Cmd_AddCommand ("fs_openedList", CL_OpenedPK3List_f );
	Cmd_AddCommand ("fs_referencedList", CL_ReferencedPK3List_f );
	Cmd_AddCommand ("model", CL_SetModel_f );
	Cmd_AddCommand ("r_fov", CL_FovAlias_f );
	Cmd_AddCommand ("video", CL_Video_f );
	Cmd_AddCommand ("video-pipe", CL_Video_f );
	Cmd_SetCommandCompletionFunc( "video", CL_CompleteVideoName );
	Cmd_AddCommand ("stopvideo", CL_StopVideo_f );
	Cmd_AddCommand ("serverinfo", CL_Serverinfo_f );
	Cmd_AddCommand ("systeminfo", CL_Systeminfo_f );
	Cmd_AddCommand ("playername", CL_SetPlayerName_f );
	Cmd_AddCommand ("setname", CL_SetPlayerName_f );
	Cmd_AddCommand ("open", CL_Open_f );
#ifdef USE_FLUX
	Cmd_AddCommand ("flux_generate", CL_FluxGenerate_f );
	Cmd_AddCommand ("flux_status", CL_FluxStatus_f );
	Cmd_AddCommand ("flux_cancel", CL_FluxCancel_f );
	Cmd_AddCommand ("flux_devices", CL_FluxDevices_f );
	Cmd_AddCommand ("flux_reload", CL_FluxReload_f );
	Cmd_AddCommand ("flux_show", CL_FluxShow_f );
	Cmd_AddCommand ("flux_view", CL_FluxView_f );
	Cmd_AddCommand ("fonts_pipeline", CL_FontsPipeline_f );
#endif
#ifdef USE_TRELLIS
	Cmd_AddCommand( "trellis_generate", CL_TrellisGenerate_f );
	Cmd_AddCommand( "trellis_pipeline", CL_TrellisPipeline_f );
	Cmd_AddCommand( "trellis_import", CL_TrellisImport_f );
	Cmd_AddCommand( "trellis_status", CL_TrellisStatus_f );
	Cmd_AddCommand( "trellis_cancel", CL_TrellisCancel_f );
	Cmd_AddCommand( "trellis_show", CL_TrellisShow_f );
	Cmd_AddCommand( "trellis_view", CL_TrellisView_f );
#if defined( USE_FLUX )
	Cmd_AddCommand( "trellis_from_prompt", CL_TrellisFromPrompt_f );
#endif
	Com_Memset( &trellis_job, 0, sizeof( trellis_job ) );
#if defined( USE_FLUX )
	trellis_chain_armed = qfalse;
#endif
#endif

#ifdef USE_CURL
	Cmd_AddCommand( "download", CL_Download_f );
	Cmd_AddCommand( "dlmap", CL_Download_f );
#endif
	Cmd_AddCommand( "modelist", CL_ModeList_f );
#if defined(USE_IMGUI) && defined(USE_VULKAN_API)
	Cmd_AddCommand( "toggle_imgui", CL_ToggleImgui_f );
#endif

	Cvar_Set( "cl_running", "1" );
#ifdef USE_MD5
	CL_GenerateQKey();
#endif
	Cvar_Get( "cl_guid", "", CVAR_USERINFO | CVAR_ROM | CVAR_PROTECTED );
	CL_UpdateGUID( NULL, 0 );

#ifdef USE_FLUX
	// Initialize FLUX job system
	Com_Memset(&flux_job, 0, sizeof(flux_job));

	// Log FLUX initialization status
	if ( cl_flux_enable && cl_flux_enable->integer ) {
		Com_Printf( "FLUX image generation: enabled (device: %s, model: %s)\n", cl_flux_device->string, cl_flux_model->string );
		Com_Printf( "FLUX external generation: %s\n", cl_flux_external && cl_flux_external->integer ? "enabled" : "disabled" );
	} else {
		Com_Printf( "FLUX image generation: disabled (set cl_flux_enable 1 to enable)\n" );
	}
	if ( cl_fonts_enable && cl_fonts_enable->integer ) {
		Com_Printf( "FonTS pipeline: enabled (cl_fonts_repo %s)\n",
			( cl_fonts_repo && cl_fonts_repo->string[0] ) ? cl_fonts_repo->string : "unset — set cl_fonts_repo + cl_fonts_cmd" );
	} else {
		Com_Printf( "FonTS pipeline: disabled (cl_fonts_enable 0; docs/FONTS.md)\n" );
	}
#else
	Com_Printf( "FLUX.2 image generation: not available (compiled without USE_FLUX)\n" );
#endif
#ifdef USE_TRELLIS
	if ( cl_trellis_enable && cl_trellis_enable->integer ) {
		Com_Printf( "TRELLIS.2 image-to-3D: enabled (repo: %s, model: %s, async: %s, auto_import: %s)\n",
			( cl_trellis_repo && cl_trellis_repo->string[0] ) ? cl_trellis_repo->string : "unset",
			cl_trellis_hf_model ? cl_trellis_hf_model->string : "microsoft/TRELLIS.2-4B",
			( cl_trellis_async && cl_trellis_async->integer ) ? "on" : "off",
			( cl_trellis_auto_import && cl_trellis_auto_import->integer ) ? "on" : "off" );
	} else {
		Com_Printf( "TRELLIS.2 image-to-3D: disabled (cl_trellis_enable 0; docs/TRELLIS.md)\n" );
	}
#else
	Com_Printf( "TRELLIS.2 image-to-3D: not available (compiled without USE_TRELLIS)\n" );
#endif
#ifdef USE_SPEC_ENERGY
	CL_SpecEnergyInit();
#else
	Com_Printf( "Spec-energy hi-res FLUX: not available (compiled without USE_SPEC_ENERGY)\n" );
#endif

	CL_InitGameSystems();

	CL_Emoji_Init();
	CL_OSP_Init();
	CL_VoIP_Init();
	CL_Mumble_Init();
	SHUD_Init();
	WS_Init();
	Steam_Init();
	MenuVideo_Init();
	SDF_Init();

#ifdef USE_LUA
	LuaDebug_SetEngineRegisterCallback( LuaBindings_RegisterAll );
#endif

	Com_Printf( "----- Client Initialization Complete -----\n" );
}


/*
===============
CL_Shutdown

Called on fatal error, quit and dedicated mode switch
===============
*/
void CL_Shutdown( const char *finalmsg, qboolean quit ) {
	static qboolean recursive = qfalse;

	// check whether the client is running at all.
	if ( !( com_cl_running && com_cl_running->integer ) )
		return;

	Com_Printf( "----- Client Shutdown (%s) -----\n", finalmsg );

	if ( recursive ) {
		Com_Printf( "WARNING: Recursive CL_Shutdown()\n" );
		return;
	}
	recursive = qtrue;

	noGameRestart = quit;
	CL_Disconnect( qfalse );
	SDF_Shutdown();
	SHUD_Shutdown();
	MenuVideo_Shutdown();

	// clear and mute all sounds until next registration
	S_DisableSounds();

	CL_ShutdownVMs();

	CL_ShutdownRef( quit ? REF_UNLOAD_DLL : REF_DESTROY_WINDOW );

	Con_Shutdown();

#ifdef USE_FLUX
	// Clean up any running FLUX jobs
#if USE_SDL
	if (flux_job.thread) {
		SDL_WaitThread(flux_job.thread, NULL);
		flux_job.thread = NULL;
	}
#endif
	if (flux_job.result) {
		flux_image_free(flux_job.result);
		flux_job.result = NULL;
	}
	flux_job.status = FLUX_JOB_IDLE;
#endif

#ifdef USE_TRELLIS
#if USE_SDL
	if ( trellis_job.thread ) {
		SDL_WaitThread( trellis_job.thread, NULL );
		trellis_job.thread = NULL;
	}
#endif
	trellis_job.status = TRELLIS_JOB_IDLE;
#endif
#ifdef USE_SPEC_ENERGY
	CL_SpecEnergyShutdown();
#endif

	Cmd_RemoveCommand ("cmd");
	Cmd_RemoveCommand ("configstrings");
	Cmd_RemoveCommand ("userinfo");
	Cmd_RemoveCommand ("clientinfo");
	Cmd_RemoveCommand ("snd_restart");
	Cmd_RemoveCommand ("vid_restart");
	Cmd_RemoveCommand ("reloadTtf");
#ifdef USE_FLUX
	Cmd_RemoveCommand ("fonts_pipeline");
#endif
#ifdef USE_TRELLIS
	Cmd_RemoveCommand( "trellis_generate" );
	Cmd_RemoveCommand( "trellis_pipeline" );
	Cmd_RemoveCommand( "trellis_import" );
	Cmd_RemoveCommand( "trellis_status" );
	Cmd_RemoveCommand( "trellis_cancel" );
	Cmd_RemoveCommand( "trellis_show" );
	Cmd_RemoveCommand( "trellis_view" );
#if defined( USE_FLUX )
	Cmd_RemoveCommand( "trellis_from_prompt" );
#endif
#endif
	Cmd_RemoveCommand ("disconnect");
	CL_Demo_ShutdownCommands();
	Cmd_RemoveCommand ("cinematic");
	Cmd_RemoveCommand ("connect");
	Cmd_RemoveCommand ("reconnect");
	Cmd_RemoveCommand ("localservers");
	Cmd_RemoveCommand ("globalservers");
	Cmd_RemoveCommand ("rcon");
	Cmd_RemoveCommand ("ping");
	Cmd_RemoveCommand ("serverstatus");
	Cmd_RemoveCommand ("showip");
	Cmd_RemoveCommand ("fs_openedList");
	Cmd_RemoveCommand ("fs_referencedList");
	Cmd_RemoveCommand ("model");
	Cmd_RemoveCommand ("r_fov");
	Cmd_RemoveCommand ("video");
	Cmd_RemoveCommand ("stopvideo");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("modelist");
	Cmd_RemoveCommand ("open");

#ifdef USE_CURL
	Com_DL_Cleanup( &download );

	Cmd_RemoveCommand( "download" );
	Cmd_RemoveCommand( "dlmap" );
#endif

	CL_ClearInput();

	Cvar_Set( "cl_running", "0" );

	recursive = qfalse;

	Com_Memset( &cls, 0, sizeof( cls ) );
	Key_SetCatcher( 0 );
	Com_Printf( "-----------------------\n" );
}


static void CL_SetServerInfo(serverInfo_t *server, const char *info, int ping) {
	if (server) {
		if (info) {
			server->clients = atoi(Info_ValueForKey(info, "clients"));
			Q_strncpyz(server->hostName,Info_ValueForKey(info, "hostname"), MAX_NAME_LENGTH);
			Q_strncpyz(server->mapName, Info_ValueForKey(info, "mapname"), MAX_NAME_LENGTH);
			server->maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
			Q_strncpyz(server->game,Info_ValueForKey(info, "game"), MAX_NAME_LENGTH);
			server->gameType = atoi(Info_ValueForKey(info, "gametype"));
			server->netType = atoi(Info_ValueForKey(info, "nettype"));
			server->minPing = atoi(Info_ValueForKey(info, "minping"));
			server->maxPing = atoi(Info_ValueForKey(info, "maxping"));
			server->punkbuster = atoi(Info_ValueForKey(info, "punkbuster"));
			server->g_humanplayers = atoi(Info_ValueForKey(info, "g_humanplayers"));
			server->g_needpass = atoi(Info_ValueForKey(info, "g_needpass"));
		}
		server->ping = ping;
	}
}


static void CL_SetServerInfoByAddress(const netadr_t *from, const char *info, int ping) {
	int i;

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.localServers[i].adr) ) {
			CL_SetServerInfo(&cls.localServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.globalServers[i].adr)) {
			CL_SetServerInfo(&cls.globalServers[i], info, ping);
		}
	}

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		if (NET_CompareAdr(from, &cls.favoriteServers[i].adr)) {
			CL_SetServerInfo(&cls.favoriteServers[i], info, ping);
		}
	}
}


/*
===================
CL_ServerInfoPacket
===================
*/
static void CL_ServerInfoPacket( const netadr_t *from, msg_t *msg ) {
	int		i, type, len;
	char	info[MAX_INFO_STRING];
	const char *infoString;
	int		prot;

	infoString = MSG_ReadString( msg );

	// if this isn't the correct protocol version, ignore it
	prot = atoi( Info_ValueForKey( infoString, "protocol" ) );
	if ( prot != OLD_PROTOCOL_VERSION && prot != NEW_PROTOCOL_VERSION && prot != com_protocol->integer ) {
		Com_DPrintf( "Different protocol info packet: %s\n", infoString );
		return;
	}

	// iterate servers waiting for ping response
	for (i=0; i<MAX_PINGREQUESTS; i++)
	{
		if ( cl_pinglist[i].adr.port && !cl_pinglist[i].time && NET_CompareAdr( from, &cl_pinglist[i].adr ) )
		{
			// calc ping time
			cl_pinglist[i].time = Sys_Milliseconds() - cl_pinglist[i].start;
			if ( cl_pinglist[i].time < 1 )
			{
				cl_pinglist[i].time = 1;
			}
			if ( com_developer->integer )
			{
				Com_Printf( "ping time %dms from %s\n", cl_pinglist[i].time, NET_AdrToString( from ) );
			}

			// save of info
			Q_strncpyz( cl_pinglist[i].info, infoString, sizeof( cl_pinglist[i].info ) );

			// tack on the net type
			// NOTE: make sure these types are in sync with the netnames strings in the UI
			switch (from->type)
			{
				case NA_BROADCAST:
				case NA_IP:
					type = 1;
					break;
#ifdef USE_IPV6
				case NA_IP6:
					type = 2;
					break;
#endif
				default:
					type = 0;
					break;
			}

			Info_SetValueForKey( cl_pinglist[i].info, "nettype", va( "%d", type ) );
			CL_SetServerInfoByAddress( from, infoString, cl_pinglist[i].time );

			return;
		}
	}

	// if not just sent a local broadcast or pinging local servers
	if (cls.pingUpdateSource != AS_LOCAL) {
		return;
	}

	for ( i = 0 ; i < MAX_OTHER_SERVERS ; i++ ) {
		// empty slot
		if ( cls.localServers[i].adr.port == 0 ) {
			break;
		}

		// avoid duplicate
		if ( NET_CompareAdr( from, &cls.localServers[i].adr ) ) {
			return;
		}
	}

	if ( i == MAX_OTHER_SERVERS ) {
		Com_DPrintf( "MAX_OTHER_SERVERS hit, dropping infoResponse\n" );
		return;
	}

	// add this to the list
	cls.numlocalservers = i+1;
	CL_InitServerInfo( &cls.localServers[i], from );

	Q_strncpyz( info, MSG_ReadString( msg ), sizeof( info ) );
	len = (int) strlen( info );
	if ( len > 0 ) {
		if ( info[ len-1 ] == '\n' ) {
			info[ len-1 ] = '\0';
		}
		Com_Printf( "%s: %s\n", NET_AdrToStringwPort( from ), info );
	}
}


/*
===================
CL_GetServerStatus
===================
*/
static serverStatus_t *CL_GetServerStatus( const netadr_t *from ) {
	int i, oldest, oldestTime;

	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, &cl_serverStatusList[i].address ) ) {
			return &cl_serverStatusList[i];
		}
	}
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( cl_serverStatusList[i].retrieved ) {
			return &cl_serverStatusList[i];
		}
	}
	oldest = -1;
	oldestTime = 0;
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if (oldest == -1 || cl_serverStatusList[i].startTime < oldestTime) {
			oldest = i;
			oldestTime = cl_serverStatusList[i].startTime;
		}
	}
	return &cl_serverStatusList[oldest];
}


/*
===================
CL_ServerStatus
===================
*/
int CL_ServerStatus( const char *serverAddress, char *serverStatusString, int maxLen ) {
	int i;
	netadr_t	to;
	serverStatus_t *serverStatus;

	// if no server address then reset all server status requests
	if ( !serverAddress ) {
		for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
			cl_serverStatusList[i].address.port = 0;
			cl_serverStatusList[i].retrieved = qtrue;
		}
		return qfalse;
	}
	// get the address
	if ( !NET_StringToAdr( serverAddress, &to, NA_UNSPEC ) ) {
		return qfalse;
	}
	serverStatus = CL_GetServerStatus( &to );
	// if no server status string then reset the server status request for this address
	if ( !serverStatusString ) {
		serverStatus->retrieved = qtrue;
		return qfalse;
	}

	// if this server status request has the same address
	if ( NET_CompareAdr( &to, &serverStatus->address) ) {
		// if we received a response for this server status request
		if (!serverStatus->pending) {
			Q_strncpyz(serverStatusString, serverStatus->string, maxLen);
			serverStatus->retrieved = qtrue;
			serverStatus->startTime = 0;
			return qtrue;
		}
		// resend the request regularly
		else if ( Sys_Milliseconds() - serverStatus->startTime > cl_serverStatusResendTime->integer ) {
			serverStatus->print = qfalse;
			serverStatus->pending = qtrue;
			serverStatus->retrieved = qfalse;
			serverStatus->time = 0;
			serverStatus->startTime = Sys_Milliseconds();
			NET_OutOfBandPrint( NS_CLIENT, &to, "getstatus" );
			return qfalse;
		}
	}
	// if retrieved
	else if ( serverStatus->retrieved ) {
		serverStatus->address = to;
		serverStatus->print = qfalse;
		serverStatus->pending = qtrue;
		serverStatus->retrieved = qfalse;
		serverStatus->startTime = Sys_Milliseconds();
		serverStatus->time = 0;
		NET_OutOfBandPrint( NS_CLIENT, &to, "getstatus" );
		return qfalse;
	}
	return qfalse;
}


/*
===================
CL_ServerStatusResponse
===================
*/
static void CL_ServerStatusResponse( const netadr_t *from, msg_t *msg ) {
	const char	*s;
	char	info[MAX_INFO_STRING];
	char	buf[64], *v[2];
	int		i, l, score, ping;
	int		len;
	serverStatus_t *serverStatus;

	serverStatus = NULL;
	for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
		if ( NET_CompareAdr( from, &cl_serverStatusList[i].address ) ) {
			serverStatus = &cl_serverStatusList[i];
			break;
		}
	}
	// if we didn't request this server status
	if (!serverStatus) {
		return;
	}

	s = MSG_ReadStringLine( msg );

	len = 0;
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "%s", s);

	if (serverStatus->print) {
		Com_Printf("Server settings:\n");
		// print cvars
		while (*s) {
			for (i = 0; i < 2 && *s; i++) {
				if (*s == '\\')
					s++;
				l = 0;
				while (*s) {
					info[l++] = *s;
					if (l >= MAX_INFO_STRING-1)
						break;
					s++;
					if (*s == '\\') {
						break;
					}
				}
				info[l] = '\0';
				if (i) {
					Com_Printf("%s\n", info);
				}
				else {
					Com_Printf("%-24s", info);
				}
			}
		}
	}

	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	if (serverStatus->print) {
		Com_Printf("\nPlayers:\n");
		Com_Printf("num: score: ping: name:\n");
	}
	for (i = 0, s = MSG_ReadStringLine( msg ); *s; s = MSG_ReadStringLine( msg ), i++) {

		len = strlen(serverStatus->string);
		Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\%s", s);

		if (serverStatus->print) {
			//score = ping = 0;
			//sscanf(s, "%d %d", &score, &ping);
			Q_strncpyz( buf, s, sizeof (buf) );
			Com_Split( buf, v, 2, ' ' );
			score = atoi( v[0] );
			ping = atoi( v[1] );
			s = strchr(s, ' ');
			if (s)
				s = strchr(s+1, ' ');
			if (s)
				s++;
			else
				s = "unknown";
			Com_Printf("%-2d   %-3d    %-3d   %s\n", i, score, ping, s );
		}
	}
	len = strlen(serverStatus->string);
	Com_sprintf(&serverStatus->string[len], sizeof(serverStatus->string)-len, "\\");

	serverStatus->time = Sys_Milliseconds();
	serverStatus->address = *from;
	serverStatus->pending = qfalse;
	if (serverStatus->print) {
		serverStatus->retrieved = qtrue;
	}
}


/*
==================
CL_LocalServers_f
==================
*/
static void CL_LocalServers_f( void ) {
	char		*message;
	int			i, j, n;
	netadr_t	to;

	Com_Printf( "Scanning for servers on the local network...\n");

	// reset the list, waiting for response
	cls.numlocalservers = 0;
	cls.pingUpdateSource = AS_LOCAL;

	for (i = 0; i < MAX_OTHER_SERVERS; i++) {
		qboolean b = cls.localServers[i].visible;
		Com_Memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
		cls.localServers[i].visible = b;
	}
	Com_Memset( &to, 0, sizeof( to ) );

	// The 'xxx' in the message is a challenge that will be echoed back
	// by the server.  We don't care about that here, but master servers
	// can use that to prevent spoofed server responses from invalid ip
	message = "\377\377\377\377getinfo xxx";
	n = (int)strlen( message );

	// send each message twice in case one is dropped
	for ( i = 0 ; i < 2 ; i++ ) {
		// send a broadcast packet on each server port
		// we support multiple server ports so a single machine
		// can nicely run multiple servers
		for ( j = 0 ; j < NUM_SERVER_PORTS ; j++ ) {
			to.port = BigShort( (short)(PORT_SERVER + j) );

			to.type = NA_BROADCAST;
			NET_SendPacket( NS_CLIENT, n, message, &to );
#ifdef USE_IPV6
			to.type = NA_MULTICAST6;
			NET_SendPacket( NS_CLIENT, n, message, &to );
#endif
		}
	}
}


/*
==================
CL_GlobalServers_f

Originally master 0 was Internet and master 1 was MPlayer.
ioquake3 2008; added support for requesting five separate master servers using 0-4.
ioquake3 2017; made master 0 fetch all master servers and 1-5 request a single master server.
==================
*/
static void CL_GlobalServers_f( void ) {
	netadr_t	to;
	int			count, i, masterNum;
	char		command[1024];
	const char	*masteraddress;

	if ( (count = Cmd_Argc()) < 3 || (masterNum = atoi(Cmd_Argv(1))) < 0 || masterNum > MAX_MASTER_SERVERS )
	{
		Com_Printf( "usage: globalservers <master# 0-%d> <protocol> [keywords]\n", MAX_MASTER_SERVERS );
		return;
	}

	// request from all master servers
	if ( masterNum == 0 ) {
		int numAddress = 0;

		for ( i = 1; i <= MAX_MASTER_SERVERS; i++ ) {
			Com_sprintf( command, sizeof( command ), "sv_master%d", i );
			masteraddress = Cvar_VariableString( command );

			if ( !*masteraddress )
				continue;

			numAddress++;

			Com_sprintf( command, sizeof( command ), "globalservers %d %s %s\n", i, Cmd_Argv( 2 ), Cmd_ArgsFrom( 3 ) );
			Cbuf_AddText( command );
		}

		if ( !numAddress ) {
			Com_Printf( "CL_GlobalServers_f: Error: No master server addresses.\n");
		}
		return;
	}

	Com_sprintf( command, sizeof( command ), "sv_master%d", masterNum );
	masteraddress = Cvar_VariableString( command );

	if ( !*masteraddress )
	{
		Com_Printf( "CL_GlobalServers_f: Error: No master server address given.\n");
		return;
	}

	// reset the list, waiting for response
	// -1 is used to distinguish a "no response"

	i = NET_StringToAdr( masteraddress, &to, NA_UNSPEC );

	if ( i == 0 )
	{
		Com_Printf( "CL_GlobalServers_f: Error: could not resolve address of master %s\n", masteraddress );
		return;
	}
	else if ( i == 2 )
		to.port = BigShort( PORT_MASTER );

	Com_Printf( "Requesting servers from %s (%s)...\n", masteraddress, NET_AdrToStringwPort( &to ) );

	cls.numglobalservers = -1;
	cls.pingUpdateSource = AS_GLOBAL;

	// Use the extended query for IPv6 masters
#ifdef USE_IPV6
	if ( to.type == NA_IP6 || to.type == NA_MULTICAST6 )
	{
		int v4enabled = Cvar_VariableIntegerValue( "net_enabled" ) & NET_ENABLEV4;

		if ( v4enabled )
		{
			Com_sprintf( command, sizeof( command ), "getserversExt %s %s",
				GAMENAME_FOR_MASTER, Cmd_Argv(2) );
		}
		else
		{
			Com_sprintf( command, sizeof( command ), "getserversExt %s %s ipv6",
				GAMENAME_FOR_MASTER, Cmd_Argv(2) );
		}
	}
	else
#endif
		Com_sprintf( command, sizeof( command ), "getservers %s", Cmd_Argv(2) );

	for ( i = 3; i < count; i++ )
	{
		Q_strcat( command, sizeof( command ), " " );
		Q_strcat( command, sizeof( command ), Cmd_Argv( i ) );
	}

	NET_OutOfBandPrint( NS_SERVER, &to, "%s", command );
}


/*
==================
CL_GetPing
==================
*/
void CL_GetPing( int n, char *buf, int buflen, int *pingtime )
{
	const char	*str;
	int		time;
	int		maxPing;

	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		// empty or invalid slot
		buf[0]    = '\0';
		*pingtime = 0;
		return;
	}

	str = NET_AdrToStringwPort( &cl_pinglist[n].adr );
	Q_strncpyz( buf, str, buflen );

	time = cl_pinglist[n].time;
	if ( time == 0 )
	{
		// check for timeout
		time = Sys_Milliseconds() - cl_pinglist[n].start;
		maxPing = Cvar_VariableIntegerValue( "cl_maxPing" );
		if ( time < maxPing )
		{
			// not timed out yet
			time = 0;
		}
	}

	CL_SetServerInfoByAddress(&cl_pinglist[n].adr, cl_pinglist[n].info, cl_pinglist[n].time);

	*pingtime = time;
}


/*
==================
CL_GetPingInfo
==================
*/
void CL_GetPingInfo( int n, char *buf, int buflen )
{
	if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port)
	{
		// empty or invalid slot
		if (buflen)
			buf[0] = '\0';
		return;
	}

	Q_strncpyz( buf, cl_pinglist[n].info, buflen );
}


/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing( int n )
{
	if (n < 0 || n >= MAX_PINGREQUESTS)
		return;

	cl_pinglist[n].adr.port = 0;
}


/*
==================
CL_GetPingQueueCount
==================
*/
int CL_GetPingQueueCount( void )
{
	int		i;
	int		count;
	ping_t*	pingptr;

	count   = 0;
	pingptr = cl_pinglist;

	for (i=0; i<MAX_PINGREQUESTS; i++, pingptr++ ) {
		if (pingptr->adr.port) {
			count++;
		}
	}

	return (count);
}


/*
==================
CL_GetFreePing
==================
*/
static ping_t* CL_GetFreePing( void )
{
	ping_t* pingptr;
	ping_t* best;
	int		oldest;
	int		i;
	int		time, msec;

	msec = Sys_Milliseconds();
	pingptr = cl_pinglist;
	for ( i = 0; (size_t) i < ARRAY_LEN( cl_pinglist ); i++, pingptr++ )
	{
		// find free ping slot
		if ( pingptr->adr.port )
		{
			if ( pingptr->time == 0 )
			{
				if ( msec - pingptr->start < 500 )
				{
					// still waiting for response
					continue;
				}
			}
			else if ( pingptr->time < 500 )
			{
				// results have not been queried
				continue;
			}
		}

		// clear it
		pingptr->adr.port = 0;
		return pingptr;
	}

	// use oldest entry
	pingptr = cl_pinglist;
	best    = cl_pinglist;
	oldest  = INT_MIN;
	for ( i = 0; (size_t) i < ARRAY_LEN( cl_pinglist ); i++, pingptr++ )
	{
		// scan for oldest
		time = msec - pingptr->start;
		if ( time > oldest )
		{
			oldest = time;
			best   = pingptr;
		}
	}

	return best;
}


/*
==================
CL_Ping_f
==================
*/
static void CL_Ping_f( void ) {
	netadr_t	to;
	ping_t*		pingptr;
	const char*		server;
	int			argc;
	netadrtype_t	family = NA_UNSPEC;

	argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 ) {
		Com_Printf( "usage: ping [-4|-6] <server>\n");
		return;
	}

	if ( argc == 2 )
		server = Cmd_Argv(1);
	else
	{
		if( !strcmp( Cmd_Argv(1), "-4" ) )
			family = NA_IP;
#ifdef USE_IPV6
		else if( !strcmp( Cmd_Argv(1), "-6" ) )
			family = NA_IP6;
		else
			Com_Printf( "warning: only -4 or -6 as address type understood.\n" );
#else
		else
			Com_Printf( "warning: only -4 as address type understood.\n" );
#endif

		server = Cmd_Argv(2);
	}

	Com_Memset( &to, 0, sizeof( to ) );

	if ( !NET_StringToAdr( server, &to, family ) ) {
		return;
	}

	pingptr = CL_GetFreePing();

	memcpy( &pingptr->adr, &to, sizeof (netadr_t) );
	pingptr->start = Sys_Milliseconds();
	pingptr->time  = 0;

	CL_SetServerInfoByAddress( &pingptr->adr, NULL, 0 );

	NET_OutOfBandPrint( NS_CLIENT, &to, "getinfo xxx" );
}


/*
==================
CL_UpdateVisiblePings_f
==================
*/
qboolean CL_UpdateVisiblePings_f(int source) {
	int			slots, i;
	char		buff[MAX_STRING_CHARS];
	int			pingTime;
	int			max;
	qboolean status = qfalse;

	if (source < 0 || source > AS_FAVORITES) {
		return qfalse;
	}

	cls.pingUpdateSource = source;

	slots = CL_GetPingQueueCount();
	if (slots < MAX_PINGREQUESTS) {
		serverInfo_t *server = NULL;

		switch (source) {
			case AS_LOCAL :
				server = &cls.localServers[0];
				max = cls.numlocalservers;
			break;
			case AS_GLOBAL :
				server = &cls.globalServers[0];
				max = cls.numglobalservers;
			break;
			case AS_FAVORITES :
				server = &cls.favoriteServers[0];
				max = cls.numfavoriteservers;
			break;
			default:
				return qfalse;
		}
		for (i = 0; i < max; i++) {
			if (server[i].visible) {
				if (server[i].ping == -1) {
					int j;

					if (slots >= MAX_PINGREQUESTS) {
						break;
					}
					for (j = 0; j < MAX_PINGREQUESTS; j++) {
						if (!cl_pinglist[j].adr.port) {
							continue;
						}
						if (NET_CompareAdr( &cl_pinglist[j].adr, &server[i].adr)) {
							// already on the list
							break;
						}
					}
					if (j >= MAX_PINGREQUESTS) {
						status = qtrue;
						for (j = 0; j < MAX_PINGREQUESTS; j++) {
							if (!cl_pinglist[j].adr.port) {
								memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
								cl_pinglist[j].start = Sys_Milliseconds();
								cl_pinglist[j].time = 0;
								NET_OutOfBandPrint(NS_CLIENT, &cl_pinglist[j].adr, "getinfo xxx");
								slots++;
								break;
							}
						}
					}
				}
				// if the server has a ping higher than cl_maxPing or
				// the ping packet got lost
				else if (server[i].ping == 0) {
					// if we are updating global servers
					if (source == AS_GLOBAL) {
						//
						if ( cls.numGlobalServerAddresses > 0 ) {
							// overwrite this server with one from the additional global servers
							cls.numGlobalServerAddresses--;
							CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
							// NOTE: the server[i].visible flag stays untouched
						}
					}
				}
			}
		}
	}

	if (slots) {
		status = qtrue;
	}
	for (i = 0; i < MAX_PINGREQUESTS; i++) {
		if (!cl_pinglist[i].adr.port) {
			continue;
		}
		CL_GetPing( i, buff, MAX_STRING_CHARS, &pingTime );
		if (pingTime != 0) {
			CL_ClearPing(i);
			status = qtrue;
		}
	}

	return status;
}


/*
==================
CL_ServerStatus_f
==================
*/
static void CL_ServerStatus_f( void ) {
	netadr_t	to, *toptr = NULL;
	const char		*server;
	serverStatus_t *serverStatus;
	int			argc;
	netadrtype_t	family = NA_UNSPEC;

	argc = Cmd_Argc();

	if ( argc != 2 && argc != 3 )
	{
		if (cls.state != CA_ACTIVE || clc.demoplaying)
		{
			Com_Printf( "Not connected to a server.\n" );
#ifdef USE_IPV6
			Com_Printf( "usage: serverstatus [-4|-6] <server>\n" );
#else
			Com_Printf("usage: serverstatus <server>\n");
#endif
			return;
		}

		toptr = &clc.serverAddress;
	}

	if ( !toptr )
	{
		Com_Memset( &to, 0, sizeof( to ) );

		if ( argc == 2 )
			server = Cmd_Argv(1);
		else
		{
			if ( !strcmp( Cmd_Argv(1), "-4" ) )
				family = NA_IP;
#ifdef USE_IPV6
			else if ( !strcmp( Cmd_Argv(1), "-6" ) )
				family = NA_IP6;
			else
				Com_Printf( "warning: only -4 or -6 as address type understood.\n" );
#else
			else
				Com_Printf( "warning: only -4 as address type understood.\n" );
#endif

			server = Cmd_Argv(2);
		}

		toptr = &to;
		if ( !NET_StringToAdr( server, toptr, family ) )
			return;
	}

	NET_OutOfBandPrint( NS_CLIENT, toptr, "getstatus" );

	serverStatus = CL_GetServerStatus( toptr );
	serverStatus->address = *toptr;
	serverStatus->print = qtrue;
	serverStatus->pending = qtrue;
}


/*
==================
CL_ShowIP_f
==================
*/
static void CL_ShowIP_f( void ) {
	Sys_ShowIP();
}

#ifdef USE_FLUX
/*
==================
FLUX Generation Thread Function
==================
*/
#if USE_SDL
static int CL_FluxGenerationThread(void *data) {
	flux_job_t *job = (flux_job_t *)data;
	flux_ctx *ctx = NULL;
	flux_params params = FLUX_PARAMS_DEFAULT;
	flux_image *image = NULL;

	// Validate job pointer
	if (!job) {
		Com_Printf(S_COLOR_RED "FLUX: Thread received NULL job pointer\n");
		return -1;
	}

	// Validate critical job fields before proceeding
	if (!job->model_path[0]) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: model_path is empty\n");
		Q_strncpyz(job->error_msg, "Model path is empty", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	if (!job->prompt[0]) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: prompt is empty\n");
		Q_strncpyz(job->error_msg, "Prompt is empty", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	if (job->width <= 0 || job->height <= 0) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: Invalid dimensions (width:%d height:%d)\n", job->width, job->height);
		Q_strncpyz(job->error_msg, "Invalid image dimensions", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	// External generation mode to avoid in-process crashes
	if (cl_flux_external && cl_flux_external->integer) {
		if (!CL_FluxGenerateExternal(job->model_path, job->prompt, job->output_path,
									 job->width, job->height, job->steps, job->seed,
									 job->error_msg, sizeof(job->error_msg))) {
			job->status = FLUX_JOB_FAILED;
			return -1;
		}
		job->status = FLUX_JOB_COMPLETED;
		return 0;
	}

	// Load FLUX model - construct absolute path from base path
	char full_model_path[1024];
	const char *base_path = Sys_DefaultBasePath();
	if (!base_path) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: Sys_DefaultBasePath() returned NULL\n");
		Q_strncpyz(job->error_msg, "Failed to get base path", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return -1;
	}
	Com_sprintf(full_model_path, sizeof(full_model_path), "%s/%s", base_path, job->model_path);

	// Add safety check for model path
	if (strlen(full_model_path) >= sizeof(full_model_path) - 1) {
		Q_strncpyz(job->error_msg, "Model path too long", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	Com_Printf("FLUX: About to call flux_load_dir()...\n");
	ctx = flux_load_dir(full_model_path);
	Com_Printf("FLUX: flux_load_dir() returned: %p\n", (void*)ctx);
	if (!ctx) {
		const char *error = flux_get_error();
		Com_Printf("FLUX: flux_load_dir() failed with error: %s\n", error ? error : "NULL");
		Q_strncpyz(job->error_msg, error ? error : "Model loading failed", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return -1;
	}
	Com_Printf("FLUX: Model loaded successfully\n");

	// Set up generation parameters
	Com_Printf("FLUX: Setting up parameters - width:%d height:%d steps:%d seed:%d\n",
		job->width, job->height, job->steps, job->seed);
	params.width = job->width;
	params.height = job->height;
	params.num_steps = job->steps;
	params.seed = job->seed;

	// Validate parameters before generation
	Com_Printf("FLUX: Validating parameters...\n");
	if (strlen(job->prompt) == 0) {
		Com_Printf("FLUX: Empty prompt detected\n");
		Q_strncpyz(job->error_msg, "Empty prompt", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	if (job->width <= 0 || job->height <= 0 || job->width > 2048 || job->height > 2048) {
		Com_Printf("FLUX: Invalid dimensions - width:%d height:%d\n", job->width, job->height);
		Q_strncpyz(job->error_msg, "Invalid image dimensions", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}
	Com_Printf("FLUX: Parameters validated successfully\n");

	// Generate image with error handling
	Com_Printf("FLUX: About to call flux_generate() with prompt: '%s'\n", job->prompt);
        Com_Printf("FLUX: Parameters: width=%d, height=%d, steps=%d, seed=%lld\n",
                params.width, params.height, params.num_steps, (long long)params.seed);
	
	// Validate context and prompt before generation
	if (!ctx) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: ctx is NULL before flux_generate()\n");
		Q_strncpyz(job->error_msg, "FLUX context is NULL", sizeof(job->error_msg));
		job->status = FLUX_JOB_FAILED;
		return -1;
	}
	if (strlen(job->prompt) == 0) {
		Com_Printf(S_COLOR_RED "FLUX: Thread: prompt is empty before flux_generate()\n");
		Q_strncpyz(job->error_msg, "Prompt is empty", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	// CRITICAL: This is where the segmentation fault likely occurs
	// Known stability issue: flux_generate() can crash the engine
	// This is documented in README_idtech3.md as a critical stability issue
	Com_Printf("FLUX: Calling flux_generate() - this may take 30-120+ seconds...\n");
	image = flux_generate(ctx, job->prompt, &params);

	Com_Printf("FLUX: flux_generate() returned: %p\n", (void*)image);
	if (!image) {
		const char *error = flux_get_error();
		Com_Printf("FLUX: flux_generate() returned NULL, error: %s\n", error ? error : "NULL");
		if (error) {
			Q_strncpyz(job->error_msg, error, sizeof(job->error_msg));
		} else {
			Q_strncpyz(job->error_msg, "flux_generate returned NULL with no error message", sizeof(job->error_msg));
		}
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}
	Com_Printf("FLUX: flux_generate() completed successfully - image: %dx%d\n", image->width, image->height);

	// Validate generated image
	Com_Printf("FLUX: Validating generated image...\n");
	if (!image) {
		Com_Printf("FLUX: Image pointer is NULL!\n");
		Q_strncpyz(job->error_msg, "Generated image pointer is NULL", sizeof(job->error_msg));
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	if (!image->data || image->width <= 0 || image->height <= 0) {
		Com_Printf("FLUX: Invalid image data - data:%p width:%d height:%d\n",
			(void *)image->data, image->width, image->height);
		Q_strncpyz(job->error_msg, "Generated image is invalid", sizeof(job->error_msg));
		flux_image_free(image);
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}
	Com_Printf("FLUX: Image validation passed\n");

	// Save image with error checking
	Com_Printf("FLUX: Saving image to: %s\n", job->output_path);
	int result = flux_image_save(image, job->output_path);
	Com_Printf("FLUX: flux_image_save() returned: %d\n", result);
	if (result != 0) {
		Com_sprintf(job->error_msg, sizeof(job->error_msg), "Failed to save image to %s (error: %d)", job->output_path, result);
		flux_image_free(image);
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}

	// Verify file was actually created
	Com_Printf("FLUX: Verifying saved file...\n");
	FILE *test_file = fopen(job->output_path, "rb");
	if (!test_file) {
		Com_sprintf(job->error_msg, sizeof(job->error_msg), "Image file not found after save: %s", job->output_path);
		flux_image_free(image);
		flux_free(ctx);
		job->status = FLUX_JOB_FAILED;
		return -1;
	}
	fclose(test_file);
	Com_Printf("FLUX: File saved and verified successfully\n");

	// Store result
	Com_Printf("FLUX: Storing result and marking as completed\n");
	job->result = image;
	job->status = FLUX_JOB_COMPLETED;

	// Clean up (keep image for main thread to handle)
	Com_Printf("FLUX: Cleaning up FLUX context\n");
	flux_free(ctx);
	Com_Printf("FLUX: Thread completed successfully\n");

	return 0;
}
#endif

/*
==================
CL_FluxGenerate_f
==================
*/
static void CL_FluxGenerate_f( void ) {
	const char *prompt;

	// Check if FLUX is enabled
	if (!cl_flux_enable || !cl_flux_enable->integer) {
		Com_Printf(S_COLOR_YELLOW "FLUX image generation is disabled. Set cl_flux_enable 1 to enable.\n");
		return;
	}

	// Check if asynchronous job is already running
	if (cl_flux_async->integer && flux_job.status == FLUX_JOB_RUNNING) {
		Com_Printf(S_COLOR_YELLOW "FLUX: Generation already in progress. Wait for completion or use 'flux_cancel' to stop.\n");
		return;
	}

	// Get prompt from command arguments
	if (Cmd_Argc() < 2) {
		Com_Printf(S_COLOR_YELLOW "Usage: flux_generate <prompt>\n");
		return;
	}
	prompt = Cmd_ArgsFrom(1);

	// Check if a job is already running
	if (flux_job.status == FLUX_JOB_RUNNING) {
		Com_Printf(S_COLOR_YELLOW "FLUX: Generation already in progress. Wait for completion or use flux_status to check.\n");
		return;
	}

	// Validate parameters
	int width = cl_flux_width->integer;
	int height = cl_flux_height->integer;
	int steps = cl_flux_steps->integer;
	int seed = cl_flux_seed->integer;

	// Validate dimensions are multiples of 16 (FLUX requirement)
	if (width % 16 != 0 || height % 16 != 0) {
		Com_Printf(S_COLOR_YELLOW "FLUX: Warning: Dimensions should be multiples of 16 for best results (%dx%d)\n",
			width, height);
	}

	// Validate steps
	if (steps < 1 || steps > 50) {
		Com_Printf(S_COLOR_RED "FLUX: Invalid number of steps: %d (must be 1-50)\n", steps);
		return;
	}

	// Choose generation mode based on cvar
	if (cl_flux_async->integer) {
#if USE_SDL
		// Asynchronous (background) mode
		goto async_generation;
#else
		Com_Printf(S_COLOR_YELLOW "FLUX: Async generation requires SDL threads; falling back to sync mode\n");
		goto sync_generation;
#endif
	} else {
		// Synchronous (blocking) mode - revert to original working implementation
		goto sync_generation;
	}

#if USE_SDL
async_generation:
	// Asynchronous generation code

	// Initialize job with safety checks - zero everything first
	Com_Memset(&flux_job, 0, sizeof(flux_job));
	
	// Set all values BEFORE marking as RUNNING to avoid race conditions
	flux_job.start_time = Com_Milliseconds();
	flux_job.timeout_seconds = 300; // 5 minute timeout

	// Get model path based on selected variant
	const char *model_path = CL_FluxGetModelPath(cl_flux_model->string);
	if (!model_path || !*model_path) {
		Com_Printf(S_COLOR_RED "FLUX: Invalid model path\n");
		return;
	}
	
	// Check if FLUX.1 is selected but files might not be available
	if ((Q_stricmp(cl_flux_model->string, "flux1-schnell") == 0 || 
	     Q_stricmp(cl_flux_model->string, "flux1-dev") == 0) &&
	    Q_stricmp(model_path, "flux") != 0) {
		char test_path[MAX_OSPATH];
		Com_sprintf(test_path, sizeof(test_path), "%s/%s/vae/diffusion_pytorch_model.safetensors", 
		            Sys_DefaultBasePath(), model_path);
		if (!CL_FluxFileExists(test_path)) {
			Com_Printf(S_COLOR_YELLOW "FLUX: FLUX.1 model files not found in %s/\n", model_path);
			Com_Printf(S_COLOR_YELLOW "FLUX: To use FLUX.1, download model files manually (see MANUAL_DOWNLOAD.md)\n");
			Com_Printf(S_COLOR_YELLOW "FLUX: Or switch to FLUX.2: /cl_flux_model flux2-dev\n");
			return;
		}
	}
	
	Q_strncpyz(flux_job.model_path, model_path, sizeof(flux_job.model_path));

	// Validate and copy prompt
	if (!prompt || strlen(prompt) == 0) {
		Com_Printf(S_COLOR_RED "FLUX: Empty prompt\n");
		return;
	}
	Q_strncpyz(flux_job.prompt, prompt, sizeof(flux_job.prompt));

	flux_job.width = width;
	flux_job.height = height;
	flux_job.steps = steps;
	flux_job.seed = seed;

	// Create output filename with timestamp
	Com_sprintf(flux_job.output_path, sizeof(flux_job.output_path), "flux_%d_%dx%d.png",
		Com_Milliseconds(), width, height);
	
	// Verify all critical fields are set before marking as RUNNING
	if (!flux_job.model_path[0] || !flux_job.prompt[0] || flux_job.width <= 0 || flux_job.height <= 0) {
		Com_Printf(S_COLOR_RED "FLUX: Internal error - job structure not properly initialized\n");
		Com_Printf(S_COLOR_RED "FLUX: model_path='%s', prompt='%s', width=%d, height=%d\n",
			flux_job.model_path, flux_job.prompt, flux_job.width, flux_job.height);
		return;
	}
	
	// Mark as RUNNING only after all values are set (memory barrier for thread safety)
	flux_job.status = FLUX_JOB_RUNNING;

	// Start background thread
	flux_job.thread = SDL_CreateThread(CL_FluxGenerationThread, "FLUX_Generation", &flux_job);
	if (!flux_job.thread) {
		Com_Printf(S_COLOR_RED "FLUX: Failed to create generation thread\n");
		flux_job.status = FLUX_JOB_IDLE;
		return;
	}

	Com_Printf("FLUX: Started background generation for prompt: %s\n", prompt);
	Com_Printf("FLUX: Model: %s, Dimensions: %dx%d, Steps: %d, Seed: %d\n",
		flux_job.model_path, flux_job.width, flux_job.height, flux_job.steps, flux_job.seed);
	Com_Printf("FLUX: Use 'flux_status' to check progress, 'flux_view <filename>' when complete\n");
	return;
#endif

sync_generation:
	// Synchronous (blocking) mode - original working implementation
	{
		char outputPath[MAX_OSPATH];
		flux_ctx *ctx = NULL;
		flux_params params = FLUX_PARAMS_DEFAULT;
		flux_image *image = NULL;
		int result;

		Com_Printf("FLUX: Generating image with prompt: %s\n", prompt);

		if (cl_flux_external && cl_flux_external->integer) {
			const char *sync_model_path = CL_FluxGetModelPath(cl_flux_model->string);
			Com_sprintf(outputPath, sizeof(outputPath), "flux_%d_%dx%d.png",
				Com_Milliseconds(), width, height);
			if (!CL_FluxGenerateExternal(sync_model_path, prompt, outputPath,
										 width, height, steps, seed,
										 NULL, 0)) {
				Com_Printf(S_COLOR_RED "FLUX: External generation failed (see console for details)\n");
				return;
			}

			Com_Printf(S_COLOR_GREEN "FLUX: Image saved to %s (seed: %d)\n", outputPath, seed);
			if (re.ReloadTexture) {
				if (re.ReloadTexture(outputPath)) {
					Com_Printf(S_COLOR_GREEN "FLUX: Texture hot-reloaded successfully!\n");
				} else {
					Com_Printf(S_COLOR_YELLOW "FLUX: Hot-reload failed, use 'vid_restart' to reload all textures\n");
				}
			} else {
				Com_Printf(S_COLOR_YELLOW "FLUX: Renderer doesn't support hot-reload, use 'vid_restart'\n");
			}
			return;
		}

		// Load FLUX model - construct absolute path from base path
		char full_model_path[1024];
		const char *sync_model_path = CL_FluxGetModelPath(cl_flux_model->string);
		Com_sprintf(full_model_path, sizeof(full_model_path), "%s/%s", Sys_DefaultBasePath(), sync_model_path);
		ctx = flux_load_dir(full_model_path);
		if (!ctx) {
			const char *error = flux_get_error();
			Com_Printf(S_COLOR_RED "FLUX: Failed to load model from %s: %s\n",
				full_model_path, error ? error : "Unknown error");
			
			// Suggest FLUX.2 if FLUX.1 fails
			if (Q_stricmp(cl_flux_model->string, "flux1-schnell") == 0 || 
			    Q_stricmp(cl_flux_model->string, "flux1-dev") == 0) {
				Com_Printf(S_COLOR_YELLOW "FLUX: FLUX.1 model files may be missing or corrupted.\n");
				Com_Printf(S_COLOR_YELLOW "FLUX: Try switching to FLUX.2: /cl_flux_model flux2-dev\n");
			}
			return;
		}

		// Set up generation parameters
		params.width = width;
		params.height = height;
		params.num_steps = steps;
		params.seed = seed;

		// Generate image
		image = flux_generate(ctx, prompt, &params);
		if (!image) {
			Com_Printf(S_COLOR_RED "FLUX: Generation failed: %s\n", flux_get_error());
			flux_free(ctx);
			return;
		}

		// Create output filename with timestamp
		Com_sprintf(outputPath, sizeof(outputPath), "flux_%d_%dx%d.png",
			Com_Milliseconds(), image->width, image->height);

		// Save image
		result = flux_image_save(image, outputPath);
		if (result != 0) {
			Com_Printf(S_COLOR_RED "FLUX: Failed to save image to %s\n", outputPath);
		} else {
			Com_Printf(S_COLOR_GREEN "FLUX: Image saved to %s (seed: %lld)\n",
				outputPath, (long long)params.seed);

			// Try to hot-reload the texture if renderer supports it
			if (re.ReloadTexture) {
				if (re.ReloadTexture(outputPath)) {
					Com_Printf(S_COLOR_GREEN "FLUX: Texture hot-reloaded successfully!\n");
				} else {
					Com_Printf(S_COLOR_YELLOW "FLUX: Hot-reload failed, use 'vid_restart' to reload all textures\n");
				}
			} else {
				Com_Printf(S_COLOR_YELLOW "FLUX: Renderer doesn't support hot-reload, use 'vid_restart'\n");
			}
		}

		// Clean up
		flux_image_free(image);
		flux_free(ctx);
	}
}

/*
==================
CL_FluxDevices_f
==================
*/
static void CL_FluxDevices_f( void ) {
	Com_Printf("FLUX Device & Backend Information:\n");
	Com_Printf("===================================\n");
	Com_Printf("Current device setting: %s\n", cl_flux_device->string);
	Com_Printf("Model variant: %s\n", cl_flux_model->string);
	Com_Printf("\n");

	// Show available FLUX backends (compile-time)
	Com_Printf("Compiled FLUX backends:\n");
#if defined(USE_METAL)
	Com_Printf("  ✅ Metal Performance Shaders (Apple Silicon)\n");
#endif
#if defined(USE_BLAS)
	Com_Printf("  ✅ BLAS accelerated (Intel/AMD CPUs)\n");
#endif
#if defined(USE_CUDA)
	Com_Printf("  ✅ CUDA (NVIDIA GPUs)\n");
#endif
#if defined(USE_VULKAN)
	Com_Printf("  ✅ Vulkan (Cross-platform GPU)\n");
#endif
#if !defined(USE_METAL) && !defined(USE_BLAS) && !defined(USE_CUDA) && !defined(USE_VULKAN)
	Com_Printf("  ⚠️  Generic C fallback (slow CPU-only)\n");
#endif

	Com_Printf("\nDevice selection options:\n");
	Com_Printf("  auto     - Use best available backend\n");
	Com_Printf("  cpu      - Force CPU-only processing\n");
	Com_Printf("  gpu      - Use GPU acceleration\n");
	Com_Printf("  gpu:N    - Use specific GPU (gpu:0, gpu:1, etc.)\n");

	Com_Printf("\nNote: Device selection requires FLUX library support.\n");
	Com_Printf("Currently using compile-time backend selection.\n");
}

/*
==================
CL_FluxCancel_f
==================
*/
static void CL_FluxCancel_f( void ) {
	if (flux_job.status == FLUX_JOB_RUNNING) {
		Com_Printf("FLUX: Cancelling generation...\n");

		// Set status to failed first to prevent race conditions
		flux_job.status = FLUX_JOB_FAILED;
		Q_strncpyz(flux_job.error_msg, "Generation cancelled by user", sizeof(flux_job.error_msg));

#if USE_SDL
		if (flux_job.thread) {
			// Wait for thread to finish
			int thread_result = 0;
			SDL_WaitThread(flux_job.thread, &thread_result);
			flux_job.thread = NULL;
			Com_Printf("FLUX: Thread cancelled (exit code: %d)\n", thread_result);
		}
#endif

		// Clean up any allocated resources
		if (flux_job.result) {
			flux_image_free(flux_job.result);
			flux_job.result = NULL;
		}

		// Remove any partial output file
		if (flux_job.output_path[0]) {
			remove(flux_job.output_path);
		}

		flux_job.status = FLUX_JOB_IDLE;
		Com_Printf("FLUX: Generation cancelled successfully\n");
	} else {
		Com_Printf("FLUX: No active generation to cancel\n");
	}
}

/*
==================
CL_FluxStatus_f
==================
*/
static void CL_FluxStatus_f( void ) {
	switch (flux_job.status) {
		case FLUX_JOB_IDLE:
			Com_Printf("FLUX: No generation in progress\n");
			Com_Printf("FLUX: Device: %s, Model: %s\n", cl_flux_device->string, cl_flux_model->string);
			break;
		case FLUX_JOB_RUNNING:
			{
				int runtime_seconds = (Com_Milliseconds() - flux_job.start_time) / 1000;
				int timeout_seconds = 300; // 5 minute timeout

				Com_Printf("FLUX: Generation in progress (%d seconds)...\n", runtime_seconds);
				Com_Printf("FLUX: Device: %s, Model: %s\n", cl_flux_device->string, cl_flux_model->string);
				Com_Printf("FLUX: Prompt: %s\n", flux_job.prompt);
				Com_Printf("FLUX: Output: %s\n", flux_job.output_path);
				Com_Printf("FLUX: Settings: %dx%d, %d steps, seed %d\n",
					flux_job.width, flux_job.height, flux_job.steps, flux_job.seed);

				if (runtime_seconds > timeout_seconds) {
					Com_Printf(S_COLOR_YELLOW "FLUX: Generation timeout after %d seconds, use 'flux_cancel' to stop\n", timeout_seconds);
				} else {
					int remaining = timeout_seconds - runtime_seconds;
					Com_Printf("FLUX: Image generation can take 30-120+ seconds depending on hardware (%d seconds until timeout)\n", remaining);
				}
			}
			break;
		case FLUX_JOB_COMPLETED:
			Com_Printf(S_COLOR_GREEN "FLUX: Generation completed!\n");
			Com_Printf("FLUX: Image saved to %s\n", flux_job.output_path);
			Com_Printf("FLUX: Use 'flux_view %s' to display the image\n", flux_job.output_path);
			Com_Printf(S_COLOR_CYAN "FLUX: You can also use 'flux_view' without arguments to view this image\n");
			break;
		case FLUX_JOB_FAILED:
			Com_Printf(S_COLOR_RED "FLUX: Generation failed: %s\n", flux_job.error_msg);
			break;
	}
}

/*
==================
CL_FluxReload_f
==================
*/
static void CL_FluxReload_f( void ) {
	const char *filename;

	if (Cmd_Argc() < 2) {
		Com_Printf(S_COLOR_YELLOW "Usage: flux_reload <filename>\n");
		Com_Printf(S_COLOR_YELLOW "Example: flux_reload flux_123456_256x256.png\n");
		return;
	}

	filename = Cmd_Argv(1);

	// Try to reload the texture
	if (re.ReloadTexture && re.ReloadTexture(filename)) {
		Com_Printf(S_COLOR_GREEN "FLUX: Successfully reloaded texture %s\n", filename);
	} else {
		Com_Printf(S_COLOR_YELLOW "FLUX: Failed to reload texture %s. Use 'vid_restart' if needed.\n", filename);
	}
}

/*
==================
CL_FluxShow_f
==================
*/
static void CL_FluxShow_f( void ) {
	const char *filename;
	qhandle_t shader;

	if (Cmd_Argc() < 2) {
		Com_Printf(S_COLOR_YELLOW "Usage: flux_show <filename>\n");
		Com_Printf(S_COLOR_YELLOW "Example: flux_show flux_123456_256x256.png\n");
		Com_Printf(S_COLOR_CYAN "This will register the image as a shader for use in menus/materials\n");
		return;
	}

	filename = Cmd_Argv(1);

	// Register the image as a shader (this forces loading/reloading)
	shader = re.RegisterShader(filename);
	if (shader) {
		Com_Printf(S_COLOR_GREEN "FLUX: Registered/updated shader '%s' (handle: %d)\n", filename, shader);
		Com_Printf(S_COLOR_CYAN "FLUX: Image is now available for use in the engine\n");
		Com_Printf(S_COLOR_CYAN "FLUX: You can now see the image in menus or use it in materials\n");
	} else {
		Com_Printf(S_COLOR_RED "FLUX: Failed to register shader for %s\n", filename);
	}
}

/*
==================
CL_FluxView_f
==================
*/
static void CL_FluxView_f( void ) {
	const char *filename;
	qboolean reloadSuccess = qfalse;
	qhandle_t shader;

	// If no filename provided, check for completed job
	if (Cmd_Argc() < 2) {
		if (flux_job.status == FLUX_JOB_COMPLETED && flux_job.output_path[0]) {
			filename = flux_job.output_path;
			Com_Printf("FLUX: Using completed job output: %s\n", filename);
		} else {
			Com_Printf(S_COLOR_YELLOW "Usage: flux_view <filename>\n");
			Com_Printf(S_COLOR_YELLOW "Example: flux_view flux_123456_256x256.png\n");
			Com_Printf(S_COLOR_CYAN "This will reload the texture and register it as a shader for viewing\n");
			if (flux_job.status == FLUX_JOB_COMPLETED) {
				Com_Printf(S_COLOR_CYAN "Or use 'flux_view' with no arguments to view the last completed generation\n");
			}
			return;
		}
	} else {
		filename = Cmd_Argv(1);
	}

	// First try to reload the texture
	if (re.ReloadTexture) {
		reloadSuccess = re.ReloadTexture(filename);
		if (reloadSuccess) {
			Com_Printf(S_COLOR_GREEN "FLUX: Texture reloaded: %s\n", filename);
		} else {
			Com_Printf(S_COLOR_YELLOW "FLUX: Texture reload failed, but continuing...\n");
		}
	}

	// Then register/update the shader
	shader = re.RegisterShader(filename);
	if (shader) {
		Com_Printf(S_COLOR_GREEN "FLUX: Shader registered: '%s' (handle: %d)\n", filename, shader);
		Com_Printf(S_COLOR_CYAN "FLUX: Image is now viewable in the engine!\n");
		if (reloadSuccess) {
			Com_Printf(S_COLOR_CYAN "FLUX: Hot-reload successful - no vid_restart needed!\n");
		}
	} else {
		Com_Printf(S_COLOR_RED "FLUX: Failed to register shader for %s\n", filename);
	}

	// If this was a completed job, clean it up
	if (flux_job.status == FLUX_JOB_COMPLETED && strcmp(filename, flux_job.output_path) == 0) {
		if (flux_job.result) {
			flux_image_free(flux_job.result);
			flux_job.result = NULL;
		}
#if USE_SDL
		if (flux_job.thread) {
			SDL_WaitThread(flux_job.thread, NULL);
			flux_job.thread = NULL;
		}
#endif
		flux_job.status = FLUX_JOB_IDLE;
		Com_Printf(S_COLOR_GREEN "FLUX: Job completed and cleaned up\n");
	}
}

/*
==================
CL_FontsPipeline_f

Run a user-configured shell command for the FonTS (ICCV 2025) typography pipeline.
Upstream FonTS is Python + PyTorch + diffusers; the engine only orchestrates via system().
==================
*/
static void CL_FontsPipeline_f( void ) {
	char cmd[8192];
	const char *repo;
	const char *base;
	const char *py;
	const char *args;
	const char *tmpl;

	if ( !cl_fonts_enable || !cl_fonts_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "fonts_pipeline: set cl_fonts_enable 1 (see docs/FONTS.md)\n" );
		return;
	}
	repo = cl_fonts_repo ? cl_fonts_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "fonts_pipeline: set cl_fonts_repo to your FonTS checkout path\n" );
		return;
	}
	base = Sys_DefaultBasePath();
	if ( !base ) {
		Com_Printf( S_COLOR_RED "fonts_pipeline: no engine base path\n" );
		return;
	}
	py = ( cl_fonts_python && cl_fonts_python->string[0] ) ? cl_fonts_python->string : "python3";
	tmpl = cl_fonts_cmd ? cl_fonts_cmd->string : "";
	if ( !tmpl || !tmpl[0] ) {
		Com_Printf( S_COLOR_YELLOW "fonts_pipeline: set cl_fonts_cmd (template with %%R %%B %%P %%A — see docs/FONTS.md)\n" );
		return;
	}
	args = ( Cmd_Argc() >= 2 ) ? Cmd_ArgsFrom( 1 ) : "";
	{
		cl_pipeline_expand_t ex;
		Com_Memset( &ex, 0, sizeof( ex ) );
		ex.repo = repo;
		ex.base = base;
		ex.py = py;
		ex.args = args;
		if ( !CL_PipelineExpandTemplate( cmd, sizeof( cmd ), tmpl, &ex ) ) {
			Com_Printf( S_COLOR_RED "fonts_pipeline: expanded command too long or bad path characters\n" );
			return;
		}
	}
	Com_Printf( "FonTS: executing (blocking): %s\n", cmd );
	if ( system( cmd ) != 0 ) {
		Com_Printf( S_COLOR_RED "fonts_pipeline: shell returned non-zero\n" );
		return;
	}
	Com_Printf( S_COLOR_GREEN "fonts_pipeline: finished\n" );
}
#endif

#ifdef USE_TRELLIS
/*
==================
CL_TrellisPipeline_f

Run a user-configured shell command for the TRELLIS.2 Python/CUDA stack.
==================
*/
static void CL_TrellisPipeline_f( void ) {
	const char *repo;
	const char *base;
	const char *engine;
	const char *py;
	const char *conda;
	const char *tmpl;
	const char *args;
	cl_pipeline_expand_t ex;

	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_pipeline: set cl_trellis_enable 1 (see docs/TRELLIS.md)\n" );
		return;
	}
	repo = cl_trellis_repo ? cl_trellis_repo->string : "";
	if ( !repo || !repo[0] ) {
		Com_Printf( S_COLOR_YELLOW "trellis_pipeline: set cl_trellis_repo to your TRELLIS.2 checkout\n" );
		return;
	}
	base = Sys_DefaultBasePath();
	engine = base;
	if ( !base ) {
		Com_Printf( S_COLOR_RED "trellis_pipeline: no engine base path\n" );
		return;
	}
	py = ( cl_trellis_python && cl_trellis_python->string[0] ) ? cl_trellis_python->string : "python3";
	conda = ( cl_trellis_conda && cl_trellis_conda->string[0] ) ? cl_trellis_conda->string : "trellis2";
	tmpl = ( cl_trellis_cmd && cl_trellis_cmd->string[0] ) ? cl_trellis_cmd->string : CL_TrellisDefaultCmd();
	args = ( Cmd_Argc() >= 2 ) ? Cmd_ArgsFrom( 1 ) : "";
	Com_Memset( &ex, 0, sizeof( ex ) );
	ex.repo = repo;
	ex.base = base;
	ex.engine = engine;
	ex.py = py;
	ex.conda = conda;
	ex.args = args;
	if ( !CL_TrellisRunExpanded( tmpl, &ex, qfalse ) ) {
		return;
	}
	Com_Printf( S_COLOR_GREEN "trellis_pipeline: finished\n" );
}

/*
==================
CL_TrellisGenerate_f
==================
*/
static void CL_TrellisGenerate_f( void ) {
	const char *output_opt;

	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_generate: set cl_trellis_enable 1 (see docs/TRELLIS.md)\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( S_COLOR_YELLOW "Usage: trellis_generate <image_path> [output.glb]\n" );
		Com_Printf( "  Example: trellis_generate screenshots/shot0000.tga\n" );
		return;
	}
	output_opt = ( Cmd_Argc() >= 3 ) ? Cmd_Argv( 2 ) : NULL;
	CL_TrellisStartJob( Cmd_Argv( 1 ), output_opt );
}

/*
==================
CL_TrellisStatus_f / CL_TrellisCancel_f
==================
*/
static void CL_TrellisStatus_f( void ) {
	switch ( trellis_job.status ) {
	case TRELLIS_JOB_IDLE:
		Com_Printf( "TRELLIS: no generation in progress\n" );
		break;
	case TRELLIS_JOB_RUNNING: {
		int runtime = ( Com_Milliseconds() - trellis_job.start_time ) / 1000;
		Com_Printf( "TRELLIS: generation in progress (%d s)\n", runtime );
		Com_Printf( "TRELLIS: image: %s -> %s\n", trellis_job.image_vfs, trellis_job.output_vfs );
		break;
	}
	case TRELLIS_JOB_COMPLETED:
		Com_Printf( S_COLOR_GREEN "TRELLIS: complete: %s (handle %d)\n",
			trellis_job.output_vfs, trellis_job.model_handle );
		Com_Printf( "TRELLIS: trellis_view to re-register; trellis_show for spawn hint\n" );
		break;
	case TRELLIS_JOB_FAILED:
		Com_Printf( S_COLOR_RED "TRELLIS: failed: %s\n", trellis_job.error_msg );
		break;
	}
}

static void CL_TrellisCancel_f( void ) {
	if ( trellis_job.status != TRELLIS_JOB_RUNNING ) {
		Com_Printf( "TRELLIS: no active generation to cancel\n" );
		return;
	}
	trellis_job.status = TRELLIS_JOB_FAILED;
	Q_strncpyz( trellis_job.error_msg, "Cancelled by user", sizeof( trellis_job.error_msg ) );
	CL_TrellisJoinThread();
	Com_Printf( "TRELLIS: background job stopped (subprocess may still run until exit)\n" );
	if ( trellis_job.output_full[0] ) {
		remove( trellis_job.output_full );
	}
	trellis_job.status = TRELLIS_JOB_IDLE;
	Com_Printf( "TRELLIS: generation cancelled\n" );
}

/*
==================
CL_TrellisShow_f / CL_TrellisView_f — mirror flux_show / flux_view for models
==================
*/
static void CL_TrellisShow_f( void ) {
	const char *name;

	if ( Cmd_Argc() < 2 ) {
		if ( trellis_job.status == TRELLIS_JOB_COMPLETED && trellis_job.output_vfs[0] ) {
			name = trellis_job.output_vfs;
		} else {
			Com_Printf( S_COLOR_YELLOW "Usage: trellis_show <models/.../asset.glb>\n" );
			return;
		}
	} else {
		name = Cmd_Argv( 1 );
	}
	if ( CL_TrellisImportVfs( name ) ) {
		Com_Printf( S_COLOR_GREEN "TRELLIS: model ready at '%s'\n", name );
		Com_Printf( "  Use in maps: misc_model \"%s\" (or your game's model entity)\n", name );
	} else {
		Com_Printf( S_COLOR_RED "TRELLIS: failed to register '%s'\n", name );
	}
}

static void CL_TrellisView_f( void ) {
	const char *name;
	qhandle_t h;

	if ( Cmd_Argc() < 2 ) {
		if ( trellis_job.status == TRELLIS_JOB_COMPLETED && trellis_job.output_vfs[0] ) {
			name = trellis_job.output_vfs;
			Com_Printf( "TRELLIS: using last job output: %s\n", name );
		} else {
			Com_Printf( S_COLOR_YELLOW "Usage: trellis_view <models/.../asset.glb>\n" );
			return;
		}
	} else {
		name = Cmd_Argv( 1 );
	}
	h = CL_TrellisImportVfs( name );
	if ( !h ) {
		Com_Printf( S_COLOR_RED "TRELLIS: failed to register '%s'\n", name );
		return;
	}
	trellis_job.model_handle = h;
	Com_Printf( S_COLOR_GREEN "TRELLIS: registered '%s' (handle %d)\n", name, h );
	CL_TrellisJoinThread();
	if ( trellis_job.status == TRELLIS_JOB_COMPLETED || trellis_job.status == TRELLIS_JOB_FAILED ) {
		trellis_job.status = TRELLIS_JOB_IDLE;
		trellis_job.notified = qfalse;
	}
}

static void CL_TrellisImport_f( void ) {
	CL_TrellisShow_f();
}

#if defined( USE_FLUX )
/*
==================
CL_TrellisFromPrompt_f — FLUX image then TRELLIS mesh (runtime text-to-3D)
==================
*/
static void CL_TrellisFromPrompt_f( void ) {
	const char *prompt;
	qboolean flux_was_async;

	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_from_prompt: set cl_trellis_enable 1\n" );
		return;
	}
	if ( !cl_flux_enable || !cl_flux_enable->integer ) {
		Com_Printf( S_COLOR_YELLOW "trellis_from_prompt: set cl_flux_enable 1\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( S_COLOR_YELLOW "Usage: trellis_from_prompt <text prompt>\n" );
		return;
	}
	if ( flux_job.status == FLUX_JOB_RUNNING || trellis_job.status == TRELLIS_JOB_RUNNING ) {
		Com_Printf( S_COLOR_YELLOW "trellis_from_prompt: wait for current FLUX/TRELLIS job or cancel\n" );
		return;
	}
	prompt = Cmd_ArgsFrom( 1 );
	flux_was_async = cl_flux_async && cl_flux_async->integer;
	if ( !flux_was_async ) {
		Cvar_Set( "cl_flux_async", "1" );
	}
	Cvar_Set( "cl_trellis_chain", "1" );
	trellis_chain_armed = qtrue;
	Cbuf_AddText( "flux_generate \"" );
	Cbuf_AddText( prompt );
	Cbuf_AddText( "\"\n" );
	Com_Printf( "TRELLIS: FLUX started; will chain TRELLIS when image completes (cl_trellis_chain 1)\n" );
	if ( !flux_was_async ) {
		Com_Printf( "TRELLIS: enabled cl_flux_async 1 for this workflow\n" );
	}
}
#endif

#if defined( USE_FLUX ) && defined( USE_TRELLIS )
static void CL_TrellisMaybeChainFromFlux( void ) {
	if ( !cl_trellis_enable || !cl_trellis_enable->integer ) {
		trellis_chain_armed = qfalse;
		return;
	}
	if ( !cl_trellis_chain || !cl_trellis_chain->integer ) {
		trellis_chain_armed = qfalse;
		return;
	}
	if ( !trellis_chain_armed ) {
		return;
	}
	if ( flux_job.status == FLUX_JOB_RUNNING ) {
		return;
	}
	trellis_chain_armed = qfalse;
	if ( flux_job.status != FLUX_JOB_COMPLETED || !flux_job.output_path[0] ) {
		Com_Printf( S_COLOR_YELLOW "TRELLIS: FLUX chain aborted (FLUX did not complete successfully)\n" );
		return;
	}
	Com_Printf( "TRELLIS: chaining from FLUX output %s\n", flux_job.output_path );
	CL_TrellisStartJob( flux_job.output_path, NULL );
}
#endif

#endif /* USE_TRELLIS */

#if defined( USE_TRELLIS ) || defined( USE_SPEC_ENERGY )
/*
==================
CL_GenerativeFrame — per-frame generative job notifications and chaining
==================
*/
void CL_GenerativeFrame( void ) {
#ifdef USE_TRELLIS
	CL_TrellisFrame();
#endif
#ifdef USE_SPEC_ENERGY
	CL_SpecEnergyFrame();
#endif
#if defined( USE_FLUX ) && defined( USE_TRELLIS )
	CL_TrellisMaybeChainFromFlux();
#endif
}
#endif /* USE_TRELLIS || USE_SPEC_ENERGY */


#ifdef USE_CURL

qboolean CL_Download( const char *cmd, const char *pakname, qboolean autoDownload )
{
	char url[MAX_OSPATH];
	char name[MAX_CVAR_VALUE_STRING];
	const char *s;

	if ( cl_dlURL->string[0] == '\0' )
	{
		Com_Printf( S_COLOR_YELLOW "cl_dlURL cvar is not set\n" );
		return qfalse;
	}

	// skip leading slashes
	while ( *pakname == '/' || *pakname == '\\' )
		pakname++;

	// skip gamedir
	s = strrchr( pakname, '/' );
	if ( s )
		pakname = s+1;

	if ( !Com_DL_ValidFileName( pakname ) )
	{
		Com_Printf( S_COLOR_YELLOW "invalid file name: '%s'.\n", pakname );
		return qfalse;
	}

	if ( !Q_stricmp( cmd, "dlmap" ) )
	{
		Q_strncpyz( name, pakname, sizeof( name ) );
		FS_StripExt( name, ".pk3" );
		if ( !name[0] )
			return qfalse;
		s = va( "maps/%s.bsp", name );
		if ( FS_FileIsInPAK( s, NULL, url ) )
		{
			Com_Printf( S_COLOR_YELLOW " map %s already exists in %s.pk3\n", name, url );
			return qfalse;
		}
	}

	return Com_DL_Begin( &download, pakname, cl_dlURL->string, autoDownload );
}


/*
==================
CL_Download_f
==================
*/
static void CL_Download_f( void )
{
	if ( Cmd_Argc() < 2 || *Cmd_Argv( 1 ) == '\0' )
	{
		Com_Printf( "usage: %s <mapname>\n", Cmd_Argv( 0 ) );
		return;
	}

	if ( !strcmp( Cmd_Argv(1), "-" ) )
	{
		Com_DL_Cleanup( &download );
		return;
	}

	CL_Download( Cmd_Argv( 0 ), Cmd_Argv( 1 ), qfalse );
}
#endif // USE_CURL
