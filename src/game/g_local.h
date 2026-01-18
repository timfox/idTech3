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
// g_local.h -- local definitions for game module
//

#include "../common/q_shared.h"
#include "bg_public.h"
#include "g_public.h"

//==================================================================

// the "gameversion" client command will print this plus compile date
#define	GAMEVERSION	"baseq3"

//==================================================================

//==================================================================

//
// entity->svFlags
// the server does not know how to interpret most of the values
// in entityStates (level eType), so the game must explicitly flag
// special server behaviors
//
#define	SVF_NOCLIENT			0x00000001	// don't send entity to clients, even if it has effects
#define	SVF_BOT					0x00000002	// set if the entity is a bot
#define	SVF_BROADCAST			0x00000020	// send to all connected clients
#define	SVF_PORTAL				0x00000040	// merge a second pvs at origin2 into snapshots
#define	SVF_USE_CURRENT_ORIGIN	0x00000080	// entity->r.currentOrigin instead of entity->s.origin
											// for link position (missiles and movers)
#define SVF_SINGLECLIENT		0x00000100	// only send to a single client (entityShared_t->singleClient)
#define SVF_NOSERVERINFO		0x00000200	// don't send CS_SERVERINFO updates to this client
											// so that it doesn't know about serverinfo changes
#define SVF_CAPSULE				0x00000400	// use capsule for collision detection instead of bbox
#define SVF_NOTSINGLECLIENT		0x00000800	// send entity to everyone but one client
											// (entityShared_t->singleClient)


//==================================================================

typedef enum {
	CON_DISCONNECTED,
	CON_CONNECTING,
	CON_CONNECTED
} clientConnected_t;

typedef enum {
	SPECTATOR_NOT,
	SPECTATOR_FREE,
	SPECTATOR_FOLLOW,
	SPECTATOR_SCOREBOARD
} spectatorState_t;

typedef enum {
	TEAM_BEGIN,		// Beginning a team game, spawn at base
	TEAM_ACTIVE		// Now actively playing
} playerTeamStateState_t;

typedef struct {
	playerTeamStateState_t	state;

	int			location;

	int			captures;
	int			basedefense;
	int			carrierdefense;
	int			flagrecovery;
	int			fragcarrier;
	int			assistscore;
	int			fragscore;
	int			suicides;

	int			numShots;
	int			numHits;
	int			numShotsAgainst;
	int			numHitsAgainst;
} playerTeamState_t;

// the auto following clients don't have a follow slot
#define	MAX_TOTAL_ENTITIES		1024		// can't be increased without changing drawsurf bit packing
#define	MAX_CLIENTS				128			// absolute limit

#define	WORLDSPAWN_COUNT		1

//============================================================================

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

struct gentity_s {
	entityState_t	s;				// communicated by server to clients
	entityShared_t	r;				// shared by both the server system and game

	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!
	//================================

	struct gclient_s	*client;			// NULL if not a client

	qboolean	inuse;

	char		*classname;			// set in QuakeEd
	int			spawnflags;			// set in QuakeEd

	qboolean	neverFree;			// if true, FreeEntity will only unlink
									// bodyque uses this

	int			flags;				// FL_* variables

	char		*model;
	char		*model2;
	int			freetime;			// level.time when the object was freed

	int			eventTime;			// events will be cleared EVENT_VALID_MSEC after set
	qboolean	freeAfterEvent;
	qboolean	unlinkAfterEvent;

	qboolean	physicsObject;		// if true, it can be pushed by movers and fall off edges
									// all game items are physicsObjects,
	float		physicsBounce;		// 1.0 = continuous bounce, 0.0 = no bounce
	int			clipmask;			// brushes with this content value will be collided against
									// when moving.  items and corpses do not collide against
									// players, for instance

	// movers
	moverState_t moverState;
	int			soundPos1;
	int			sound1to2;
	int			sound2to3;
	int			soundPos2;
	int			soundLoop;
	gentity_t	*parent;
	gentity_t	*nextTrain;
	gentity_t	*prevTrain;
	vec3_t		pos1, pos2;
	float		speed;
	float		lastSpeed;			// used by trains that have been restarted
	char		*message;
	char		*blocker;

