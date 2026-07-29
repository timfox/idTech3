/*
===========================================================================
Engine-level telemetry, replay timing, save slots, quests, and dialogue
lines for Lua/script integration (client-side game systems).
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "engine_db.h"
#include "g_engine_systems.h"
#include <string.h>

#define JSON_IMPLEMENTATION
#include "json.h"
#undef JSON_IMPLEMENTATION

#define TELEMETRY_MAX 64
#define SAVE_SLOTS 8
#define QUEST_MAX 64
#define DIALOGUE_MAX 32
#define DIALOGUE_CHOICE_MAX 8

typedef struct {
	char name[64];
	double value;
} telemEntry_t;

static telemEntry_t s_telem[TELEMETRY_MAX];
static int s_telemCount;
static cvar_t *cv_engine_telemetry;

static int s_replayFrame;
static int s_replayBaseTime;
static qboolean s_replayActive;
static cvar_t *cv_engine_replay;

typedef struct {
	char label[128];
	qboolean used;
} saveSlot_t;

static saveSlot_t s_saves[SAVE_SLOTS];
static int s_lastSaveSlot;

typedef struct {
	char id[64];
	char title[128];
	char stage[128];
	qboolean used;
} questEntry_t;

static questEntry_t s_quests[QUEST_MAX];
static int s_questCount;

typedef struct {
	char speaker[64];
	char text[512];
	char locKey[64];
	char voice[MAX_QPATH];
	float duration;
	int choiceCount;
	char choiceLabel[DIALOGUE_CHOICE_MAX][128];
	char choiceNext[DIALOGUE_CHOICE_MAX][64];
	qboolean used;
} dialogueLine_t;

static dialogueLine_t s_dialogue[DIALOGUE_MAX];
static int s_dialogueCount;
static cvar_t *cv_com_ubertools;

/* ---- Telemetry ---- */

void EngineTelemetry_Init( void ) {
	Com_Memset( s_telem, 0, sizeof( s_telem ) );
	s_telemCount = 0;
	cv_engine_telemetry = Cvar_Get( "engine_telemetry", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cv_engine_telemetry, "Enable engine telemetry counters (Engine.Telemetry.* Lua API)." );
	Com_Printf( "EngineTelemetry: initialized (cvar engine_telemetry=%d)\n", cv_engine_telemetry ? cv_engine_telemetry->integer : 1 );
}

void EngineTelemetry_Shutdown( void ) {
	s_telemCount = 0;
}

void EngineTelemetry_Record( const char *name, double value ) {
	int i;
	if ( !name || !name[0] || !cv_engine_telemetry || !cv_engine_telemetry->integer ) {
		return;
	}
	for ( i = 0; i < s_telemCount; i++ ) {
		if ( !Q_stricmp( s_telem[i].name, name ) ) {
			s_telem[i].value = value;
			return;
		}
	}
	if ( s_telemCount >= TELEMETRY_MAX ) {
		return;
	}
	Q_strncpyz( s_telem[s_telemCount].name, name, sizeof( s_telem[0].name ) );
	s_telem[s_telemCount].value = value;
	s_telemCount++;
}

double EngineTelemetry_Get( const char *name ) {
	int i;
	if ( !name ) {
		return 0.0;
	}
	for ( i = 0; i < s_telemCount; i++ ) {
		if ( !Q_stricmp( s_telem[i].name, name ) ) {
			return s_telem[i].value;
		}
	}
	return 0.0;
}

void EngineTelemetry_Clear( void ) {
	s_telemCount = 0;
}

/* ---- Replay / deterministic frame index ---- */

void EngineReplay_Init( void ) {
	s_replayFrame = 0;
	s_replayBaseTime = 0;
	s_replayActive = qfalse;
	cv_engine_replay = Cvar_Get( "engine_replay", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cv_engine_replay, "Drive Engine.Replay frame index from client snapshots (serverTime)." );
	Com_Printf( "EngineReplay: frame counter ready (cvar engine_replay=%d)\n",
		cv_engine_replay ? cv_engine_replay->integer : 1 );
}

void EngineReplay_Shutdown( void ) {
	s_replayActive = qfalse;
	s_replayFrame = 0;
}

