/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Optional peer-assisted package delivery backend.
===========================================================================
*/

extern "C" {
#include "client.h"
}

#include "cl_torrent.h"
#include "engine/content/torrent_manifest.h"

#include <cstring>
#include <cstdlib>
#include <string>

#ifdef USE_LIBTORRENT
#include <libtorrent/version.hpp>
#endif

static cvar_t *torrent_enable;
static cvar_t *torrent_port;
static cvar_t *torrent_downloadRate;
static cvar_t *torrent_uploadRate;
static cvar_t *torrent_maxConnections;
static cvar_t *torrent_maxPackageSize;
static cvar_t *torrent_seedCompleted;
static cvar_t *torrent_allowPublicTrackers;
static cvar_t *torrent_allowDht;
static cvar_t *torrent_allowPex;
static cvar_t *torrent_allowLsd;
static cvar_t *torrent_requireSignature;
static cvar_t *torrent_debug;

static std::uint64_t CL_Torrent_MaxPackageSize( void ) {
	if ( !torrent_maxPackageSize || !torrent_maxPackageSize->string || !torrent_maxPackageSize->string[0] ) {
		return 8589934592ull;
	}
	char *end = nullptr;
	const unsigned long long value = std::strtoull( torrent_maxPackageSize->string, &end, 10 );
	if ( end == torrent_maxPackageSize->string || value == 0ull ) {
		return 8589934592ull;
	}
	return static_cast<std::uint64_t>( value );
}

static bool CL_Torrent_Compiled( void ) {
#ifdef USE_LIBTORRENT
	return true;
#else
	return false;
#endif
}

static const char *CL_Torrent_VersionString( void ) {
#ifdef USE_LIBTORRENT
	return LIBTORRENT_VERSION;
#else
	return "not compiled";
#endif
}

static void CL_Torrent_Status_f( void ) {
	Com_Printf( "Peer-assisted content delivery: %s\n", CL_Torrent_Compiled() ? "compiled" : "not compiled" );
	Com_Printf( "libtorrent: %s\n", CL_Torrent_VersionString() );
	Com_Printf( "torrent_enable: %d\n", torrent_enable ? torrent_enable->integer : 0 );
	Com_Printf( "torrent_port: %d\n", torrent_port ? torrent_port->integer : 0 );
	Com_Printf( "torrent_downloadRate: %d\n", torrent_downloadRate ? torrent_downloadRate->integer : 0 );
	Com_Printf( "torrent_uploadRate: %d\n", torrent_uploadRate ? torrent_uploadRate->integer : 0 );
	Com_Printf( "torrent_maxConnections: %d\n", torrent_maxConnections ? torrent_maxConnections->integer : 0 );
	Com_Printf( "torrent_seedCompleted: %d\n", torrent_seedCompleted ? torrent_seedCompleted->integer : 0 );
	Com_Printf( "torrent_allowPublicTrackers: %d\n", torrent_allowPublicTrackers ? torrent_allowPublicTrackers->integer : 0 );
	Com_Printf( "torrent_allowDht: %d\n", torrent_allowDht ? torrent_allowDht->integer : 0 );
	Com_Printf( "torrent_allowPex: %d\n", torrent_allowPex ? torrent_allowPex->integer : 0 );
	Com_Printf( "torrent_allowLsd: %d\n", torrent_allowLsd ? torrent_allowLsd->integer : 1 );
	Com_Printf( "torrent_requireSignature: %d\n", torrent_requireSignature ? torrent_requireSignature->integer : 1 );
}

static void CL_Torrent_CheckManifest_f( void ) {
	if ( Cmd_Argc() < 2 || !Cmd_Argv( 1 )[0] ) {
		Com_Printf( "usage: torrent_checkmanifest <manifest.json>\n" );
		return;
	}

	void *buffer = nullptr;
	const int len = FS_ReadFile( Cmd_Argv( 1 ), &buffer );
	if ( len <= 0 || !buffer ) {
		Com_Printf( S_COLOR_RED "could not read manifest: %s\n", Cmd_Argv( 1 ) );
		if ( buffer ) {
			FS_FreeFile( buffer );
		}
		return;
	}

	idtech3::content::TorrentManifestValidationOptions options;
	options.maxPackageSize = CL_Torrent_MaxPackageSize();
	options.requireSignature = !torrent_requireSignature || torrent_requireSignature->integer != 0;
	const std::string text( static_cast<const char *>( buffer ), static_cast<std::size_t>( len ) );
	const auto result = idtech3::content::TorrentManifest_ValidateText( text, options );
	FS_FreeFile( buffer );

	if ( !result.ok ) {
		Com_Printf( S_COLOR_RED "manifest rejected: %s\n", result.error.c_str() );
		return;
	}
	Com_Printf( "manifest ok: %s %s, %u files, %llu bytes\n",
		result.manifest.package.c_str(),
		result.manifest.version.c_str(),
		static_cast<unsigned>( result.manifest.files.size() ),
		static_cast<unsigned long long>( result.manifest.totalSize ) );
}