	int			timestamp;		// body queue sinking, etc
	char		*target;
	char		*targetname;
	char		*team;
	char		*targetShaderName;
	char		*targetShaderNewName;
	gentity_t	*target_ent;

	float		speed_mod;
	float		speed_mod_time;

	char		*closetarget;
	gentity_t	*opentarget;
	gentity_t	*activator;
	gentity_t	*teamchain;		// next entity in team
	gentity_t	*teammaster;	// master of the team

	unsigned int	snapshotCallbackTime;

	char		*attach_target;
	gentity_t	*attach_target_ent;
	vec3_t		attach_offset;
	vec3_t		attach_angles;

	qboolean	trigger_protected;

	int			damage;
	int			splashDamage;
	int			splashRadius;

	int			methodOfDeath;

	int			splashMethodOfDeath;

	int			health;
	int			maxhealth;

	qboolean	takedamage;

	int			damage_debounce_time;
	int			damage_debounce_time2;
	int			last_move_time;

	int			pain_debounce_time;
	int			fly_sound_debounce_time;	// wind tunnel

	gentity_t	*chain;
	gentity_t	*enemy;
	gentity_t	*oldenemy;
	gentity_t	*activator;
	gentity_t	*teamchain;		// next entity in team
	gentity_t	*teammaster;	// master of the team

	int			watertype;
	int			waterlevel;

	int			count;			// items

	// timing variables
	float		wait;
	float		random;
	float		delay;

	// velocity
	vec3_t		thrown_weapon_velocity;

	gitem_t		*item;			// for bonus items

	// OSP
	qboolean	isOSPreward;
	int			OSPtype;

	// legacy compatibility
	int			ecsEntityId;	// ID for ECS integration
};

struct gclient_s {
	// ps MUST be the first element, because the server expects it
	playerState_t	ps;				// communicated by server to clients

	// the rest of the structure is private to game
	clientConnected_t	connected;
	usercmd_t	cmd;				// we would lose angles if not persistant
	qboolean	localClient;		// true if "ip" info key is "localhost"
	qboolean	initialSpawn;		// the first spawn should be at a cool location
	qboolean	predictItemPickup;	// based on cg_predictItems userinfo
	qboolean	pmoveFixed;			//
	char			ip[NET_ADDRSTR_LEN];		// ip address string

	int				areabits[MAX_MAP_AREA_BYTES/8];		// portalarea visibility bits

	playerTeamState_t teamState;				// status in teamplay games
	int				teamStateUpdate;			// time of last team state update

	int				numBinocZoomFov;
	int				binocZoomTime;
	qboolean		binocZoomOn;

	int				switchTeamTime;			// time the player switched teams

	int				score;			// total score
	int				kills;			// total kills
	int				deaths;			// total deaths
	int				damage_dealt;	// total damage dealt
	int				damage_received;	// total damage received
	int				suicides;		// total suicides
	int				teamkills;		// total teamkills

	int				ping;			// server ping

	int				lastKillTime;	// for multiple kill rewards

	qboolean		readyToExit;	// wishes to leave the intermission

	qboolean		noclip;

	int				lastCmdTime;	// level.time of last usercmd_t, for EF_CONNECTION
									// we can't just use pers.lastCommand.time, because
									// of the g_sycronousclients case
	int				buttons;
	int				oldbuttons;
	int				latched_buttons;

	vec3_t			oldOrigin;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int				damage_armor;		// damage absorbed by armor
	int				damage_blood;		// damage taken out of health
	int				damage_knockback;	// impact damage
	vec3_t			damage_from;		// origin for vector calculation
	qboolean		damage_fromWorld;	// if true, don't use the damage_from vector

	int				accurateCount;		// for "impressive" reward sound

	int				accuracy_shots;		// total number of shots
	int				accuracy_hits;		// total number of hits

	//
	int				lastkilled_client;	// last client that this client killed
	int				lasthurt_client;	// last client that damaged this client
	int				lasthurt_mod;		// type of damage the client did

	// timers
	int				respawnTime;		// can respawn when time > this, force after g_forcerespawn
	int				inactivityTime;		// kick players when time > this
	qboolean		inactivityWarning;	// qtrue if the five second warning has been given
	int				rewardTime;			// clear the EF_AWARD_IMPRESSIVE, etc when time > this