void EngineReplay_BeginFrame( int serverTime ) {
	if ( !cv_engine_replay || !cv_engine_replay->integer ) {
		return;
	}
	if ( !s_replayActive ) {
		s_replayBaseTime = serverTime;
		s_replayFrame = 0;
		s_replayActive = qtrue;
	}
	s_replayFrame++;
	(void)serverTime;
}

void EngineReplay_EndFrame( void ) {
	if ( !cv_engine_replay || !cv_engine_replay->integer ) {
		return;
	}
	/* Hook for future record buffer */
}

int EngineReplay_GetFrameIndex( void ) {
	return s_replayFrame;
}

int EngineReplay_GetBaseTime( void ) {
	return s_replayBaseTime;
}

/* ---- Save slots (client-local JSON v1 + legacy .txt) ---- */

#define ENGINE_SAVE_MOD_VERSION "idtech3_engine"

static unsigned EngineSave_ChecksumLabel( const char *label ) {
	if ( !label ) {
		return 0u;
	}
	return Com_BlockChecksum( label, (int)strlen( label ) );
}

/*
===============
EngineSave_JsonEscapeLabel
===============
Returns escaped length on success, -1 if label is invalid or out does not fit.
*/
static int EngineSave_JsonEscapeLabel( const char *in, char *out, int outSize ) {
	int o = 0;
	unsigned char c;

	if ( !in || !out || outSize < 2 ) {
		return -1;
	}
	while ( ( c = (unsigned char)*in++ ) != 0 ) {
		if ( c < 0x20u ) {
			return -1;
		}
		if ( c == '"' || c == '\\' ) {
			if ( o + 2 >= outSize ) {
				return -1;
			}
			out[o++] = '\\';
			out[o++] = (char)c;
		} else {
			if ( o + 1 >= outSize ) {
				return -1;
			}
			out[o++] = (char)c;
		}
	}
	out[o] = '\0';
	return o;
}

int EngineSave_ProtocolVersion( void ) {
	return ENGINE_SAVE_PROTOCOL_VERSION;
}

void EngineSave_Init( void ) {
	Com_Memset( s_saves, 0, sizeof( s_saves ) );
	s_lastSaveSlot = -1;
	EngineDB_Init();
	Com_Printf( "EngineSave: %d slots, protocol v%d (save/engine_slot_*.json)\n",
		SAVE_SLOTS, ENGINE_SAVE_PROTOCOL_VERSION );
}

void EngineSave_Shutdown( void ) {
	Com_Memset( s_saves, 0, sizeof( s_saves ) );
	s_lastSaveSlot = -1;
	EngineDB_Shutdown();
}

qboolean EngineSave_WriteSlot( int slot, const char *label ) {
	char path[MAX_OSPATH];
	char body[640];
	char escaped[256];
	fileHandle_t f;
	unsigned checksum;
	int bodyLen;

	if ( slot < 0 || slot >= SAVE_SLOTS || !label || !label[0] ) {
		return qfalse;
	}
	if ( EngineSave_JsonEscapeLabel( label, escaped, sizeof( escaped ) ) < 0 ) {
		Com_Printf( S_COLOR_YELLOW "EngineSave: rejected label (invalid JSON characters)\n" );
		return qfalse;
	}
	Q_strncpyz( s_saves[slot].label, label, sizeof( s_saves[0].label ) );
	s_saves[slot].used = qtrue;
	s_lastSaveSlot = slot;

	checksum = EngineSave_ChecksumLabel( label );
	EngineDB_SaveWriteSlot( slot, label, ENGINE_SAVE_PROTOCOL_VERSION, ENGINE_SAVE_MOD_VERSION, checksum );
	Com_sprintf( body, sizeof( body ),
		"{\n  \"protocolVersion\": %d,\n  \"modVersion\": \"%s\",\n  \"label\": \"%s\",\n  \"checksum\": %u\n}\n",
		ENGINE_SAVE_PROTOCOL_VERSION, ENGINE_SAVE_MOD_VERSION, escaped, checksum );
	bodyLen = (int)strlen( body );

	Com_sprintf( path, sizeof( path ), "save/engine_slot_%d.json", slot );
	f = FS_FOpenFileWrite( path );
	if ( f ) {
		FS_Write( body, bodyLen, f );
		FS_FCloseFile( f );
		Com_Printf( "EngineSave: wrote %s\n", path );
	}
	return qtrue;
}

