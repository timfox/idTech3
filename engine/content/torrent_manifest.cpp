/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Peer-assisted package manifest validation.
===========================================================================
*/

#include "engine/content/torrent_manifest.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace idtech3::content {

namespace {

bool IsHexString( const std::string& value ) {
	return !value.empty() && std::all_of( value.begin(), value.end(), []( unsigned char c ) {
		return std::isxdigit( c ) != 0;
	} );
}

bool IsAbsoluteOrDrivePath( const std::string& path ) {
	if ( path.empty() ) {
		return false;
	}
	if ( path[0] == '/' || path[0] == '\\' ) {
		return true;
	}
	return path.size() >= 2 && std::isalpha( static_cast<unsigned char>( path[0] ) ) && path[1] == ':';
}

std::string ExtractString( const std::string& text, const char *key ) {
	const std::string token = std::string( "\"" ) + key + "\"";
	const std::size_t keyPos = text.find( token );
	if ( keyPos == std::string::npos ) {
		return {};
	}
	const std::size_t colon = text.find( ':', keyPos + token.size() );
	if ( colon == std::string::npos ) {
		return {};
	}
	const std::size_t firstQuote = text.find( '"', colon + 1 );
	if ( firstQuote == std::string::npos ) {
		return {};
	}
	std::string out;
	bool escaped = false;
	for ( std::size_t i = firstQuote + 1; i < text.size(); ++i ) {
		const char c = text[i];
		if ( escaped ) {
			out.push_back( c );
			escaped = false;
			continue;
		}
		if ( c == '\\' ) {
			escaped = true;
			continue;
		}
		if ( c == '"' ) {
			return out;
		}
		out.push_back( c );
	}
	return {};
}

bool ExtractUnsigned( const std::string& text, const char *key, std::uint64_t& value ) {
	const std::string token = std::string( "\"" ) + key + "\"";
	const std::size_t keyPos = text.find( token );
	if ( keyPos == std::string::npos ) {
		return false;
	}
	const std::size_t colon = text.find( ':', keyPos + token.size() );
	if ( colon == std::string::npos ) {
		return false;
	}
	std::size_t i = colon + 1;
	while ( i < text.size() && std::isspace( static_cast<unsigned char>( text[i] ) ) ) {
		++i;
	}
	if ( i >= text.size() || !std::isdigit( static_cast<unsigned char>( text[i] ) ) ) {
		return false;
	}
	std::uint64_t result = 0;
	for ( ; i < text.size() && std::isdigit( static_cast<unsigned char>( text[i] ) ); ++i ) {
		const unsigned digit = static_cast<unsigned>( text[i] - '0' );
		if ( result > ( std::numeric_limits<std::uint64_t>::max() - digit ) / 10ull ) {
			return false;
		}
		result = result * 10ull + digit;
	}
	value = result;
	return true;
}

bool NextObjectInArray( const std::string& text, const char *key, std::size_t& cursor, std::string& objectText ) {
	const std::string token = std::string( "\"" ) + key + "\"";
	if ( cursor == 0 ) {
		const std::size_t keyPos = text.find( token );
		if ( keyPos == std::string::npos ) {
			return false;
		}
		const std::size_t openArray = text.find( '[', keyPos + token.size() );
		if ( openArray == std::string::npos ) {
			return false;
		}
		cursor = openArray + 1;
	}
	while ( cursor < text.size() && std::isspace( static_cast<unsigned char>( text[cursor] ) ) ) {
		++cursor;
	}
	if ( cursor >= text.size() || text[cursor] == ']' ) {
		return false;
	}
	const std::size_t open = text.find( '{', cursor );
	if ( open == std::string::npos ) {
		return false;
	}
	unsigned depth = 0;
	bool inString = false;
	bool escaped = false;
	for ( std::size_t i = open; i < text.size(); ++i ) {
		const char c = text[i];
		if ( inString ) {
			if ( escaped ) {
				escaped = false;
			} else if ( c == '\\' ) {
				escaped = true;
			} else if ( c == '"' ) {
				inString = false;
			}
			continue;
		}
		if ( c == '"' ) {
			inString = true;
		} else if ( c == '{' ) {
			++depth;
		} else if ( c == '}' ) {
			if ( --depth == 0 ) {
				objectText = text.substr( open, i - open + 1 );
				cursor = i + 1;
				return true;
			}
		}
	}
	return false;
}

TorrentManifestValidationResult Fail( const char *message ) {
	TorrentManifestValidationResult result;
	result.error = message;
	return result;
}

} // namespace