	int				airOutTime;

	int				lastKillTime;		// for multiple kill rewards

	qboolean		fireHeld;			// used for hook
	gentity_t		*hook;				// grapple hook if out

	int				switchTeamTime;		// time the player switched teams

	// timeResidual is used to handle events that happen every second
	// like health / armor countdowns and regeneration
	int				timeResidual;

#ifdef MISSIONPACK
	gclient_t		*coach;
#endif

	char			*areabits;
};

//
// this structure is cleared as each map is entered
//
#define	MAX_SPAWN_VARS			64
#define	MAX_SPAWN_VARS_CHARS	4096

typedef struct {
	struct gclient_s	*clients;		// [maxclients]

	struct gentity_s	*gentities;
	int				gentitySize;
	int				num_entities;		// current number, <= MAX_GENTITIES

	int				warmupTime;			// restart match at this time

	fileHandle_t	logFile;

	// store latched cvars here that we want to get at often
	int				maxclients;

	int				framenum;
	int				time;					// in msec
	int				previousTime;			// so movers can back up when blocked

	int				startTime;				// level.time the map was started

	int				teamScores[TEAM_NUM_TEAMS];
	int				lastTeamLocationTime;		// last time of client team location update

	qboolean		newSession;			// don't use any old session data, because
											// we changed gametype

	qboolean		restarted;			// waiting for a map_restart to fire

	int				numConnectedClients;
	int				numNonSpectatorClients;		// includes connecting clients
	int				numPlayingClients;			// connected, not spec, and not waiting for restart
	int				sortedClients[MAX_CLIENTS];		// sorted by score
	int				follow1, follow2;		// clientNums for auto-follow spectators

	int				snd_fry;				// damped sound for standing in lava

	// voting state
	char			voteString[MAX_STRING_CHARS];
	char			voteDisplayString[MAX_STRING_CHARS];
	int				voteTime;				// level.time vote was called
	int				voteExecuteTime;		// when it is supposed to be executed
	int				voteYes;
	int				voteNo;
	int				numVotingClients;		// set by CalculateRanks

	// team voting state
	char			teamVoteString[2][MAX_STRING_CHARS];
	int				teamVoteTime[2];		// level.time vote was called
	int				teamVoteYes[2];
	int				teamVoteNo[2];
	int				numteamVotingClients[2];	// set by CalculateRanks

	// spawn variables
	qboolean		spawning;				// the G_Spawn*() functions are valid
	int				numSpawnVars;
	char			*spawnVars[MAX_SPAWN_VARS][2];	// key / value pairs
	int				numSpawnVarChars;
	char			spawnVarChars[MAX_SPAWN_VARS_CHARS];

	// intermission state
	int				intermissionQueued;		// intermission was qualified, but
											// wait INTERMISSION_DELAY_TIME before
											// actually going there so the last
											// frag can be watched.  Disable future
											// kills during this delay
	int				intermissiontime;		// time the intermission was started
	char			*changemap;
	qboolean		readyToExit;			// at least one client wants to exit
	int				exitTime;
	vec3_t			intermission_origin;	// also used for spectator spawns
	vec3_t			intermission_angle;

	qboolean		locationLinked;			// target_locations get linked
	gentity_t		*locationHead;			// head of the location list
	int				bodyQueIndex;			// dead bodies
	gentity_t		*bodyQue[BODY_QUEUE_SIZE];

	int				portalSequence;
} level_locals_t;


//
// g_spawn.c
//
qboolean	G_SpawnString( const char *key, const char *defaultString, char **out );
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
qboolean	G_SpawnFloat( const char *key, const char *defaultString, float *out );
qboolean	G_SpawnInt( const char *key, const char *defaultString, int *out );
qboolean	G_SpawnVector( const char *key, const char *defaultString, float *out );
void		G_SpawnEntitiesFromString( void );

//
// g_cmds.c
//
void Cmd_Score_f (gentity_t *ent);
char *ConcatArgs( int start );

//
// g_items.c
//
void G_CheckTeamItems( void );
void G_RunItem( gentity_t *ent );
void RespawnItem( gentity_t *ent );

