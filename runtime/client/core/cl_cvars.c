/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_cvars.h"

cvar_t	*cl_noprint;
cvar_t	*cl_motd;
cvar_t	*rcon_client_password;
cvar_t	*rconAddress;
cvar_t	*cl_timeout;
cvar_t	*cl_autoNudge;
cvar_t	*cl_autoGraphicsProfile;
cvar_t	*cl_preferModernGraphics;
cvar_t	*cl_timeNudge;
cvar_t	*cl_showTimeDelta;
cvar_t	*cl_shownet;
cvar_t	*cl_activeAction;
cvar_t	*cl_motdString;
cvar_t	*cl_conXOffset;
cvar_t	*cl_conColor;
cvar_t	*cl_inGameVideo;
cvar_t	*cl_lanForcePackets;
cvar_t	*cl_guidServerUniq;
cvar_t	*cl_reconnectArgs;

void CL_InitCvars( void )
{
	cvar_t *cv;

	cl_noprint = Cvar_Get( "cl_noprint", "0", 0 );
	Cvar_SetDescription( cl_noprint, "Disable printing of information in the console." );

	cv = Cvar_Get( "cl_jsEscapeMenu", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cv, "Route the in-game escape menu to the JavaScript overlay (ui_rpMenu pointer mode) instead of the native UI module." );
	cv = Cvar_Get( "ui_rpMenu", "0", CVAR_TEMP );
	Cvar_SetDescription( cv, "JS overlay menu active flag: releases the mouse cursor, suppresses game input, and lets scripts receive input_key/rp_click events." );
	cl_motd = Cvar_Get( "cl_motd", "1", 0 );
	Cvar_SetDescription( cl_motd, "Toggle the display of the 'Message of the day'. When Quake 3 Arena starts a map up, it sends the GL_RENDERER string to the Message Of The Day server at id. This responds back with a message of the day to the client." );

	cl_timeout = Cvar_Get( "cl_timeout", "200", 0 );
	Cvar_CheckRange( cl_timeout, "5", NULL, CV_INTEGER );
	Cvar_SetDescription( cl_timeout, "Duration of receiving nothing from server for client to decide it must be disconnected (in seconds)." );

	cl_autoNudge = Cvar_Get( "cl_autoNudge", "0", CVAR_TEMP );
	Cvar_CheckRange( cl_autoNudge, "0", "1", CV_FLOAT );
	Cvar_SetDescription( cl_autoNudge, "Automatic time nudge that uses your average ping as the time nudge, values:\n  0 - use fixed \\cl_timeNudge\n (0..1] - factor of median average ping to use as timenudge\n" );

#ifdef LEGACY_STANDALONE
	cl_autoGraphicsProfile = Cvar_Get( "cl_autoGraphicsProfile", "0", CVAR_ARCHIVE_ND );
#else
	cl_autoGraphicsProfile = Cvar_Get( "cl_autoGraphicsProfile", "1", CVAR_ARCHIVE_ND );
#endif
	Cvar_CheckRange( cl_autoGraphicsProfile, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_autoGraphicsProfile,
		"Auto graphics profile on cgame load: baseq3+cgame.qvm -> classic_baseq3.cfg; "
		"OpenArena native -> classic_openarena_native.cfg unless modern/hybrid already requested "
		"(r_fbo+r_pbr, selective hybrid, or cl_preferModernGraphics 1) -> modern_openarena.cfg; "
		"other native cgame -> modern_native.cfg. Set 0 to disable auto selection." );

	cl_preferModernGraphics = Cvar_Get( "cl_preferModernGraphics", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_preferModernGraphics, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_preferModernGraphics,
		"When 1, OpenArena native auto-profile uses modern_openarena.cfg (PBR/FBO/SSR) "
		"instead of classic_openarena_native.cfg. Also implied by selective hybrid or r_fbo+r_pbr." );
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

	rconAddress = Cvar_Get ("rconAddress", "", 0);
	Cvar_SetDescription( rconAddress, "The IP address of the remote console you wish to connect to." );

	cl_conXOffset = Cvar_Get ("cl_conXOffset", "0", 0);
	Cvar_SetDescription( cl_conXOffset, "Console notifications X-offset." );
	cl_conColor = Cvar_Get( "cl_conColor", "", 0 );
	Cvar_SetDescription( cl_conColor, "Console background color, set as R G B A values from 0-255, use with \\seta to save in config." );

#ifdef MACOS_X
	cl_inGameVideo = Cvar_Get( "r_inGameVideo", "0", CVAR_ARCHIVE_ND );
#else
	cl_inGameVideo = Cvar_Get( "r_inGameVideo", "1", CVAR_ARCHIVE_ND );
#endif
	Cvar_SetDescription( cl_inGameVideo, "Controls whether in-game video should be drawn." );

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

	cl_reconnectArgs = Cvar_Get( "cl_reconnectArgs", "", CVAR_ARCHIVE_ND | CVAR_NOTABCOMPLETE );

	// Hub auth token used to decrypt rcon_autoset payloads. CVAR_PROTECTED
	// so QVMs cannot set it; set from engine login / config / +set.
	// Cleared client-side on auth_fail server command.
	{
		cvar_t *authToken = Cvar_Get( "cl_authToken", "", CVAR_ARCHIVE_ND | CVAR_PROTECTED );
		Cvar_SetDescription( authToken,
			"Long-lived hub authentication token. Used to decrypt rcon_autoset blobs; cleared on auth_fail." );
	}

	Cvar_Get ("name", "UnnamedPlayer", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("snaps", "40", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("model", "gargoyle", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("headmodel", "gargoyle", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("team_model", "gargoyle", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("team_headmodel", "gargoyle", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("color1", "4", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("color2", "5", CVAR_USERINFO | CVAR_ARCHIVE );
	Cvar_Get ("handicap", "100", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("sex", "male", CVAR_USERINFO | CVAR_ARCHIVE_ND );
	Cvar_Get ("cl_anonymous", "0", CVAR_USERINFO | CVAR_ARCHIVE_ND );

	Cvar_Get ("password", "", CVAR_USERINFO | CVAR_NORESTART);
	Cvar_Get ("cg_predictItems", "1", CVAR_USERINFO | CVAR_ARCHIVE );
	// Advertise flatscreen client over the VR usercmd button protocol.
	Cvar_Get ("vr", "0", CVAR_USERINFO | CVAR_ROM );

	Cvar_Get ("cg_viewsize", "100", CVAR_ARCHIVE_ND );
	Cvar_Get ("cg_stereoSeparation", "0", CVAR_ROM);
#ifdef LEGACY_STANDALONE
	Cvar_Set( "cl_autoGraphicsProfile", "0" );
	Cvar_Set( "net_p2p", "1" );
	Cvar_Set( "net_sdr", "1" );
	Cvar_Set( "net_p2pBackend", "auto" );
	Cvar_Set( "net_p2pStun", "1" );
	Cvar_Set( "net_p2pStunAutoAdvertise", "1" );
	Cvar_Set( "net_p2pPunch", "1" );
	Cvar_Set( "cl_p2pAutoReconnect", "1" );
	Cvar_Set( "sv_p2pHostMigration", "1" );
	Cvar_Set( "in_mouse", "1" );
	Cvar_Set( "in_nograb", "0" );
	Cvar_Set( "cl_freelook", "1" );
	Cvar_Set( "sensitivity", "3" );
	Cvar_Set( "cl_mouseAccel", "0" );
	Cvar_Set( "m_filter", "0" );
	Cvar_Set( "cl_builtInTtf", "1" );
	Cvar_Set( "cl_builtInTtfConsole", "1" );
	Cvar_Set( "r_sdfEnable", "1" );
	Cvar_Set( "r_font", "fonts/Inter-Bold.ttf" );
	Cvar_Set( "r_consoleFont", "fonts/consolemono.ttf" );
	Cvar_Set( "r_fontKerning", "1" );
	Cvar_Set( "r_fontConsoleProportional", "1" );
	Cvar_Set( "r_fontDpi", "96" );
	Cvar_Set( "r_fontLcd", "0" );
	Cvar_Set( "r_fontMipmap", "0" );
	Cvar_Set( "r_fontSubpixelPos", "0" );
	Cvar_Set( "r_fontHint", "1" );
	Cvar_Set( "r_fontShadow", "1" );
	Cvar_Set( "r_fontSize", "14" );
	Cvar_Set( "r_textMode", "1" );
	Cvar_Set( "r_openWorld", "0" );
	Cvar_Set( "cl_openWorldSync", "0" );
	Cvar_Set( "r_bspStream", "0" );
	Cvar_Set( "cm_stream", "0" );
	Cvar_Set( "cm_streamMerge", "0" );
	Cvar_Set( "cm_openWorldCollision", "0" );
	Cvar_Set( "r_district", "0" );
	Cvar_Set( "r_proc", "0" );
	Cvar_Set( "r_cbtTerrain", "0" );
	Cvar_Set( "r_vdb", "0" );
	Cvar_Set( "r_vdbFog", "0" );
	Cvar_Set( "r_volumetricFog", "0" );
#endif
}
