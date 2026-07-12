/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Submit Bullet debug lines to the Vulkan scene as world-space polys.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "../physics/phys_debugdraw.h"
#include "client.h"

void CL_PhysDebugDrawSubmit( void ) {
	cvar_t *debugDraw;
	int i;
	int count;
	const phys_debug_line_t *lines;
	polyVert_t verts[4];
	vec3_t delta;
	vec3_t side;
	vec3_t up = { 0.0f, 0.0f, 1.0f };
	float thickness = 1.5f;
	byte r, g, b;

	debugDraw = Cvar_Get( "phys_debugDraw", "0", CVAR_ARCHIVE );
	if ( !debugDraw || !debugDraw->integer ) {
		return;
	}

	count = PhysDebug_GetLineCount();
	lines = PhysDebug_GetLines();
	if ( !lines || count <= 0 ) {
		return;
	}

	for ( i = 0; i < count; i++ ) {
		VectorSubtract( lines[i].to, lines[i].from, delta );
		if ( VectorLength( delta ) < 0.01f ) {
			continue;
		}

		CrossProduct( delta, up, side );
		if ( VectorLength( side ) < 0.01f ) {
			up[0] = 1.0f; up[1] = 0.0f; up[2] = 0.0f;
			CrossProduct( delta, up, side );
		}
		VectorNormalize( side );
		VectorScale( side, thickness, side );

		r = (byte)( lines[i].color[0] * 255.0f );
		g = (byte)( lines[i].color[1] * 255.0f );
		b = (byte)( lines[i].color[2] * 255.0f );

		VectorSubtract( lines[i].from, side, verts[0].xyz );
		VectorAdd( lines[i].from, side, verts[1].xyz );
		VectorAdd( lines[i].to, side, verts[2].xyz );
		VectorSubtract( lines[i].to, side, verts[3].xyz );

		verts[0].st[0] = 0; verts[0].st[1] = 0;
		verts[1].st[0] = 0; verts[1].st[1] = 1;
		verts[2].st[0] = 1; verts[2].st[1] = 1;
		verts[3].st[0] = 1; verts[3].st[1] = 0;

		verts[0].modulate.rgba[0] = r; verts[0].modulate.rgba[1] = g; verts[0].modulate.rgba[2] = b; verts[0].modulate.rgba[3] = 200;
		verts[1].modulate.rgba[0] = r; verts[1].modulate.rgba[1] = g; verts[1].modulate.rgba[2] = b; verts[1].modulate.rgba[3] = 200;
		verts[2].modulate.rgba[0] = r; verts[2].modulate.rgba[1] = g; verts[2].modulate.rgba[2] = b; verts[2].modulate.rgba[3] = 200;
		verts[3].modulate.rgba[0] = r; verts[3].modulate.rgba[1] = g; verts[3].modulate.rgba[2] = b; verts[3].modulate.rgba[3] = 200;

		re.AddPolyToScene( cls.whiteShader, 4, verts, 1 );
	}
}
