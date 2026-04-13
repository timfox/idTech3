/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Copy optional bundled content from APK assets (apkassets/...) into the
writable app data directory so FS can find base/ without manual sideloading.
===========================================================================
*/

#ifdef __ANDROID__

#include "../../qcommon/q_shared.h"
#include <android/asset_manager.h>
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define ASSET_TAG "idTech3Assets"
#define ASSET_ROOT "apkassets"
#define ASSET_ROOT_SLASH ASSET_ROOT "/"

static qboolean android_mkdir_p( char *path ) {
	size_t len = strlen( path );
	if ( len == 0 ) {
		return qfalse;
	}
	for ( char *p = path + 1; *p; p++ ) {
		if ( *p == '/' ) {
			*p = '\0';
			if ( mkdir( path, 0755 ) != 0 && errno != EEXIST ) {
				*p = '/';
				return qfalse;
			}
			*p = '/';
		}
	}
	if ( mkdir( path, 0755 ) != 0 && errno != EEXIST ) {
		return qfalse;
	}
	return qtrue;
}

static void copy_one_asset( AAssetManager *mgr, const char *assetPath, const char *destPath ) {
	AAsset *a = AAssetManager_open( mgr, assetPath, AASSET_MODE_STREAMING );
	FILE *fp;
	char dirbuf[MAX_OSPATH];
	char *slash;
	long sz, rd;
	char *buf;

	if ( !a ) {
		return;
	}

	sz = AAsset_getLength( a );
	if ( sz <= 0 ) {
		AAsset_close( a );
		return;
	}

	Q_strncpyz( dirbuf, destPath, sizeof( dirbuf ) );
	slash = strrchr( dirbuf, '/' );
	if ( slash ) {
		*slash = '\0';
		if ( !android_mkdir_p( dirbuf ) ) {
			__android_log_print( ANDROID_LOG_WARN, ASSET_TAG, "mkdir failed: %s", dirbuf );
			AAsset_close( a );
			return;
		}
	}

	fp = fopen( destPath, "wb" );
	if ( !fp ) {
		__android_log_print( ANDROID_LOG_WARN, ASSET_TAG, "open write failed: %s", destPath );
		AAsset_close( a );
		return;
	}

	buf = (char *)malloc( (size_t)sz );
	if ( !buf ) {
		fclose( fp );
		AAsset_close( a );
		return;
	}

	rd = AAsset_read( a, buf, (size_t)sz );
	AAsset_close( a );
	if ( rd != sz ) {
		free( buf );
		fclose( fp );
		return;
	}

	if ( fwrite( buf, 1, (size_t)sz, fp ) != (size_t)sz ) {
		free( buf );
		fclose( fp );
		return;
	}

	free( buf );
	fclose( fp );
	__android_log_print( ANDROID_LOG_INFO, ASSET_TAG, "installed %s -> %s (%ld bytes)", assetPath, destPath, sz );
}

/* assetPath is e.g. "apkassets" or "apkassets/base"; writableRoot is fs_basepath */
static void walk_assets( AAssetManager *mgr, const char *assetPath, const char *writableRoot, int *outCount ) {
	AAssetDir *dir;
	const char *name;
	char childAsset[MAX_OSPATH];
	const size_t rootLen = strlen( ASSET_ROOT_SLASH );

	dir = AAssetManager_openDir( mgr, assetPath );
	if ( !dir ) {
		return;
	}

	while ( ( name = AAssetDir_getNextFileName( dir ) ) != NULL ) {
		Com_sprintf( childAsset, sizeof( childAsset ), "%s/%s", assetPath, name );

		{
			AAssetDir *probe = AAssetManager_openDir( mgr, childAsset );
			if ( probe ) {
				AAssetDir_close( probe );
				walk_assets( mgr, childAsset, writableRoot, outCount );
				continue;
			}
		}

		/* File: map apkassets/<rel> -> writableRoot/<rel> */
		if ( Q_strncmp( childAsset, ASSET_ROOT_SLASH, (int)rootLen ) != 0 ) {
			continue;
		}

		{
			const char *rel = childAsset + rootLen;
			char destPath[MAX_OSPATH];
			char normRel[MAX_OSPATH];
			int i, j;

			/* Normalize rel: reject ".." path segments */
			j = 0;
			for ( i = 0; rel[i] && j < (int)sizeof( normRel ) - 1; i++ ) {
				if ( rel[i] == '\\' ) {
					normRel[j++] = '/';
				} else {
					normRel[j++] = rel[i];
				}
			}
			normRel[j] = '\0';
			if ( strstr( normRel, ".." ) != NULL ) {
				continue;
			}

			Com_sprintf( destPath, sizeof( destPath ), "%s/%s", writableRoot, normRel );

			{
				struct stat st;
				if ( stat( destPath, &st ) == 0 ) {
					continue;
				}
			}

			copy_one_asset( mgr, childAsset, destPath );
			if ( outCount ) {
				(*outCount)++;
			}
		}
	}

	AAssetDir_close( dir );
}

void Android_AssetBootstrapUnpack( void *assetManager, const char *writableRoot ) {
	AAssetManager *am = (AAssetManager *)assetManager;
	int n = 0;
	AAssetDir *probe;

	if ( !am || !writableRoot || !writableRoot[0] ) {
		return;
	}

	probe = AAssetManager_openDir( am, ASSET_ROOT );
	if ( !probe ) {
		__android_log_print( ANDROID_LOG_INFO, ASSET_TAG,
			"No assets/%s in APK — skipping bundled game data install", ASSET_ROOT );
		return;
	}
	AAssetDir_close( probe );

	__android_log_print( ANDROID_LOG_INFO, ASSET_TAG,
		"Unpacking assets/%s into %s (existing files skipped)", ASSET_ROOT, writableRoot );

	walk_assets( am, ASSET_ROOT, writableRoot, &n );

	__android_log_print( ANDROID_LOG_INFO, ASSET_TAG,
		"Asset bootstrap finished (%d new file(s))", n );
}

#endif /* __ANDROID__ */