static qboolean EngineSave_ParseJsonSlot( const char *text, char *labelOut, int labelLen, int *protoOut ) {
	const char *jsonEnd;
	const char *valueJson;
	unsigned storedCrc;
	unsigned labelCrc;

	if ( !text || !labelOut || labelLen < 1 ) {
		return qfalse;
	}
	labelOut[0] = '\0';
	if ( protoOut ) {
		*protoOut = 0;
	}

	jsonEnd = text + strlen( text );
	valueJson = JSON_ObjectGetNamedValue( text, jsonEnd, "protocolVersion" );
	if ( valueJson && protoOut ) {
		*protoOut = JSON_ValueGetInt( valueJson, jsonEnd );
	}

	valueJson = JSON_ObjectGetNamedValue( text, jsonEnd, "label" );
	if ( !valueJson || JSON_ValueGetString( valueJson, jsonEnd, labelOut, (unsigned)labelLen ) == 0 ) {
		return qfalse;
	}

	valueJson = JSON_ObjectGetNamedValue( text, jsonEnd, "checksum" );
	if ( !valueJson ) {
		return qtrue;
	}
	storedCrc = (unsigned)JSON_ValueGetInt( valueJson, jsonEnd );
	labelCrc = EngineSave_ChecksumLabel( labelOut );
	if ( storedCrc != labelCrc ) {
		Com_Printf( S_COLOR_YELLOW "EngineSave: checksum mismatch slot data\n" );
		return qfalse;
	}
	return qtrue;
}

qboolean EngineSave_ReadSlot( int slot, char *labelOut, int labelLen ) {
	char path[MAX_OSPATH];
	byte *buf;
	int len;
	int proto = 0;

	if ( slot < 0 || slot >= SAVE_SLOTS || !labelOut || labelLen < 1 ) {
		return qfalse;
	}
	labelOut[0] = '\0';

	if ( EngineDB_SaveReadSlot( slot, labelOut, labelLen, &proto ) ) {
		if ( proto != 0 && proto != ENGINE_SAVE_PROTOCOL_VERSION ) {
			Com_Printf( S_COLOR_YELLOW "EngineSave: sqlite protocol %d != engine %d\n",
				proto, ENGINE_SAVE_PROTOCOL_VERSION );
		}
		Q_strncpyz( s_saves[slot].label, labelOut, sizeof( s_saves[0].label ) );
		s_saves[slot].used = qtrue;
		return qtrue;
	}

	Com_sprintf( path, sizeof( path ), "save/engine_slot_%d.json", slot );
	len = FS_ReadFile( path, (void **)&buf );
	if ( len > 0 && buf ) {
		qboolean ok;

		buf[len] = '\0';
		ok = EngineSave_ParseJsonSlot( (const char *)buf, labelOut, labelLen, &proto );
		FS_FreeFile( buf );
		if ( ok ) {
			if ( proto != 0 && proto != ENGINE_SAVE_PROTOCOL_VERSION ) {
				Com_Printf( S_COLOR_YELLOW "EngineSave: protocol %d != engine %d\n",
					proto, ENGINE_SAVE_PROTOCOL_VERSION );
			}
			Q_strncpyz( s_saves[slot].label, labelOut, sizeof( s_saves[0].label ) );
			s_saves[slot].used = qtrue;
			return qtrue;
		}
	}

	Com_sprintf( path, sizeof( path ), "save/engine_slot_%d.txt", slot );
	len = FS_ReadFile( path, (void **)&buf );
	if ( len > 0 && buf ) {
		if ( len >= labelLen ) {
			len = labelLen - 1;
		}
		Com_Memcpy( labelOut, buf, len );
		labelOut[len] = '\0';
		FS_FreeFile( buf );
		Q_strncpyz( s_saves[slot].label, labelOut, sizeof( s_saves[0].label ) );
		s_saves[slot].used = qtrue;
		return qtrue;
	}

	if ( s_saves[slot].used ) {
		Q_strncpyz( labelOut, s_saves[slot].label, labelLen );
		return qtrue;
	}
	return qfalse;
}

