#ifndef APP_CRDT_H
#define APP_CRDT_H

#include "q_shared.h"

#define APP_CRDT_MAX_SCRIPTS	16
#define APP_CRDT_MAX_PAYLOAD	512
#define APP_CRDT_DEFAULT_QUEUE	64

typedef struct {
	int major;
	int minor;
	int patch;
} appCrdtVersion_t;

typedef struct {
	appCrdtVersion_t version;
	char manifestPath[MAX_OSPATH];
	char scriptPaths[APP_CRDT_MAX_SCRIPTS][MAX_OSPATH];
	int scriptCount;
} appCrdtSpec_t;

typedef struct {
	int msgMajor;
	char payload[APP_CRDT_MAX_PAYLOAD];
} appCrdtEvent_t;

typedef enum {
	APP_CRDT_DISPATCH_DELIVER,
	APP_CRDT_DISPATCH_BUFFER,
	APP_CRDT_DISPATCH_ADAPT
} appCrdtDispatchResult_t;

typedef void ( *appCrdtDeliverFn )( int msgMajor, const char *payload, void *userData );
typedef void ( *appCrdtAdaptFn )( int msgMajor, const char *payload, void *userData );

typedef struct {
	appCrdtEvent_t events[APP_CRDT_DEFAULT_QUEUE];
	int head;
	int count;
	int capacity;
	appCrdtDeliverFn deliver;
	appCrdtAdaptFn adapt;
	void *userData;
} appCrdtQueue_t;

/* semver (Algorithm 1) */
qboolean AppCrdt_ParseVersion( const char *text, appCrdtVersion_t *out );
void AppCrdt_FormatVersion( const appCrdtVersion_t *ver, char *buf, int buflen );
int AppCrdt_CompareVersion( const appCrdtVersion_t *a, const appCrdtVersion_t *b );
qboolean AppCrdt_MergeLWW( appCrdtVersion_t *local, const appCrdtVersion_t *remote );

/* manifest */
qboolean AppCrdt_ParseManifestJson( const char *jsonText, appCrdtSpec_t *spec );
qboolean AppCrdt_LoadManifest( const char *manifestPath, appCrdtSpec_t *spec );

/* event queue (Algorithm 2) */
void AppCrdt_QueueInit( appCrdtQueue_t *queue, int capacity,
	appCrdtDeliverFn deliver, appCrdtAdaptFn adapt, void *userData );
appCrdtDispatchResult_t AppCrdt_QueueDispatch( appCrdtQueue_t *queue, int localMajor,
	int msgMajor, const char *payload );
int AppCrdt_QueueFlushUpToMajor( appCrdtQueue_t *queue, int localMajor );
void AppCrdt_QueueClear( appCrdtQueue_t *queue );

/* engine integration */
void AppCrdt_Init( void );
qboolean AppCrdt_IsEnabled( void );
int AppCrdt_GetLocalMajor( void );
void AppCrdt_SetLocalVersion( const appCrdtVersion_t *ver );
const appCrdtVersion_t *AppCrdt_GetLocalVersion( void );
int AppCrdt_GetQueueMax( void );

void Cmd_AppCrdtStatus_f( void );

/* idtech3backend submodule integration (optional) */
void AppCrdt_SetBackendRoot( const char *root );
const char *AppCrdt_GetBackendRoot( void );
qboolean AppCrdt_BackendAvailable( void );
qboolean AppCrdt_GetDefaultBackendManifest( char *buf, int buflen );
qboolean AppCrdt_ResolveBackendOsPath( const char *qpath, char *osOut, int osLen );
void AppCrdt_RefreshBackendRoot( void );

qboolean AppCrdt_ApplyPublish( const appCrdtSpec_t *spec );

#endif /* APP_CRDT_H */
