/*
===========================================================================
Blacksun Feature Flags (C++23 implementation)

Provides a small feature toggle registry for the mod, exposed through a
stable C ABI for use from legacy gamecode. Uses C++23 std::string_view and
unordered_map.
===========================================================================
*/

#include "bs_features.h"

#include <unordered_map>
#include <string_view>
#include <string>
#include <mutex>

namespace {
	using namespace std::literals;

	std::unordered_map<std::string, bool> g_features;
	std::mutex g_mutex;
	bool g_init = false;

	constexpr std::pair<std::string_view, bool> kDefaultFeatures[] = {
		{"vehicles"sv, false},
		{"puzzles"sv, false},
		{"cyberfx"sv, true},
		{"oop_entities"sv, false}
	};

	void ensure_init() {
		if ( g_init ) {
			return;
		}
		std::scoped_lock lock( g_mutex );
		if ( g_init ) {
			return;
		}
		g_features.clear();
		for ( const auto &p : kDefaultFeatures ) {
			g_features.emplace( p.first, p.second );
		}
		g_init = true;
	}
}

extern "C" void BS_Features_InitDefaults( void ) {
	ensure_init();
}

extern "C" void BS_Features_Set( const char *name, qboolean enabled ) {
	if ( !name ) {
		return;
	}
	ensure_init();
	std::scoped_lock lock( g_mutex );
	g_features[ name ] = ( enabled != qfalse );
}

extern "C" qboolean BS_Features_IsEnabled( const char *name ) {
	if ( !name ) {
		return qfalse;
	}
	ensure_init();
	std::scoped_lock lock( g_mutex );
	auto it = g_features.find( name );
	if ( it == g_features.end() ) {
		return qfalse;
	}
	return it->second ? qtrue : qfalse;
}


