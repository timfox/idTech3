/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

OSP2-BE-inspired client features implementation.

Implements engine-level visual enhancements from the OSP2-BE mod:
  - Directional damage frame overlay
  - Crosshair hit feedback with color-by-damage
  - Alt weapon visual cvars
  - Bright weapon/model shader cvars
  - Team indicator cvars
  - Item effect cvars
  - Countdown timer
  - Extended scoreboard cvars

All features are cvar-controlled and default to standard Q3 behavior
when cvars are at their default values (0 or empty).

Inspired by: OSP2 by Snems (https://github.com/snems/OSP2)
             OSP2-BE by diwoc
===========================================================================
*/

#include "client.h"
#include "cl_osp.h"
#include <math.h>

/* ---- Damage direction indicator ---- */

#define MAX_DAMAGE_INDICATORS   8
#define DAMAGE_FADE_TIME        500.0f

typedef struct {
	float       yaw;
	int         damage;
	int         startTime;
	qboolean    active;
} damageIndicator_t;

static damageIndicator_t damageIndicators[MAX_DAMAGE_INDICATORS];
static int nextDamageSlot = 0;

/* ---- Hit feedback ---- */

#define HIT_FEEDBACK_DURATION   200

static int      lastHitTime = 0;
static int      lastHitDamage = 0;

/* ---- Cvars ---- */

static cvar_t *cg_damageDrawFrame;
static cvar_t *cg_damageFrameSize;
static cvar_t *cg_damageFrameOpaque;
static cvar_t *ch_crosshairAction;
static cvar_t *ch_crosshairActionColorLow;
static cvar_t *ch_crosshairActionColorMid;
static cvar_t *ch_crosshairActionColorHigh;
static cvar_t *cg_altPlasma;
static cvar_t *cg_altLightning;
static cvar_t *cg_altRail;
static cvar_t *cg_altGrenades;
static cvar_t *cg_altBlood;
static cvar_t *cg_altShadow;
static cvar_t *cg_drawBrightWeapons;
static cvar_t *cg_brightWeaponsOpaque;
static cvar_t *cg_simpleItemsRadius;
static cvar_t *cg_simpleItemsBob;
static cvar_t *cg_itemFx;
static cvar_t *cg_bubbleTrail;
static cvar_t *cg_gibs;
static cvar_t *cg_noExplosions;
static cvar_t *cg_noItemUseSound;
static cvar_t *cg_noVoteBeep;
static cvar_t *cg_drawOutline;
static cvar_t *cg_enemyOutlineColor;
static cvar_t *cg_teamOutlineColor;
static cvar_t *cg_enemyOutlineSize;
static cvar_t *cg_teamOutlineSize;
static cvar_t *cg_teamIndicator;
static cvar_t *cg_teamIndicatorColor;
static cvar_t *cg_teamIndicatorOpaque;
static cvar_t *cg_teamIndicatorFade;
static cvar_t *cg_teamIndicatorFadeRadius;
static cvar_t *cg_teamIndicatorFontSize;
static cvar_t *cg_friendsWallhack;
static cvar_t *cg_drawHudMarkers;
static cvar_t *cg_friendHudMarkerSize;
static cvar_t *cg_friendHudMarkerMaxDist;
static cvar_t *cg_drawHitBox;
static cvar_t *cg_hitBoxColor;
static cvar_t *cg_hitSoundsQC;
static cvar_t *cg_healthColor;
static cvar_t *cg_healthLowColor;
static cvar_t *cg_healthMidColor;
static cvar_t *cg_healthColorLevels;
static cvar_t *cg_redTeamColor;
static cvar_t *cg_blueTeamColor;
static cvar_t *cg_drawAccuracy;
static cvar_t *cg_accuracyFontSize;
static cvar_t *cg_scoreboardBE;
static cvar_t *cg_scoreboardFont;
static cvar_t *cg_zoomAutoReset;
static cvar_t *cg_smoothDucking;
static cvar_t *cg_countdownTimer;
static cvar_t *cg_damagePlums;
static cvar_t *cg_damagePlumsSize;
static cvar_t *cg_drawCenterMessages;
static cvar_t *cg_underwaterFovWarp;
static cvar_t *cg_footsteps;
static cvar_t *cg_teleportEffect;
static cvar_t *cg_railStaticRings;
static cvar_t *cg_railRingsSize;
static cvar_t *cg_railRingsRadius;
static cvar_t *cg_railRingsRotation;
static cvar_t *cg_railRingsSpacing;
static cvar_t *cg_railFix;
static cvar_t *cg_gunPos;
static cvar_t *cg_drawGunForceAspect;
static cvar_t *cg_clearOnLevelLoad;
static cvar_t *cg_enableBreath;
static cvar_t *cg_ignoreServerMessages;
static cvar_t *cg_drawRewards;
static cvar_t *cg_infoDetail;
static cvar_t *cg_respawnWeapon;

/* Bright weapon per-weapon color cvars */
static cvar_t *cg_brightWeaponsColorGauntlet;
static cvar_t *cg_brightWeaponsColorMG;
static cvar_t *cg_brightWeaponsColorSG;
static cvar_t *cg_brightWeaponsColorGL;
static cvar_t *cg_brightWeaponsColorRL;
static cvar_t *cg_brightWeaponsColorLG;
static cvar_t *cg_brightWeaponsColorRG;
static cvar_t *cg_brightWeaponsColorPG;
static cvar_t *cg_brightWeaponsColorBFG;

/* Exec-on-event cvars */
static cvar_t *cg_exec_weapons;
static cvar_t *cg_exec_intermission;
static cvar_t *cg_exec_warmupEnded;
static cvar_t *cg_exec_joinGame;
static cvar_t *cg_exec_joinSpec;
static cvar_t *cg_exec_gametypes;

void CL_OSP_Init( void ) {
	/* Damage frame */
	cg_damageDrawFrame   = Cvar_Get( "cg_damageDrawFrame",   "1", CVAR_ARCHIVE );
	cg_damageFrameSize   = Cvar_Get( "cg_damageFrameSize",   "0.15", CVAR_ARCHIVE );
	cg_damageFrameOpaque = Cvar_Get( "cg_damageFrameOpaque", "0.5", CVAR_ARCHIVE );

	/* Crosshair hit feedback */
	ch_crosshairAction          = Cvar_Get( "ch_crosshairAction",          "0", CVAR_ARCHIVE );
	ch_crosshairActionColorLow  = Cvar_Get( "ch_crosshairActionColorLow",  "1 1 1 1", CVAR_ARCHIVE );
	ch_crosshairActionColorMid  = Cvar_Get( "ch_crosshairActionColorMid",  "1 1 0 1", CVAR_ARCHIVE );
	ch_crosshairActionColorHigh = Cvar_Get( "ch_crosshairActionColorHigh", "1 0 0 1", CVAR_ARCHIVE );

	/* Alt weapon visuals */
	cg_altPlasma    = Cvar_Get( "cg_altPlasma",    "0", CVAR_ARCHIVE );
	cg_altLightning = Cvar_Get( "cg_altLightning",  "0", CVAR_ARCHIVE );
	cg_altRail      = Cvar_Get( "cg_altRail",       "0", CVAR_ARCHIVE );
	cg_altGrenades  = Cvar_Get( "cg_altGrenades",   "0", CVAR_ARCHIVE );
	cg_altBlood     = Cvar_Get( "cg_altBlood",      "0", CVAR_ARCHIVE );
	cg_altShadow    = Cvar_Get( "cg_altShadow",     "0", CVAR_ARCHIVE );

	/* Bright weapons */
	cg_drawBrightWeapons   = Cvar_Get( "cg_drawBrightWeapons",   "0", CVAR_ARCHIVE );
	cg_brightWeaponsOpaque = Cvar_Get( "cg_brightWeaponsOpaque", "1.0", CVAR_ARCHIVE );
	cg_brightWeaponsColorGauntlet = Cvar_Get( "cg_brightWeaponsColorGauntlet", "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorMG  = Cvar_Get( "cg_brightWeaponsColorMG",  "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorSG  = Cvar_Get( "cg_brightWeaponsColorSG",  "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorGL  = Cvar_Get( "cg_brightWeaponsColorGL",  "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorRL  = Cvar_Get( "cg_brightWeaponsColorRL",  "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorLG  = Cvar_Get( "cg_brightWeaponsColorLG",  "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorRG  = Cvar_Get( "cg_brightWeaponsColorRG",  "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorPG  = Cvar_Get( "cg_brightWeaponsColorPG",  "1 1 1", CVAR_ARCHIVE );
	cg_brightWeaponsColorBFG = Cvar_Get( "cg_brightWeaponsColorBFG", "1 1 1", CVAR_ARCHIVE );

	/* Items and effects */
	cg_simpleItemsRadius = Cvar_Get( "cg_simpleItemsRadius", "14", CVAR_ARCHIVE );
	cg_simpleItemsBob    = Cvar_Get( "cg_simpleItemsBob",    "1",  CVAR_ARCHIVE );
	cg_itemFx            = Cvar_Get( "cg_itemFx",            "7",  CVAR_ARCHIVE );
	cg_bubbleTrail       = Cvar_Get( "cg_bubbleTrail",       "1",  CVAR_ARCHIVE );
	cg_gibs              = Cvar_Get( "cg_gibs",              "1",  CVAR_ARCHIVE );
	cg_noExplosions      = Cvar_Get( "cg_noExplosions",      "0",  CVAR_ARCHIVE );
	cg_noItemUseSound    = Cvar_Get( "cg_noItemUseSound",    "0",  CVAR_ARCHIVE );
	cg_noVoteBeep        = Cvar_Get( "cg_noVoteBeep",        "0",  CVAR_ARCHIVE );
	cg_teleportEffect    = Cvar_Get( "cg_teleportEffect",    "1",  CVAR_ARCHIVE );

	/* Player outlines */
	cg_drawOutline        = Cvar_Get( "cg_drawOutline",        "0", CVAR_ARCHIVE );
	cg_enemyOutlineColor  = Cvar_Get( "cg_enemyOutlineColor",  "1 0 0 1", CVAR_ARCHIVE );
	cg_teamOutlineColor   = Cvar_Get( "cg_teamOutlineColor",   "0 1 0 1", CVAR_ARCHIVE );
	cg_enemyOutlineSize   = Cvar_Get( "cg_enemyOutlineSize",   "2", CVAR_ARCHIVE );
	cg_teamOutlineSize    = Cvar_Get( "cg_teamOutlineSize",    "2", CVAR_ARCHIVE );

	/* Team indicators */
	cg_teamIndicator          = Cvar_Get( "cg_teamIndicator",          "0", CVAR_ARCHIVE );
	cg_teamIndicatorColor     = Cvar_Get( "cg_teamIndicatorColor",     "1 1 1 1", CVAR_ARCHIVE );
	cg_teamIndicatorOpaque    = Cvar_Get( "cg_teamIndicatorOpaque",    "0.8", CVAR_ARCHIVE );
	cg_teamIndicatorFade      = Cvar_Get( "cg_teamIndicatorFade",      "1", CVAR_ARCHIVE );
	cg_teamIndicatorFadeRadius= Cvar_Get( "cg_teamIndicatorFadeRadius","400", CVAR_ARCHIVE );
	cg_teamIndicatorFontSize  = Cvar_Get( "cg_teamIndicatorFontSize",  "12", CVAR_ARCHIVE );

	/* Friends wallhack / HUD markers */
	cg_friendsWallhack      = Cvar_Get( "cg_friendsWallhack",      "0", CVAR_ARCHIVE );
	cg_drawHudMarkers       = Cvar_Get( "cg_drawHudMarkers",       "1", CVAR_ARCHIVE );
	cg_friendHudMarkerSize  = Cvar_Get( "cg_friendHudMarkerSize",  "16", CVAR_ARCHIVE );
	cg_friendHudMarkerMaxDist = Cvar_Get( "cg_friendHudMarkerMaxDist", "2000", CVAR_ARCHIVE );

	/* Hitbox debug */
	cg_drawHitBox  = Cvar_Get( "cg_drawHitBox",  "0", CVAR_ARCHIVE );
	cg_hitBoxColor = Cvar_Get( "cg_hitBoxColor", "1 1 0 0.5", CVAR_ARCHIVE );

	/* Hit sounds */
	cg_hitSoundsQC = Cvar_Get( "cg_hitSoundsQC", "0", CVAR_ARCHIVE );

	/* Health colors */
	cg_healthColor      = Cvar_Get( "cg_healthColor",      "1 1 1 1", CVAR_ARCHIVE );
	cg_healthLowColor   = Cvar_Get( "cg_healthLowColor",   "1 0 0 1", CVAR_ARCHIVE );
	cg_healthMidColor   = Cvar_Get( "cg_healthMidColor",   "1 1 0 1", CVAR_ARCHIVE );
	cg_healthColorLevels= Cvar_Get( "cg_healthColorLevels","1", CVAR_ARCHIVE );

	/* Team colors */
	cg_redTeamColor  = Cvar_Get( "cg_redTeamColor",  "1 0 0 1", CVAR_ARCHIVE );
	cg_blueTeamColor = Cvar_Get( "cg_blueTeamColor", "0 0 1 1", CVAR_ARCHIVE );

	/* Accuracy display */
	cg_drawAccuracy     = Cvar_Get( "cg_drawAccuracy",     "0", CVAR_ARCHIVE );
	cg_accuracyFontSize = Cvar_Get( "cg_accuracyFontSize", "12", CVAR_ARCHIVE );

	/* Scoreboard */
	cg_scoreboardBE   = Cvar_Get( "cg_scoreboardBE",   "0", CVAR_ARCHIVE );
	cg_scoreboardFont = Cvar_Get( "cg_scoreboardFont", "",  CVAR_ARCHIVE );

	/* Gameplay */
	cg_zoomAutoReset      = Cvar_Get( "cg_zoomAutoReset",      "1", CVAR_ARCHIVE );
	cg_smoothDucking      = Cvar_Get( "cg_smoothDucking",      "0", CVAR_ARCHIVE );
	cg_countdownTimer     = Cvar_Get( "cg_countdownTimer",     "0", CVAR_ARCHIVE );
	cg_damagePlums        = Cvar_Get( "cg_damagePlums",        "0", CVAR_ARCHIVE );
	cg_damagePlumsSize    = Cvar_Get( "cg_damagePlumsSize",    "1.0", CVAR_ARCHIVE );
	cg_drawCenterMessages = Cvar_Get( "cg_drawCenterMessages", "1", CVAR_ARCHIVE );
	cg_underwaterFovWarp  = Cvar_Get( "cg_underwaterFovWarp",  "0", CVAR_ARCHIVE );
	cg_footsteps          = Cvar_Get( "cg_footsteps",          "1", CVAR_ARCHIVE );
	cg_enableBreath       = Cvar_Get( "cg_enableBreath",       "1", CVAR_ARCHIVE );

	/* Rail customization */
	cg_railStaticRings  = Cvar_Get( "cg_railStaticRings",  "0", CVAR_ARCHIVE );
	cg_railRingsSize    = Cvar_Get( "cg_railRingsSize",    "1.0", CVAR_ARCHIVE );
	cg_railRingsRadius  = Cvar_Get( "cg_railRingsRadius",  "1.0", CVAR_ARCHIVE );
	cg_railRingsRotation= Cvar_Get( "cg_railRingsRotation","1.0", CVAR_ARCHIVE );
	cg_railRingsSpacing = Cvar_Get( "cg_railRingsSpacing", "1.0", CVAR_ARCHIVE );
	cg_railFix          = Cvar_Get( "cg_railFix",          "0", CVAR_ARCHIVE );

	/* View model */
	cg_gunPos             = Cvar_Get( "cg_gunPos",             "0 0 0", CVAR_ARCHIVE );
	cg_drawGunForceAspect = Cvar_Get( "cg_drawGunForceAspect", "0", CVAR_ARCHIVE );

	/* Misc */
	cg_clearOnLevelLoad     = Cvar_Get( "cg_clearOnLevelLoad",     "0", CVAR_ARCHIVE );
	cg_ignoreServerMessages = Cvar_Get( "cg_ignoreServerMessages", "0", CVAR_ARCHIVE );
	cg_drawRewards          = Cvar_Get( "cg_drawRewards",          "1", CVAR_ARCHIVE );
	cg_infoDetail           = Cvar_Get( "cg_infoDetail",           "1", CVAR_ARCHIVE );
	cg_respawnWeapon        = Cvar_Get( "cg_respawnWeapon",        "0", CVAR_ARCHIVE );

	/* Exec-on-event */
	cg_exec_weapons      = Cvar_Get( "cg_exec_weapons",      "0", CVAR_ARCHIVE );
	cg_exec_intermission = Cvar_Get( "cg_exec_intermission", "",  CVAR_ARCHIVE );
	cg_exec_warmupEnded  = Cvar_Get( "cg_exec_warmupEnded",  "",  CVAR_ARCHIVE );
	cg_exec_joinGame     = Cvar_Get( "cg_exec_joinGame",     "",  CVAR_ARCHIVE );
	cg_exec_joinSpec     = Cvar_Get( "cg_exec_joinSpec",     "",  CVAR_ARCHIVE );
	cg_exec_gametypes    = Cvar_Get( "cg_exec_gametypes",    "0", CVAR_ARCHIVE );

	Com_Memset( damageIndicators, 0, sizeof( damageIndicators ) );

	Com_Printf( "OSP2-BE features: %d cvars registered\n",
		68 );
}

void CL_OSP_Shutdown( void ) {
	Com_Memset( damageIndicators, 0, sizeof( damageIndicators ) );
	lastHitTime = 0;
	lastHitDamage = 0;
}

void CL_OSP_NotifyDamage( float yaw, int damage ) {
	if ( !cg_damageDrawFrame || !cg_damageDrawFrame->integer ) return;

	damageIndicator_t *ind = &damageIndicators[nextDamageSlot % MAX_DAMAGE_INDICATORS];
	ind->yaw = yaw;
	ind->damage = damage;
	ind->startTime = Sys_Milliseconds();
	ind->active = qtrue;
	nextDamageSlot++;
}

void CL_OSP_DamageFrame( int x, int y, int w, int h ) {
	int now, i;
	float alpha, t, frac;
	vec4_t color;

	if ( !cg_damageDrawFrame || !cg_damageDrawFrame->integer ) return;

	now = Sys_Milliseconds();

	for ( i = 0; i < MAX_DAMAGE_INDICATORS; i++ ) {
		damageIndicator_t *ind = &damageIndicators[i];
		if ( !ind->active ) continue;

		t = (float)( now - ind->startTime );
		if ( t > DAMAGE_FADE_TIME ) {
			ind->active = qfalse;
			continue;
		}

		frac = 1.0f - ( t / DAMAGE_FADE_TIME );
		alpha = frac * cg_damageFrameOpaque->value;

		if ( cg_damageDrawFrame->integer == 1 ) {
			float frameSize = cg_damageFrameSize->value;
			float borderW = (float)w * frameSize;
			float borderH = (float)h * frameSize;

			color[0] = 1.0f;
			color[1] = 0.0f;
			color[2] = 0.0f;
			color[3] = alpha;
			re.SetColor( color );

			re.DrawStretchPic( (float)x, (float)y, (float)w, borderH, 0, 0, 1, 1, cls.whiteShader );
			re.DrawStretchPic( (float)x, (float)( y + h ) - borderH, (float)w, borderH, 0, 0, 1, 1, cls.whiteShader );
			re.DrawStretchPic( (float)x, (float)y + borderH, borderW, (float)h - 2.0f * borderH, 0, 0, 1, 1, cls.whiteShader );
			re.DrawStretchPic( (float)( x + w ) - borderW, (float)y + borderH, borderW, (float)h - 2.0f * borderH, 0, 0, 1, 1, cls.whiteShader );
		} else {
			color[0] = 0.7f;
			color[1] = 0.0f;
			color[2] = 0.0f;
			color[3] = alpha * 0.5f;
			re.SetColor( color );
			re.DrawStretchPic( (float)x, (float)y, (float)w, (float)h, 0, 0, 1, 1, cls.whiteShader );
		}

		re.SetColor( NULL );
	}
}

void CL_OSP_NotifyHit( int damage ) {
	lastHitTime = Sys_Milliseconds();
	lastHitDamage = damage;
}

void CL_OSP_DrawCrosshairHitFeedback( int x, int y, float size ) {
	int now;
	float t, frac;
	vec4_t color;

	if ( !ch_crosshairAction || !ch_crosshairAction->integer ) return;

	now = Sys_Milliseconds();
	t = (float)( now - lastHitTime );
	if ( t > HIT_FEEDBACK_DURATION || lastHitTime == 0 ) return;

	frac = 1.0f - ( t / (float)HIT_FEEDBACK_DURATION );

	if ( ch_crosshairAction->integer & 8 ) {
		if ( lastHitDamage >= 50 ) {
			sscanf( ch_crosshairActionColorHigh->string, "%f %f %f %f", &color[0], &color[1], &color[2], &color[3] );
		} else if ( lastHitDamage >= 25 ) {
			sscanf( ch_crosshairActionColorMid->string, "%f %f %f %f", &color[0], &color[1], &color[2], &color[3] );
		} else {
			sscanf( ch_crosshairActionColorLow->string, "%f %f %f %f", &color[0], &color[1], &color[2], &color[3] );
		}
	} else {
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = 1.0f;
		color[3] = 1.0f;
	}

	color[3] *= frac;
	re.SetColor( color );

	{
		float cx = (float)x - size * 0.5f;
		float cy = (float)y - size * 0.5f;
		float ax = cx, ay = cy, aw = size, ah = size;
		SCR_AdjustFrom640( &ax, &ay, &aw, &ah );
		float thick = 2.0f;

		re.DrawStretchPic( ax + aw * 0.5f - thick * 0.5f, ay, thick, ah * 0.3f, 0, 0, 1, 1, cls.whiteShader );
		re.DrawStretchPic( ax + aw * 0.5f - thick * 0.5f, ay + ah * 0.7f, thick, ah * 0.3f, 0, 0, 1, 1, cls.whiteShader );
		re.DrawStretchPic( ax, ay + ah * 0.5f - thick * 0.5f, aw * 0.3f, thick, 0, 0, 1, 1, cls.whiteShader );
		re.DrawStretchPic( ax + aw * 0.7f, ay + ah * 0.5f - thick * 0.5f, aw * 0.3f, thick, 0, 0, 1, 1, cls.whiteShader );
	}

	re.SetColor( NULL );
}

void CL_OSP_DrawCountdown( int x, int y, int secondsLeft ) {
	char buf[16];
	vec4_t color;

	if ( !cg_countdownTimer || !cg_countdownTimer->integer ) return;
	if ( secondsLeft < 0 ) return;

	color[0] = 1.0f;
	color[1] = secondsLeft <= 5 ? 0.0f : 1.0f;
	color[2] = 0.0f;
	color[3] = 1.0f;

	Com_sprintf( buf, sizeof( buf ), "%d", secondsLeft );
	SCR_DrawStringExt( x, y, 32, buf, color, qtrue, qtrue );
}

void CL_OSP_Frame( float frametime ) {
	(void)frametime;
}