void UseHoldableItem( gentity_t *ent );
void PrecacheItem (gitem_t *it);
gentity_t *Drop_Item( gentity_t *ent, gitem_t *item, float angle );
gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity );
void G_SpawnItem (gentity_t *ent, gitem_t *item);
void FinishSpawningItem( gentity_t *ent );
void Think_Weapon (gentity_t *ent);
int ArmorIndex (gentity_t *ent);
void	Add_Armor (gentity_t *ent, int amount);
void Touch_Item (gentity_t *ent, gentity_t *other, trace_t *trace);

//
// g_utils.c
//
int G_ModelIndex( char *name );
int G_SoundIndex( char *name );
void G_TeamCommand( team_t team, char *cmd );
void G_KillBox (gentity_t *ent);
gentity_t *G_Find (gentity_t *from, int fieldofs, const char *match);
gentity_t *G_PickTarget (char *targetname);
void G_UseTargets (gentity_t *ent, gentity_t *activator);
void G_SetMovedir ( vec3_t angles, vec3_t movedir);

void G_InitGentity( gentity_t *e );
gentity_t *G_Spawn (void);
gentity_t *G_TempEntity( vec3_t origin, int event );
void G_Sound( gentity_t *ent, int channel, int soundIndex );
void G_FreeEntity( gentity_t *e );
void G_TouchTriggers (gentity_t *ent);

float	*tv( float x, float y, float z );
char	*vtos( const vec3_t v );

float vectoyaw( const vec3_t vec );

void G_AddPredictableEvent( gentity_t *ent, int event, int eventParm );
void G_AddEvent( gentity_t *ent, int event, int eventParm );
void G_SetOrigin( gentity_t *ent, vec3_t origin );
void G_SetAngle( gentity_t *ent, vec3_t angle );

qboolean G_admin_permission (gentity_t *ent, const char *flag);

//
// g_combat.c
//
qboolean CanDamage (gentity_t *targ, vec3_t origin);
void G_Damage (gentity_t *targ, gentity_t *inflictor, gentity_t *attacker, vec3_t dir, vec3_t point, int damage, int dflags, int mod);
qboolean G_RadiusDamage (vec3_t origin, gentity_t *attacker, float damage, float radius, gentity_t *ignore, int mod);
int G_InvulnerabilityEffect( gentity_t *targ );
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath );

//
// g_missile.c
//
void G_RunMissile( gentity_t *ent );

gentity_t *fire_plasma (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_grenade (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_rocket (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_bfg (gentity_t *self, vec3_t start, vec3_t aimdir);
gentity_t *fire_grapple (gentity_t *self, vec3_t start, vec3_t aimdir);


//
// g_mover.c
//
void G_RunMover( gentity_t *ent );
void Touch_DoorTrigger( gentity_t *ent, gentity_t *other, trace_t *trace );

//
// g_trigger.c
//
void trigger_teleporter_touch (gentity_t *self, gentity_t *other, trace_t *trace );


//
// g_misc.c
//
void TeleportPlayer( gentity_t *player, vec3_t origin, vec3_t angles );

//
// g_weapon.c
//
qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker );
void CalcMuzzlePoint ( gentity_t *ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint );

//
// g_client.c
//
void ClientRespawn(gentity_t *ent);
void BeginIntermission (void);
void InitClientPersistant (gclient_t *client);
void InitClientResp (gclient_t *client);
void InitBodyQue (void);
void ClientSpawn( gentity_t *ent );
void player_die (gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod);
void AddScore( gentity_t *ent, vec3_t origin, int score );
void CalculateRanks( void );
qboolean SpotWouldTelefrag( gentity_t *spot );

//
// g_svcmds.c
//
qboolean ConsoleCommand( void );
void G_ProcessIPBans(void);
qboolean G_FilterPacket (char *from);

//
// g_weapon.c
//
void FireWeapon( gentity_t *ent );
void G_StartKamikaze( gentity_t *ent );

#ifdef MISSIONPACK

//
// g_weapon.c
//
void G_StartWeaponVis( gentity_t *ent );

#endif

extern	level_locals_t	level;
extern	gentity_t		g_entities[MAX_GENTITIES];

#define	FOFS(x) ((size_t)&(((gentity_t *)0)->x))