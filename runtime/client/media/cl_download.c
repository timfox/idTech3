/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client pak/map download path (UDP server list + optional cURL).
===========================================================================
*/

#include "client.h"
#include "cl_download.h"
#include "cl_connect.h"
#include "cl_torrent.h"

#include <string.h>

#ifdef USE_CURL
#include "cl_curl.h"

download_t download;
#endif

cvar_t *cl_allowDownload;
#ifdef USE_CURL
cvar_t *cl_dlURL;
cvar_t *cl_dlDirectory;
cvar_t *cl_mapAutoDownload;

static void CL_Download_f( void );
#endif

static void CL_CompleteCallvote( const char *args, int argNum )
{
	if ( argNum >= 2 ) {
		const char *p = Com_SkipTokens( args, 1, " " );

		if ( p > args )
			Field_CompleteCommand( p, qtrue, qtrue );
	}
}

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
			CL_Connect_Reconnect();
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
	qboolean triedTorrent = qfalse;

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

		if(!(cl_allowDownload->integer & DLF_NO_REDIRECT) &&
			!(clc.sv_allowDownload & DLF_NO_REDIRECT) && *clc.sv_dlURL) {
			const char *torrentURL = va("%s/%s", clc.sv_dlURL, remoteName);
			if(CL_Torrent_IsPackageURL(torrentURL)) {
				triedTorrent = qtrue;
				useCURL = CL_Torrent_BeginPackageDownload(localName, torrentURL);
			}
		}

#ifndef USE_CURL
		(void)triedTorrent;
#endif
#ifdef USE_CURL
		if(!useCURL && !triedTorrent && !(cl_allowDownload->integer & DLF_NO_REDIRECT)) {
			const char *redirectURL = NULL;
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
				redirectURL = va("%s/%s", clc.sv_dlURL, remoteName);
				CL_cURL_BeginDownload(localName, redirectURL);
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
#if !defined( DEDICATED ) && defined( USE_CURL )
/*
===============
CL_CMStream_PrefetchHandler

Sector pk3 HTTP prefetch when cm_stream requests adjacent cells (sv_sectorURL).
===============
*/
void CL_CMStream_PrefetchHandler( const char *localName, const char *remoteURL )
{
	cvar_t *allow;

	if ( !localName || !localName[0] || !remoteURL || !remoteURL[0] ) {
		return;
	}
	if ( FS_FileExists( localName ) ) {
		return;
	}
	allow = Cvar_Get( "cl_allowDownload", "1", CVAR_ARCHIVE );
	if ( !allow || !( allow->integer & DLF_ENABLE ) || ( allow->integer & DLF_NO_REDIRECT ) ) {
		Com_Printf( "[cm_stream] skip HTTP prefetch %s (cl_allowDownload)\n", localName );
		return;
	}
	if ( !clc.cURLEnabled ) {
		Com_Printf( "[cm_stream] skip HTTP prefetch %s (libcurl unavailable)\n", localName );
		return;
	}
	if ( clc.downloadCURL ) {
		Com_DPrintf( "[cm_stream] defer prefetch %s (download in progress)\n", localName );
		return;
	}
	Com_Printf( "[cm_stream] HTTP prefetch %s\n", remoteURL );
	CL_cURL_BeginDownload( localName, remoteURL );
}
#endif
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

/*
==================
CL_Download_Frame

Pump manual \download / \dlmap jobs and in-server cURL downloads.
Returns qtrue when the frame should stop early (disconnected cURL UI mode).
==================
*/
qboolean CL_Download_Frame( int msec, int realMsec ) {
	(void)msec;
	(void)realMsec;
#ifdef USE_CURL
	if ( download.cURL ) {
		Com_DL_Perform( &download );
	}

	if ( clc.downloadCURLM ) {
		CL_cURL_PerformDownload();
		if ( clc.cURLDisconnected ) {
			cls.frametime = msec;
			cls.realtime += msec;
			cls.framecount++;
			SCR_UpdateScreen();
			S_Update( realMsec );
			Con_RunConsole();
			return qtrue;
		}
	}
#endif
	return qfalse;
}

void CL_Download_Init( void ) {
	cl_allowDownload = Cvar_Get( "cl_allowDownload", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_allowDownload, "Enables downloading of content needed in server. Valid bitmask flags:\n 1: Downloading enabled\n 2: Do not use HTTP/FTP downloads\n 4: Do not use UDP downloads" );
	CL_Torrent_Init();
#ifdef USE_CURL
	cl_mapAutoDownload = Cvar_Get( "cl_mapAutoDownload", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_mapAutoDownload, "Automatic map download for play and demo playback (via automatic \\dlmap call)." );
#ifdef USE_CURL_DLOPEN
	cl_cURLLib = Cvar_Get( "cl_cURLLib", DEFAULT_CURL_LIB, 0 );
	Cvar_SetDescription( cl_cURLLib, "Filename of cURL library to load." );
#endif
	cl_dlURL = Cvar_Get( "cl_dlURL", "http://ws.q3df.org/maps/download/%1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_dlURL, "Cvar must point to download location." );

	cl_dlDirectory = Cvar_Get( "cl_dlDirectory", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( cl_dlDirectory, "0", "1", CV_INTEGER );
	{
		const char *s = va( "Save downloads initiated by \\dlmap and \\download commands in:\n"
			" 0 - current game directory\n"
			" 1 - basegame (%s) directory\n", FS_GetBaseGameDir() );
		Cvar_SetDescription( cl_dlDirectory, s );
	}

	Cmd_AddCommand( "download", CL_Download_f );
	Cmd_AddCommand( "dlmap", CL_Download_f );
#if !defined( DEDICATED ) && defined( USE_CURL )
	CM_Stream_SetPrefetchHandler( CL_CMStream_PrefetchHandler );
#endif
#endif
}

void CL_Download_Shutdown( void ) {
	CL_Torrent_Shutdown();
#ifdef USE_CURL
	Com_DL_Cleanup( &download );
	Cmd_RemoveCommand( "download" );
	Cmd_RemoveCommand( "dlmap" );
#endif
}
