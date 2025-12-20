/*
===========================================================================
Backend Interface (Renderer-Agnostic)

Small function table to decouple renderer front-end code from a specific
backend implementation (GL/VK/D3D/Metal). Callers can set a backend once at
startup, and helper utilities (frame graph, etc.) can invoke through this
interface without direct backend knowledge.
===========================================================================
*/

#pragma once

#include "../../common/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rb_backend_iface_s {
	// Lifecycle
	qboolean (*init)( void );
	void (*shutdown)( void );

	// Per-frame scope
	void (*begin_frame)( void );
	void (*end_frame)( void );

	// Pass scope (render, compute, resolve, etc.)
	void (*begin_pass)( const char *name );
	void (*end_pass)( void );
} rb_backend_iface_t;

// Returns the active backend interface (never NULL; falls back to a null backend).
const rb_backend_iface_t *RB_GetBackendInterface( void );

// Set/replace the active backend interface. Passing NULL restores the null backend.
void RB_SetBackendInterface( const rb_backend_iface_t *iface );

// Restore the null backend (all functions are no-ops).
void RB_ResetBackendInterface( void );

#ifdef __cplusplus
} // extern "C"
#endif


