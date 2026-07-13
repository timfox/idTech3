/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Peer-assisted package manifest validation.
===========================================================================
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace idtech3::content {

struct TorrentManifestFile {
	std::string path;
	std::uint64_t size = 0;
	std::string sha256;
};

struct TorrentManifest {
	std::string package;
	std::string version;
	std::string infoHash;
	std::string signature;
	std::uint64_t totalSize = 0;
	std::vector<TorrentManifestFile> files;
};

struct TorrentManifestValidationOptions {
	std::uint64_t maxPackageSize = 8589934592ull;
	std::size_t maxFiles = 256;
	bool requireSignature = true;
};

struct TorrentManifestValidationResult {
	bool ok = false;
	std::string error;
	TorrentManifest manifest;
};

bool TorrentManifest_IsSafeRelativePath( const std::string& path );
bool TorrentManifest_IsAllowedPackagePath( const std::string& path );
TorrentManifestValidationResult TorrentManifest_ValidateText(
	const std::string& text,
	const TorrentManifestValidationOptions& options );

} // namespace idtech3::content
