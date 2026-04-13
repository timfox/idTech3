Optional bundled game data for first-run install.

Place files here under the same layout as fs_basepath, e.g.:
  apkassets/base/pak0.pk3

On startup the engine copies missing files into the app data directory
(android_main: Android_AssetBootstrapUnpack). Existing files on disk are never
overwritten.

Remove this folder if you do not ship data inside the APK.