int EngineSave_LastSlot( void ) {
	return s_lastSaveSlot;
}

qboolean EngineProfile_Set( const char *key, const char *value ) {
	return EngineDB_ProfileSet( key, value );
}

qboolean EngineProfile_Get( const char *key, char *out, int outSize ) {
	return EngineDB_ProfileGet( key, out, outSize );
}

qboolean EngineProfile_Delete( const char *key ) {
	return EngineDB_ProfileDelete( key );
}

qboolean EngineDatabase_IsAvailable( void ) {
	return EngineDB_IsAvailable();
}

const char *EngineDatabase_GetPath( void ) {
	return EngineDB_GetPath();
}

qboolean EngineDatabase_Exec( const char *sql ) {
	return EngineDB_Exec( sql );
}

qboolean EngineDatabase_QueryOne( const char *sql, char *out, int outSize ) {
	return EngineDB_QueryOne( sql, out, outSize );
}

/* ---- Quests ---- */

void EngineQuest_Init( void ) {
	s_questCount = 0;
	Com_Memset( s_quests, 0, sizeof( s_quests ) );
	Com_Printf( "EngineQuest: module ready (Engine.Quest.*)\n" );
}

void EngineQuest_Shutdown( void ) {
	s_questCount = 0;
}

int EngineQuest_Add( const char *id, const char *title, const char *stage ) {
	int i;
	if ( !id || !id[0] ) {
		return -1;
	}
	for ( i = 0; i < s_questCount; i++ ) {
		if ( s_quests[i].used && !Q_stricmp( s_quests[i].id, id ) ) {
			if ( title ) {
				Q_strncpyz( s_quests[i].title, title, sizeof( s_quests[0].title ) );
			}
			if ( stage ) {
				Q_strncpyz( s_quests[i].stage, stage, sizeof( s_quests[0].stage ) );
			}
			return i;
		}
	}
	if ( s_questCount >= QUEST_MAX ) {
		return -1;
	}
	i = s_questCount++;
	s_quests[i].used = qtrue;
	Q_strncpyz( s_quests[i].id, id, sizeof( s_quests[0].id ) );
	Q_strncpyz( s_quests[i].title, title ? title : "", sizeof( s_quests[0].title ) );
	Q_strncpyz( s_quests[i].stage, stage ? stage : "active", sizeof( s_quests[0].stage ) );
	return i;
}

qboolean EngineQuest_SetStage( const char *id, const char *stage ) {
	int i;
	if ( !id || !stage ) {
		return qfalse;
	}
	for ( i = 0; i < s_questCount; i++ ) {
		if ( s_quests[i].used && !Q_stricmp( s_quests[i].id, id ) ) {
			Q_strncpyz( s_quests[i].stage, stage, sizeof( s_quests[0].stage ) );
			return qtrue;
		}
	}
	return qfalse;
}

const char *EngineQuest_GetStage( const char *id ) {
	int i;
	if ( !id ) {
		return "";
	}
	for ( i = 0; i < s_questCount; i++ ) {
		if ( s_quests[i].used && !Q_stricmp( s_quests[i].id, id ) ) {
			return s_quests[i].stage;
		}
	}
	return "";
}

int EngineQuest_Count( void ) {
	return s_questCount;
}

/* ---- Dialogue ---- */

void EngineDialogue_Init( void ) {
	s_dialogueCount = 0;
	Com_Memset( s_dialogue, 0, sizeof( s_dialogue ) );
	cv_com_ubertools = Cvar_Get( "com_ubertools", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cv_com_ubertools, "Master runtime toggle for ÜberTools clean-room features." );
	Com_Printf( "EngineDialogue: buffer ready (Engine.Dialogue.*) com_ubertools=%d\n",
		cv_com_ubertools ? cv_com_ubertools->integer : 1 );
}

void EngineDialogue_Shutdown( void ) {
	s_dialogueCount = 0;
}

