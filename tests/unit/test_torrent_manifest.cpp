#include "engine/content/torrent_manifest.h"

#include <cassert>
#include <string>

using idtech3::content::TorrentManifest_IsAllowedPackagePath;
using idtech3::content::TorrentManifest_IsSafeRelativePath;
using idtech3::content::TorrentManifest_ValidateText;
using idtech3::content::TorrentManifestValidationOptions;

static std::string ValidManifest( void ) {
	return R"json({
		"package": "base_patch",
		"version": "2026.07.13",
		"info_hash": "0123456789abcdef0123456789abcdef01234567",
		"total_size": 12,
		"signature": "ed25519:test-signature",
		"files": [
			{
				"path": "baseq3/pak9.pk3",
				"size": 12,
				"sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
			}
		]
	})json";
}

int main( void ) {
	assert( TorrentManifest_IsSafeRelativePath( "baseq3/pak9.pk3" ) );
	assert( TorrentManifest_IsAllowedPackagePath( "baseq3/pak9.pk3" ) );
	assert( !TorrentManifest_IsAllowedPackagePath( "/baseq3/pak9.pk3" ) );
	assert( !TorrentManifest_IsAllowedPackagePath( "baseq3/../pak9.pk3" ) );
	assert( !TorrentManifest_IsAllowedPackagePath( "baseq3/native.so" ) );
	assert( !TorrentManifest_IsAllowedPackagePath( "C:\\baseq3\\pak9.pk3" ) );

	TorrentManifestValidationOptions options;
	const auto valid = TorrentManifest_ValidateText( ValidManifest(), options );
	assert( valid.ok );
	assert( valid.manifest.files.size() == 1 );

	std::string unsignedManifest = ValidManifest();
	const std::size_t sigPos = unsignedManifest.find( "\"signature\"" );
	assert( sigPos != std::string::npos );
	unsignedManifest.erase( sigPos, unsignedManifest.find( ",", sigPos ) - sigPos + 1 );
	assert( !TorrentManifest_ValidateText( unsignedManifest, options ).ok );

	options.requireSignature = false;
	assert( TorrentManifest_ValidateText( unsignedManifest, options ).ok );

	options.maxPackageSize = 8;
	assert( !TorrentManifest_ValidateText( ValidManifest(), options ).ok );

	return 0;
}
