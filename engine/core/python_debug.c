/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Embedded Python scripting — Infernux-inspired batch bridge (arXiv:2604.10263).
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "python_debug.h"
#include "python_batch.h"

#ifdef USE_PYTHON

#include "Python.h"

#define MAX_PY_TRACKED_SCRIPTS 64
#define MAX_PY_FRAME_CALLBACKS 32
#define MAX_PY_EVENT_TYPES     24
#define MAX_PY_EVENT_CALLBACKS 32

typedef struct {
	char name[64];
	PyObject *callbacks[MAX_PY_EVENT_CALLBACKS];
	int count;
} pyEventBucket_t;

static qboolean s_pyReady;
static int s_pyTrackedCount;
static char s_pyTrackedScripts[MAX_PY_TRACKED_SCRIPTS][MAX_OSPATH];
static PyObject *s_pyFrameCallbacks[MAX_PY_FRAME_CALLBACKS];
static int s_pyFrameCallbackCount;
static pyEventBucket_t s_pyEvents[MAX_PY_EVENT_TYPES];
static int s_pyEventBucketCount;

static cvar_t *py_autoInit;
static cvar_t *py_allowEvents;
static cvar_t *py_allowExec;
static cvar_t *py_frameCallbackBudgetMs;
static cvar_t *py_compatTarget;

static qboolean PyDebug_IsAllowedPath( const char *scriptPath )
{
	if ( !scriptPath || !scriptPath[0] ) {
		return qfalse;
	}
	return ( !Q_strncmp( scriptPath, "scripts/python/", 15 ) ||
		!Q_strncmp( scriptPath, "gameplay/", 9 ) ||
		!Q_strncmp( scriptPath, "server/", 7 ) ||
		!Q_strncmp( scriptPath, "client/", 7 ) );
}