int EngineDialogue_Start( const char *speaker, const char *text ) {
	return EngineDialogue_StartEx( speaker, text, NULL, NULL, 0.0f );
}

int EngineDialogue_StartEx( const char *speaker, const char *text, const char *locKey,
	const char *voice, float duration ) {
	int i;
	if ( !text ) {
		return -1;
	}
	if ( s_dialogueCount >= DIALOGUE_MAX ) {
		memmove( s_dialogue, s_dialogue + 1, sizeof( s_dialogue[0] ) * (DIALOGUE_MAX - 1) );
		s_dialogueCount = DIALOGUE_MAX - 1;
	}
	i = s_dialogueCount++;
	Com_Memset( &s_dialogue[i], 0, sizeof( s_dialogue[i] ) );
	s_dialogue[i].used = qtrue;
	Q_strncpyz( s_dialogue[i].speaker, speaker ? speaker : "", sizeof( s_dialogue[0].speaker ) );
	Q_strncpyz( s_dialogue[i].text, text, sizeof( s_dialogue[0].text ) );
	if ( locKey ) {
		Q_strncpyz( s_dialogue[i].locKey, locKey, sizeof( s_dialogue[0].locKey ) );
	}
	if ( voice ) {
		Q_strncpyz( s_dialogue[i].voice, voice, sizeof( s_dialogue[0].voice ) );
	}
	s_dialogue[i].duration = duration;
	return i;
}

qboolean EngineDialogue_AddChoice( int lineIndex, const char *label, const char *nextId ) {
	dialogueLine_t *line;
	int c;
	if ( lineIndex < 0 || lineIndex >= s_dialogueCount || !label ) {
		return qfalse;
	}
	line = &s_dialogue[lineIndex];
	if ( line->choiceCount >= DIALOGUE_CHOICE_MAX ) {
		return qfalse;
	}
	c = line->choiceCount++;
	Q_strncpyz( line->choiceLabel[c], label, sizeof( line->choiceLabel[0] ) );
	Q_strncpyz( line->choiceNext[c], nextId ? nextId : "", sizeof( line->choiceNext[0] ) );
	return qtrue;
}

qboolean EngineDialogue_Get( int index, char *speakerOut, int speakerSize,
	char *textOut, int textSize, char *locKeyOut, int locKeySize,
	float *durationOut, int *choiceCountOut ) {
	if ( index < 0 || index >= s_dialogueCount || !s_dialogue[index].used ) {
		return qfalse;
	}
	if ( speakerOut && speakerSize > 0 ) {
		Q_strncpyz( speakerOut, s_dialogue[index].speaker, speakerSize );
	}
	if ( textOut && textSize > 0 ) {
		Q_strncpyz( textOut, s_dialogue[index].text, textSize );
	}
	if ( locKeyOut && locKeySize > 0 ) {
		Q_strncpyz( locKeyOut, s_dialogue[index].locKey, locKeySize );
	}
	if ( durationOut ) {
		*durationOut = s_dialogue[index].duration;
	}
	if ( choiceCountOut ) {
		*choiceCountOut = s_dialogue[index].choiceCount;
	}
	return qtrue;
}

qboolean EngineDialogue_GetChoice( int lineIndex, int choiceIndex,
	char *labelOut, int labelSize, char *nextOut, int nextSize ) {
	if ( lineIndex < 0 || lineIndex >= s_dialogueCount ) {
		return qfalse;
	}
	if ( choiceIndex < 0 || choiceIndex >= s_dialogue[lineIndex].choiceCount ) {
		return qfalse;
	}
	if ( labelOut && labelSize > 0 ) {
		Q_strncpyz( labelOut, s_dialogue[lineIndex].choiceLabel[choiceIndex], labelSize );
	}
	if ( nextOut && nextSize > 0 ) {
		Q_strncpyz( nextOut, s_dialogue[lineIndex].choiceNext[choiceIndex], nextSize );
	}
	return qtrue;
}

void EngineDialogue_Clear( void ) {
	s_dialogueCount = 0;
	Com_Memset( s_dialogue, 0, sizeof( s_dialogue ) );
}

int EngineDialogue_ActiveCount( void ) {
	return s_dialogueCount;
}
