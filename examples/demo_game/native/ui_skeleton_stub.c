/*
 * Minimal native UI module for demo / skeleton playfields (no retail ui.qvm).
 * Loaded from idtech3_demo/vm/ui*.so (or .dll) before falling back to ui.qvm.
 *
 * API: id Tech 3 UI VM / native bridge (vmMain + dllEntry).
 */
#include "../../../src/qcommon/q_shared.h"
#include "../../../src/qcommon/qcommon.h"
#include "../../../src/ui/ui_public.h"

#include <stddef.h>

void QDECL dllEntry( dllSyscall_t syscallptr );
intptr_t QDECL vmMain( int command, int arg0, int arg1, int arg2 );

static dllSyscall_t trap;

#if defined( _WIN32 ) && !defined( __GNUC__ )
#define UI_DLLEXPORT __declspec( dllexport )
#else
#define UI_DLLEXPORT
#endif

void QDECL dllEntry( dllSyscall_t syscallptr ) {
	trap = syscallptr;
}

static int float_bits( float f ) {
	union {
		float f;
		int i;
	} u;
	u.f = f;
	return u.i;
}

static void draw_placeholder( int realtime ) {
	float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	int h = 480;
	const char *title = "idtech3_demo - minimal native UI";
	const char *hint = "Add full base/ pk3s for menus & maps; this stub proves client + renderer only.";

	if ( trap ) {
		trap( UI_R_DRAWSTRING, 24, 32, float_bits( 0.35f ), (intptr_t)title, (intptr_t)white );
		trap( UI_R_DRAWSTRING, 24, 64, float_bits( 0.28f ), (intptr_t)hint, (intptr_t)white );
		trap( UI_R_DRAWSTRING, 24, h - 48, float_bits( 0.22f ), (intptr_t)"~ key: console", (intptr_t)white );
		(void)realtime;
	}
}

intptr_t QDECL vmMain( int command, int arg0, int arg1, int arg2 ) {
	(void)arg0;
	(void)arg1;
	(void)arg2;

	switch ( command ) {
	case UI_GETAPIVERSION:
		return UI_API_VERSION;
	case UI_INIT:
	case UI_SHUTDOWN:
		return 0;
	case UI_KEY_EVENT:
	case UI_MOUSE_EVENT:
		return 0;
	case UI_REFRESH:
		draw_placeholder( arg0 );
		return 0;
	case UI_IS_FULLSCREEN:
		return 0;
	case UI_SET_ACTIVE_MENU:
		return 0;
	case UI_CONSOLE_COMMAND:
		return 0;
	case UI_DRAW_CONNECT_SCREEN:
		return 0;
	case UI_HASUNIQUECDKEY:
		return 0;
	default:
		return 0;
	}
}
