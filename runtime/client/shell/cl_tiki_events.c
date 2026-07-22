/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Dispatch allowlisted TIKI frame commands into sound / particle / dialogue.
Never executes arbitrary console text from assets.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cl_uber_effects.h"
#include "cl_tiki_events.h"

#ifdef USE_BABBLE
#include "../../../modules/dialogue/babble.h"
#endif

extern sfxHandle_t S_RegisterSound( const char *name, qboolean compressed );
extern void S_StartLocalSound( sfxHandle_t sfx, int channelNum );
extern void S_Mixer_NotifyVoiceActive( void );

static qboolean CL_Tiki_CmdAllowed( const char *cmd ) {
	static const char *allowed[] = {
		"sound", "sound_vo", "voice", "effect", "fx", "footstep",
		"tag", "shout", "dialogue", "babble", "particle", "viewkick",
		NULL
	};
	int i;
	if ( !cmd || !cmd[0] ) {
		return qfalse;
	}
	for ( i = 0; allowed[i]; i++ ) {
		if ( !Q_stricmp( cmd, allowed[i] ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

void CL_Tiki_DispatchFrameCmd( const char *cmd, const char *arg, float x, float y, float z ) {
	if ( !cmd || !cmd[0] ) {
		return;
	}
	if ( !CL_Tiki_CmdAllowed( cmd ) ) {
		Com_DPrintf( "TIKI: rejected frame cmd '%s'\n", cmd );
		return;
	}

	if ( !Q_stricmp( cmd, "sound" ) || !Q_stricmp( cmd, "sound_vo" ) ||
		!Q_stricmp( cmd, "voice" ) || !Q_stricmp( cmd, "footstep" ) ) {
		if ( arg && arg[0] && !strstr( arg, ".." ) ) {
			sfxHandle_t sfx = S_RegisterSound( arg, qfalse );
			if ( sfx ) {
				S_StartLocalSound( sfx, 0 );
				if ( !Q_stricmp( cmd, "voice" ) || !Q_stricmp( cmd, "sound_vo" ) ) {
					S_Mixer_NotifyVoiceActive();
				}
			}
		}
		return;
	}
	if ( !Q_stricmp( cmd, "effect" ) || !Q_stricmp( cmd, "fx" ) || !Q_stricmp( cmd, "particle" ) ) {
		if ( arg && arg[0] ) {
			CL_UberEffects_Emit( arg, x, y, z );
		}
		return;
	}
#ifdef USE_BABBLE
	if ( !Q_stricmp( cmd, "dialogue" ) || !Q_stricmp( cmd, "babble" ) ) {
		if ( arg && arg[0] ) {
			Babble_Start( arg );
		}
		return;
	}
#else
	(void)x;
	(void)y;
	(void)z;
#endif
	Com_DPrintf( "TIKI: unhandled allowlisted cmd '%s'\n", cmd );
}