static void PyDebug_InitPolicyCvars( void )
{
	if ( !py_allowEvents ) {
		py_allowEvents = Cvar_Get( "py_allowEvents", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( py_allowEvents, "Allow Python event handlers via Engine.on (0=off, 1=on)." );
	}
	if ( !py_allowExec ) {
		py_allowExec = Cvar_Get( "py_allowExec", "1", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( py_allowExec, "Allow Engine.exec console commands from Python (0=off, 1=on)." );
	}
	if ( !py_frameCallbackBudgetMs ) {
		py_frameCallbackBudgetMs = Cvar_Get( "py_frameCallbackBudgetMs", "2", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( py_frameCallbackBudgetMs, "Soft per-frame Python callback budget in ms (0=unlimited)." );
	}
	if ( !py_compatTarget ) {
		py_compatTarget = Cvar_Get( "py_compatTarget", "cpython-3.10+", CVAR_ROM | CVAR_PROTECTED );
		Cvar_SetDescription( py_compatTarget, "Read-only Python scripting API target." );
	}
	if ( !py_autoInit ) {
		py_autoInit = Cvar_Get( "py_autoInit", "0", CVAR_ARCHIVE_ND );
		Cvar_SetDescription( py_autoInit, "Initialize CPython at startup (0=manual py_reload, 1=auto)." );
	}
}

static void PyDebug_ClearCallbacks( void )
{
	int i;
	int j;

	for ( i = 0; i < s_pyFrameCallbackCount; i++ ) {
		Py_XDECREF( s_pyFrameCallbacks[i] );
		s_pyFrameCallbacks[i] = NULL;
	}
	s_pyFrameCallbackCount = 0;

	for ( i = 0; i < s_pyEventBucketCount; i++ ) {
		for ( j = 0; j < s_pyEvents[i].count; j++ ) {
			Py_XDECREF( s_pyEvents[i].callbacks[j] );
			s_pyEvents[i].callbacks[j] = NULL;
		}
		s_pyEvents[i].count = 0;
		s_pyEvents[i].name[0] = '\0';
	}
	s_pyEventBucketCount = 0;
}

static void PyDebug_CloseState( void )
{
	PyDebug_ClearCallbacks();
	if ( s_pyReady ) {
		PyBatch_Shutdown();
		Py_Finalize();
		s_pyReady = qfalse;
	}
}

static PyObject *py_id3_print( PyObject *self, PyObject *args )
{
	const char *msg;

	(void)self;
	if ( !PyArg_ParseTuple( args, "s", &msg ) ) {
		return NULL;
	}
	Com_Printf( "%s\n", msg );
	Py_RETURN_NONE;
}

static PyObject *py_id3_cvar_get( PyObject *self, PyObject *args )
{
	const char *name;
	const char *value;

	(void)self;
	if ( !PyArg_ParseTuple( args, "s", &name ) ) {
		return NULL;
	}
	value = Cvar_VariableString( name );
	return PyUnicode_FromString( value ? value : "" );
}

static PyObject *py_id3_cvar_set( PyObject *self, PyObject *args )
{
	const char *name;
	const char *value;

	(void)self;
	if ( !PyArg_ParseTuple( args, "ss", &name, &value ) ) {
		return NULL;
	}
	Cvar_Set2( name, value, qtrue );
	Py_RETURN_NONE;
}

static PyObject *py_id3_exec( PyObject *self, PyObject *args )
{
	const char *cmd;

	(void)self;
	PyDebug_InitPolicyCvars();
	if ( !py_allowExec || !py_allowExec->integer ) {
		Py_RETURN_NONE;
	}
	if ( !PyArg_ParseTuple( args, "s", &cmd ) ) {
		return NULL;
	}
	if ( cmd && cmd[0] ) {
		Cbuf_AddText( cmd );
		Cbuf_AddText( "\n" );
	}
	Py_RETURN_NONE;
}

static PyObject *py_id3_milliseconds( PyObject *self, PyObject *unused )
{
	(void)self;
	(void)unused;
	return PyLong_FromLong( Sys_Milliseconds() );
}

static PyObject *py_id3_engine_info( PyObject *self, PyObject *unused )
{
	char info[256];

	(void)self;
	(void)unused;
	Com_sprintf( info, sizeof( info ), "idtech3 python=%s", Py_GetVersion() );
	return PyUnicode_FromString( info );
}

static PyObject *py_id3_on_frame( PyObject *self, PyObject *args )
{
	PyObject *callback;

	(void)self;
	if ( !PyArg_ParseTuple( args, "O", &callback ) ) {
		return NULL;
	}
	if ( !PyCallable_Check( callback ) ) {
		PyErr_SetString( PyExc_TypeError, "on_frame expects a callable" );
		return NULL;
	}
	if ( s_pyFrameCallbackCount >= MAX_PY_FRAME_CALLBACKS ) {
		PyErr_SetString( PyExc_RuntimeError, "frame callback limit reached" );
		return NULL;
	}
	Py_INCREF( callback );
	s_pyFrameCallbacks[s_pyFrameCallbackCount++] = callback;
	Py_RETURN_NONE;
}

static pyEventBucket_t *PyDebug_FindEventBucket( const char *name, qboolean create )
{
	int i;

	for ( i = 0; i < s_pyEventBucketCount; i++ ) {
		if ( !Q_stricmp( s_pyEvents[i].name, name ) ) {
			return &s_pyEvents[i];
		}
	}
	if ( !create || s_pyEventBucketCount >= MAX_PY_EVENT_TYPES ) {
		return NULL;
	}
	Q_strncpyz( s_pyEvents[s_pyEventBucketCount].name, name, sizeof( s_pyEvents[0].name ) );
	s_pyEvents[s_pyEventBucketCount].count = 0;
	return &s_pyEvents[s_pyEventBucketCount++];
}

static PyObject *py_id3_on_event( PyObject *self, PyObject *args )
{
	const char *eventName;
	PyObject *callback;
	pyEventBucket_t *bucket;

	(void)self;
	PyDebug_InitPolicyCvars();
	if ( !py_allowEvents || !py_allowEvents->integer ) {
		Py_RETURN_NONE;
	}
	if ( !PyArg_ParseTuple( args, "sO", &eventName, &callback ) ) {
		return NULL;
	}
	if ( !PyCallable_Check( callback ) ) {
		PyErr_SetString( PyExc_TypeError, "on_event expects (name, callable)" );
		return NULL;
	}
	bucket = PyDebug_FindEventBucket( eventName, qtrue );
	if ( !bucket ) {
		PyErr_SetString( PyExc_RuntimeError, "event bucket limit reached" );
		return NULL;
	}
	if ( bucket->count >= MAX_PY_EVENT_CALLBACKS ) {
		PyErr_SetString( PyExc_RuntimeError, "event callback limit reached" );
		return NULL;
	}
	Py_INCREF( callback );
	bucket->callbacks[bucket->count++] = callback;
	Py_RETURN_NONE;
}

static PyMethodDef s_pyIdtech3Methods[] = {
	{ "print", py_id3_print, METH_VARARGS, "Print to engine console." },
	{ "cvar_get", py_id3_cvar_get, METH_VARARGS, "Get cvar string value." },
	{ "cvar_set", py_id3_cvar_set, METH_VARARGS, "Set cvar value." },
	{ "exec", py_id3_exec, METH_VARARGS, "Append console command." },
	{ "milliseconds", py_id3_milliseconds, METH_NOARGS, "Sys_Milliseconds()." },
	{ "engine_info", py_id3_engine_info, METH_NOARGS, "Engine + Python version string." },
	{ "on_frame", py_id3_on_frame, METH_VARARGS, "Register per-frame callback(msec, realMsec)." },
	{ "on_event", py_id3_on_event, METH_VARARGS, "Register event callback(s0, s1, i0, i1)." },
	{ "batch_read", PyBatch_Read, METH_VARARGS, "Batch-read SoA field for handle list." },
	{ "batch_write", PyBatch_Write, METH_VARARGS, "Batch-write SoA field for handle list." },
	{ "batch_info", PyBatch_Info, METH_NOARGS, "Batch bridge metadata." },
	{ "spawn_demo_grid", PyBatch_SpawnDemoGrid, METH_VARARGS, "Spawn NxN demo entity grid; returns count." },
	{ NULL, NULL, 0, NULL }
};

static struct PyModuleDef s_pyIdtech3Module = {
	PyModuleDef_HEAD_INIT,
	"_idtech3",
	"idTech3 native Python bridge (Infernux-style batch path)",
	-1,
	s_pyIdtech3Methods,
	NULL,
	NULL,
	NULL,
	NULL
};

static qboolean PyDebug_RegisterModule( void )
{
	PyObject *mod;

	mod = PyModule_Create( &s_pyIdtech3Module );
	if ( !mod ) {
		return qfalse;
	}
	PyDict_SetItemString( PyImport_GetModuleDict(), "_idtech3", mod );
	Py_DECREF( mod );
	return qtrue;
}

static qboolean PyDebug_EnsureRuntime( void )
{
	if ( s_pyReady ) {
		return qtrue;
	}

	Py_Initialize();
	PyBatch_Init();
	if ( !PyDebug_RegisterModule() ) {
		Com_Printf( S_COLOR_RED "Python: failed to register _idtech3 module\n" );
		return qfalse;
	}

	s_pyReady = qtrue;
	Com_Printf( "Python: runtime ready (%s); py_reload scripts/python/*.py\n", Py_GetVersion() );
	Com_Printf( "Python: batch bridge enabled (Infernux-style SoA columns)\n" );
	return qtrue;
}

static void PyDebug_PrintError( const char *prefix )
{
	PyObject *ptype;
	PyObject *pvalue;
	PyObject *ptrace;
	PyObject *pstr;

	PyErr_Fetch( &ptype, &pvalue, &ptrace );
	pstr = pvalue ? PyObject_Str( pvalue ) : NULL;
	Com_Printf( S_COLOR_RED "Python: %s: %s\n", prefix, pstr ? PyUnicode_AsUTF8( pstr ) : "(unknown error)" );
	Py_XDECREF( pstr );
	Py_XDECREF( ptype );
	Py_XDECREF( pvalue );
	Py_XDECREF( ptrace );
}

static qboolean PyDebug_RunSource( const char *source, const char *label )
{
	PyObject *mainModule;
	PyObject *globalDict;
	PyObject *result;

	if ( !PyDebug_EnsureRuntime() ) {
		return qfalse;
	}

	mainModule = PyImport_AddModule( "__main__" );
	globalDict = PyModule_GetDict( mainModule );
	result = PyRun_String( source, Py_file_input, globalDict, globalDict );
	if ( !result ) {
		PyDebug_PrintError( label ? label : "exec" );
		return qfalse;
	}
	Py_DECREF( result );
	return qtrue;
}

static qboolean PyDebug_LoadScript( const char *scriptPath )
{
	void *buf = NULL;
	int blen;
	qboolean ok;
	char bootstrap[512];

	if ( !PyDebug_EnsureRuntime() ) {
		return qfalse;
	}
	if ( !PyDebug_IsAllowedPath( scriptPath ) ) {
		Com_Printf( S_COLOR_RED "Python: denied path '%s' (allowed: scripts/python/, gameplay/, server/, client/)\n", scriptPath );
		return qfalse;
	}

	blen = FS_ReadFile( scriptPath, &buf );
	if ( blen <= 0 || !buf ) {
		Com_Printf( S_COLOR_RED "Python: could not read '%s'\n", scriptPath );
		return qfalse;
	}

	Com_sprintf( bootstrap, sizeof( bootstrap ),
		"import sys\n"
		"if 'scripts/python' not in sys.path:\n"
		"    sys.path.insert(0, 'scripts/python')\n" );

	ok = PyDebug_RunSource( bootstrap, "bootstrap" );
	if ( ok ) {
		ok = PyDebug_RunSource( (const char *)buf, scriptPath );
	}
	FS_FreeFile( buf );
	return ok;
}

static void PyDebug_TrackScript( const char *scriptPath )
{
	int i;

	for ( i = 0; i < s_pyTrackedCount; i++ ) {
		if ( !Q_stricmp( s_pyTrackedScripts[i], scriptPath ) ) {
			return;
		}
	}
	if ( s_pyTrackedCount >= MAX_PY_TRACKED_SCRIPTS ) {
		Com_Printf( S_COLOR_YELLOW "Python: tracked script limit reached (%d)\n", MAX_PY_TRACKED_SCRIPTS );
		return;
	}
	Q_strncpyz( s_pyTrackedScripts[s_pyTrackedCount], scriptPath, sizeof( s_pyTrackedScripts[0] ) );
	s_pyTrackedCount++;
}

void PyDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 )
{
	pyEventBucket_t *bucket;
	int i;

	PyDebug_InitPolicyCvars();
	if ( !s_pyReady || !py_allowEvents || !py_allowEvents->integer || !eventName ) {
		return;
	}
	bucket = PyDebug_FindEventBucket( eventName, qfalse );
	if ( !bucket ) {
		return;
	}

	for ( i = 0; i < bucket->count; i++ ) {
		PyObject *result = PyObject_CallFunction( bucket->callbacks[i], "ssii",
			s0 ? s0 : "", s1 ? s1 : "", i0, i1 );
		if ( !result ) {
			PyDebug_PrintError( eventName );
			PyErr_Clear();
		} else {
			Py_DECREF( result );
		}
	}
}

void PyDebug_Frame( int msec, int realMsec )
{
	int i;
	const int startTime = Sys_Milliseconds();
	const int budgetMs = py_frameCallbackBudgetMs ? py_frameCallbackBudgetMs->integer : 0;
	static int s_lastBudgetWarnMs = 0;

	PyDebug_InitPolicyCvars();
	if ( !s_pyReady ) {
		return;
	}

	for ( i = 0; i < s_pyFrameCallbackCount; i++ ) {
		PyObject *result;

		if ( budgetMs > 0 && ( Sys_Milliseconds() - startTime ) >= budgetMs ) {
			const int now = Sys_Milliseconds();
			if ( now - s_lastBudgetWarnMs > 3000 ) {
				s_lastBudgetWarnMs = now;
				Com_Printf( S_COLOR_YELLOW "Python: frame callback budget reached (%d ms)\n", budgetMs );
			}
			break;
		}

		result = PyObject_CallFunction( s_pyFrameCallbacks[i], "ii", msec, realMsec );
		if ( !result ) {
			PyDebug_PrintError( "frame" );
			PyErr_Clear();
		} else {
			Py_DECREF( result );
		}
	}

	PyDebug_EmitEvent( "frame", NULL, NULL, msec, realMsec );
}

void Cmd_PyReload_f( void )
{
	int argc = Cmd_Argc();
	int i;
	int successCount = 0;
	int failureCount = 0;

	if ( argc <= 1 ) {
		PyDebug_CloseState();
		if ( PyDebug_EnsureRuntime() ) {
			Com_Printf( "Python: runtime initialized (%s)\n", Py_GetVersion() );
		}
		return;
	}

	PyDebug_CloseState();
	PyDebug_ClearCallbacks();
	s_pyTrackedCount = 0;

	if ( !PyDebug_EnsureRuntime() ) {
		return;
	}

	for ( i = 1; i < argc; i++ ) {
		const char *scriptPath = Cmd_Argv( i );
		if ( !scriptPath || !scriptPath[0] ) {
			continue;
		}
		if ( PyDebug_LoadScript( scriptPath ) ) {
			PyDebug_TrackScript( scriptPath );
			successCount++;
		} else {
			failureCount++;
		}
	}

	Com_Printf( "Python: loaded %d script(s), %d failure(s)\n", successCount, failureCount );
}

void Cmd_PyList_f( void )
{
	int i;

	PyDebug_InitPolicyCvars();
	Com_Printf( "Python: compile-time API CPython (%s)\n", py_compatTarget ? py_compatTarget->string : "?" );
	Com_Printf( "Python: path policy scripts/python/, gameplay/, server/, client/\n" );
	Com_Printf( "Python: runtime %s\n", s_pyReady ? "initialized" : "not initialized (py_reload)" );
	Com_Printf( "Python: tracked scripts (%d)\n", s_pyTrackedCount );
	for ( i = 0; i < s_pyTrackedCount; i++ ) {
		Com_Printf( "  %2d: %s\n", i + 1, s_pyTrackedScripts[i] );
	}
	Com_Printf( "Python: frame callbacks=%d event buckets=%d\n", s_pyFrameCallbackCount, s_pyEventBucketCount );
}

void Cmd_PyDump_f( void )
{
	Cmd_PyList_f();
}

void Cmd_PyExec_f( void )
{
	const char *code;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: py_exec <python source>\n" );
		return;
	}
	code = Cmd_Argv( 1 );
	if ( PyDebug_RunSource( code, "py_exec" ) ) {
		Com_Printf( "Python: py_exec ok\n" );
	}
}

void PyDebug_InitCvars( void )
{
	PyDebug_InitPolicyCvars();
	if ( py_autoInit && py_autoInit->integer ) {
		(void)PyDebug_EnsureRuntime();
	}
}

#else /* !USE_PYTHON */

void PyDebug_InitCvars( void ) {}
void Cmd_PyReload_f( void ) { Com_Printf( "Python support disabled. Build with -DUSE_PYTHON=ON.\n" ); }
void Cmd_PyList_f( void ) { Com_Printf( "Python support disabled. Build with -DUSE_PYTHON=ON.\n" ); }
void Cmd_PyDump_f( void ) { Cmd_PyList_f(); }
void Cmd_PyExec_f( void ) { Com_Printf( "Python support disabled. Build with -DUSE_PYTHON=ON.\n" ); }
void PyDebug_Frame( int msec, int realMsec ) { (void)msec; (void)realMsec; }
void PyDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 )
{
	(void)eventName; (void)s0; (void)s1; (void)i0; (void)i1;
}

#endif /* USE_PYTHON */
