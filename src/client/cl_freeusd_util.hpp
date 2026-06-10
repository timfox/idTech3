/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared FreeUSD path helpers for client console tools (C++20).
===========================================================================
*/
#pragma once

#include <string>
#include <string_view>

extern "C" {
const char *Cvar_VariableString( const char *var_name );
char *FS_BuildOSPath( const char *base, const char *game, const char *qpath );
}

inline std::string Cl_FreeusdBuildOsPath( std::string_view qpath )
{
	if ( qpath.empty() ) {
		return {};
	}
	const std::string qpathStr( qpath );
	const char *base = Cvar_VariableString( "fs_basepath" );
	const char *game = Cvar_VariableString( "fs_game" );
	char *os = FS_BuildOSPath( base, game, qpathStr.c_str() );

	if ( !os || !os[0] ) {
		return {};
	}
	return std::string( os );
}
