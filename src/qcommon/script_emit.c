#include "q_shared.h"
#include "qcommon.h"
#include "script_emit.h"
#include "js_debug.h"
#include "csharp_debug.h"
#include "python_debug.h"

void Com_ScriptEmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 ) {
#ifdef USE_DUKTAPE
	JsDebug_EmitEvent( eventName, s0, s1, i0, i1 );
#endif
#ifdef USE_CSHARP
	CsDebug_EmitEvent( eventName, s0, s1, i0, i1 );
#endif
#ifdef USE_PYTHON
	PyDebug_EmitEvent( eventName, s0, s1, i0, i1 );
#endif
	(void)eventName;
	(void)s0;
	(void)s1;
	(void)i0;
	(void)i1;
}

void Com_ScriptSetCurrentMenu( int menu ) {
#ifdef USE_DUKTAPE
	JsDebug_SetCurrentMenu( menu );
#else
	(void)menu;
#endif
}