bool TorrentManifest_IsSafeRelativePath( const std::string& path ) {
	if ( path.empty() || path.size() >= 240 ) {
		return false;
	}
	if ( IsAbsoluteOrDrivePath( path ) ) {
		return false;
	}
	std::string component;
	for ( char c : path ) {
		if ( c == '\\' || c == '/' ) {
			if ( component.empty() || component == "." || component == ".." ) {
				return false;
			}
			component.clear();
			continue;
		}
		if ( static_cast<unsigned char>( c ) < 32 ) {
			return false;
		}
		component.push_back( c );
	}
	return !component.empty() && component != "." && component != "..";
}

bool TorrentManifest_IsAllowedPackagePath( const std::string& path ) {
	if ( !TorrentManifest_IsSafeRelativePath( path ) ) {
		return false;
	}
	const std::size_t dot = path.find_last_of( '.' );
	if ( dot == std::string::npos ) {
		return false;
	}
	std::string ext = path.substr( dot );
	std::transform( ext.begin(), ext.end(), ext.begin(), []( unsigned char c ) {
		return static_cast<char>( std::tolower( c ) );
	} );
	return ext == ".pk3";
}

TorrentManifestValidationResult TorrentManifest_ValidateText(
	const std::string& text,
	const TorrentManifestValidationOptions& options ) {
	if ( text.empty() ) {
		return Fail( "manifest is empty" );
	}
	if ( text.size() > 1024 * 1024 ) {
		return Fail( "manifest is too large" );
	}

	TorrentManifestValidationResult result;
	TorrentManifest& manifest = result.manifest;
	manifest.package = ExtractString( text, "package" );
	manifest.version = ExtractString( text, "version" );
	manifest.infoHash = ExtractString( text, "info_hash" );
	manifest.signature = ExtractString( text, "signature" );
	ExtractUnsigned( text, "total_size", manifest.totalSize );

	if ( manifest.package.empty() ) {
		return Fail( "manifest missing package" );
	}
	if ( manifest.version.empty() ) {
		return Fail( "manifest missing version" );
	}
	if ( manifest.infoHash.empty() ) {
		return Fail( "manifest missing info_hash" );
	}
	if ( ( manifest.infoHash.size() != 40 && manifest.infoHash.size() != 64 ) || !IsHexString( manifest.infoHash ) ) {
		return Fail( "manifest info_hash must be 40 or 64 hex characters" );
	}
	if ( options.requireSignature && manifest.signature.empty() ) {
		return Fail( "manifest missing signature" );
	}
	if ( manifest.totalSize == 0 ) {
		return Fail( "manifest total_size must be non-zero" );
	}
	if ( manifest.totalSize > options.maxPackageSize ) {
		return Fail( "manifest exceeds max package size" );
	}

	std::size_t cursor = 0;
	std::string objectText;
	std::uint64_t fileTotal = 0;
	while ( NextObjectInArray( text, "files", cursor, objectText ) ) {
		if ( manifest.files.size() >= options.maxFiles ) {
			return Fail( "manifest exceeds max file count" );
		}
		TorrentManifestFile file;
		file.path = ExtractString( objectText, "path" );
		file.sha256 = ExtractString( objectText, "sha256" );
		if ( !ExtractUnsigned( objectText, "size", file.size ) || file.size == 0 ) {
			return Fail( "manifest file has invalid size" );
		}
		if ( !TorrentManifest_IsAllowedPackagePath( file.path ) ) {
			return Fail( "manifest file path is not an allowed package path" );
		}
		if ( file.sha256.size() != 64 || !IsHexString( file.sha256 ) ) {
			return Fail( "manifest file sha256 must be 64 hex characters" );
		}
		if ( fileTotal > std::numeric_limits<std::uint64_t>::max() - file.size ) {
			return Fail( "manifest file sizes overflow" );
		}
		fileTotal += file.size;
		manifest.files.push_back( file );
	}
	if ( manifest.files.empty() ) {
		return Fail( "manifest has no files" );
	}
	if ( fileTotal != manifest.totalSize ) {
		return Fail( "manifest total_size does not match files" );
	}

	result.ok = true;
	return result;
}

} // namespace idtech3::content
