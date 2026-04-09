/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Optional copy of bundled game data from APK assets into writable storage.
===========================================================================
*/

#ifndef ANDROID_ASSET_BOOTSTRAP_H
#define ANDROID_ASSET_BOOTSTRAP_H

#ifdef __ANDROID__

/* Copy assets under "apkassets/<tree>" into writableRoot/<tree> (creates dirs).
 * Safe to call before Com_Init; skips files that already exist on disk. */
void Android_AssetBootstrapUnpack( void *assetManager, const char *writableRoot );

#else

static inline void Android_AssetBootstrapUnpack( void *am, const char *writableRoot ) {
	(void)am;
	(void)writableRoot;
}

#endif

#endif
