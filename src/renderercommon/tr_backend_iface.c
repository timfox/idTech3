/*
===========================================================================
Backend Interface (Renderer-Agnostic)

Provides a tiny function-table abstraction so front-end code can drive an
arbitrary backend (GL/VK/D3D/Metal) without hard dependencies. A null backend
is installed by default so calls are always safe.
===========================================================================
*/

#include "tr_backend_iface.h"

#include <string.h>

static qboolean RB_NullInit( void ) {
	return qtrue;
}

static void RB_NullVoid( void ) {
}

static void RB_NullBeginPass( const char *name ) {
	(void)name;
}

static rb_backend_iface_t rb_null_backend = {
	.init = RB_NullInit,
	.shutdown = RB_NullVoid,
	.begin_frame = RB_NullVoid,
	.end_frame = RB_NullVoid,
	.begin_pass = RB_NullBeginPass,
	.end_pass = RB_NullVoid
};

static rb_backend_iface_t rb_active_backend = { 0 };

const rb_backend_iface_t *RB_GetBackendInterface( void ) {
	// Always return a valid interface; fall back to the null backend.
	if ( rb_active_backend.init == NULL &&
	     rb_active_backend.shutdown == NULL &&
	     rb_active_backend.begin_frame == NULL &&
	     rb_active_backend.end_frame == NULL &&
	     rb_active_backend.begin_pass == NULL &&
	     rb_active_backend.end_pass == NULL ) {
		return &rb_null_backend;
	}
	return &rb_active_backend;
}

void RB_SetBackendInterface( const rb_backend_iface_t *iface ) {
	if ( iface ) {
		rb_active_backend = *iface;
	} else {
		RB_ResetBackendInterface();
	}
}

void RB_ResetBackendInterface( void ) {
	rb_active_backend = rb_null_backend;
}