void CL_Torrent_Init( void ) {
	torrent_enable = Cvar_Get( "torrent_enable", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_enable, "Enable optional peer-assisted package delivery. Requires a build with libtorrent support." );
	torrent_port = Cvar_Get( "torrent_port", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_port, "Peer-assisted content listen port. 0 lets the backend choose an ephemeral port." );
	torrent_downloadRate = Cvar_Get( "torrent_downloadRate", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_downloadRate, "Peer-assisted content download rate limit in bytes/sec. 0 is unlimited." );
	torrent_uploadRate = Cvar_Get( "torrent_uploadRate", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_uploadRate, "Peer-assisted content upload rate limit in bytes/sec. 0 is unlimited." );
	torrent_maxConnections = Cvar_Get( "torrent_maxConnections", "80", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_maxConnections, "Maximum peer-assisted content peer connections." );
	torrent_maxPackageSize = Cvar_Get( "torrent_maxPackageSize", "8589934592", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_maxPackageSize, "Maximum accepted peer-assisted package size in bytes." );
	torrent_seedCompleted = Cvar_Get( "torrent_seedCompleted", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_seedCompleted, "Seed completed peer-assisted packages after installation. Off by default." );
	torrent_allowPublicTrackers = Cvar_Get( "torrent_allowPublicTrackers", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_allowPublicTrackers, "Allow public trackers for package delivery. Official content should keep this disabled." );
	torrent_allowDht = Cvar_Get( "torrent_allowDht", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_allowDht, "Allow DHT for package delivery. Official content should keep this disabled." );
	torrent_allowPex = Cvar_Get( "torrent_allowPex", "0", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_allowPex, "Allow peer exchange for package delivery. Official content should keep this disabled." );
	torrent_allowLsd = Cvar_Get( "torrent_allowLsd", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_allowLsd, "Allow local service discovery for LAN package delivery." );
	torrent_requireSignature = Cvar_Get( "torrent_requireSignature", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( torrent_requireSignature, "Require signed torrent package manifests before installation." );
	torrent_debug = Cvar_Get( "torrent_debug", "0", CVAR_TEMP );
	Cvar_SetDescription( torrent_debug, "Verbose peer-assisted package delivery diagnostics." );

	Cvar_CheckRange( torrent_enable, "0", "1", CV_INTEGER );
	Cvar_CheckRange( torrent_port, "0", "65535", CV_INTEGER );
	Cvar_CheckRange( torrent_downloadRate, "0", nullptr, CV_INTEGER );
	Cvar_CheckRange( torrent_uploadRate, "0", nullptr, CV_INTEGER );
	Cvar_CheckRange( torrent_maxConnections, "1", "1024", CV_INTEGER );
	Cvar_CheckRange( torrent_seedCompleted, "0", "1", CV_INTEGER );
	Cvar_CheckRange( torrent_allowPublicTrackers, "0", "1", CV_INTEGER );
	Cvar_CheckRange( torrent_allowDht, "0", "1", CV_INTEGER );
	Cvar_CheckRange( torrent_allowPex, "0", "1", CV_INTEGER );
	Cvar_CheckRange( torrent_allowLsd, "0", "1", CV_INTEGER );
	Cvar_CheckRange( torrent_requireSignature, "0", "1", CV_INTEGER );
	Cvar_CheckRange( torrent_debug, "0", "1", CV_INTEGER );

	Cmd_AddCommand( "torrent_status", CL_Torrent_Status_f );
	Cmd_AddCommand( "torrent_checkmanifest", CL_Torrent_CheckManifest_f );
}

void CL_Torrent_Shutdown( void ) {
	Cmd_RemoveCommand( "torrent_checkmanifest" );
	Cmd_RemoveCommand( "torrent_status" );
}

qboolean CL_Torrent_Available( void ) {
	return CL_Torrent_Compiled() && torrent_enable && torrent_enable->integer ? qtrue : qfalse;
}

qboolean CL_Torrent_IsPackageURL( const char *url ) {
	if ( !url || !url[0] ) {
		return qfalse;
	}
	if ( !std::strncmp( url, "magnet:", 7 ) ) {
		return qtrue;
	}
	const std::size_t len = std::strlen( url );
	return len > 8 && !Q_stricmp( url + len - 8, ".torrent" ) ? qtrue : qfalse;
}

qboolean CL_Torrent_BeginPackageDownload( const char *localName, const char *packageURL ) {
	if ( !CL_Torrent_IsPackageURL( packageURL ) ) {
		return qfalse;
	}
	if ( !CL_Torrent_Available() ) {
		Com_Printf( S_COLOR_YELLOW "peer-assisted package delivery is disabled or not compiled; falling back where possible\n" );
		return qfalse;
	}
	if ( !localName || !idtech3::content::TorrentManifest_IsAllowedPackagePath( localName ) ) {
		Com_Printf( S_COLOR_RED "refusing unsafe torrent package target: %s\n", localName ? localName : "" );
		return qfalse;
	}

	Com_Printf( S_COLOR_YELLOW "peer-assisted package downloads are enabled, but session transfer is not started by this build slice yet: %s\n", packageURL );
	return qfalse;
}
