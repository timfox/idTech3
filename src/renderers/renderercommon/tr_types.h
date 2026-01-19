/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#ifndef __TR_TYPES_H
#define __TR_TYPES_H

#include "../../common/q_shared.h"
#include "../../common/qcommon.h"

#define MAX_VIDEO_HANDLES	16

#define	MAX_DLIGHTS			32			// can't be increased, because bit flags are used on surfaces

// renderfx flags - strongly typed enum for better type safety
typedef enum {
	RF_NONE				= 0x0000,
	RF_MINLIGHT			= 0x0001,		// always have some light (viewmodel, some items)
	RF_THIRD_PERSON		= 0x0002,		// don't draw through eyes, only mirrors (player bodies, chat sprites)
	RF_FIRST_PERSON		= 0x0004,		// only draw through eyes (view weapon, damage blood blob)
	RF_DEPTHHACK		= 0x0008,		// for view weapon Z crunching
	RF_CROSSHAIR		= 0x0010,		// This item is a cross hair and will draw over everything similar to
										// DEPTHHACK in stereo rendering mode, with the difference that the
										// projection matrix won't be hacked to reduce the stereo separation as
										// is done for the gun.
	RF_NOSHADOW			= 0x0040,		// don't add stencil shadows
	RF_LIGHTING_ORIGIN	= 0x0080,		// use refEntity->lightingOrigin instead of refEntity->origin
										// for lighting.  This allows entities to sink into the floor
										// with their origin going solid, and allows all parts of a
										// player to get the same lighting
	RF_SHADOW_PLANE		= 0x0100,		// use refEntity->shadowPlane
	RF_WRAP_FRAMES		= 0x0200,		// mod the model frames by the maxframes to allow continuous
										// animation without needing to know the frame count
	RF_HILIGHT			= 0x0400,		// entity is hilighted (for example, when hovering over the entity in the editor)
	RF_BLINK			= 0x0800,		// entity is blinking (for example, a powerup)
	RF_FORCENOLOD		= 0x1000,		// entity forces LOD to 0 (full detail)
	RF_ADDITIVE			= 0x2000,		// entity is rendered with additive blending
	RF_TRANSLUCENT		= 0x4000		// entity is translucent (but not additive)
} renderFxFlags_t;

// Compile-time validation of enum values
STATIC_ASSERT((int)RF_MINLIGHT == 0x0001, "RF_MINLIGHT value mismatch");
STATIC_ASSERT((int)RF_THIRD_PERSON == 0x0002, "RF_THIRD_PERSON value mismatch");
STATIC_ASSERT((int)RF_FIRST_PERSON == 0x0004, "RF_FIRST_PERSON value mismatch");
STATIC_ASSERT((int)RF_DEPTHHACK == 0x0008, "RF_DEPTHHACK value mismatch");
STATIC_ASSERT((int)RF_CROSSHAIR == 0x0010, "RF_CROSSHAIR value mismatch");
STATIC_ASSERT((int)RF_NOSHADOW == 0x0040, "RF_NOSHADOW value mismatch");
STATIC_ASSERT((int)RF_LIGHTING_ORIGIN == 0x0080, "RF_LIGHTING_ORIGIN value mismatch");
STATIC_ASSERT((int)RF_SHADOW_PLANE == 0x0100, "RF_SHADOW_PLANE value mismatch");
STATIC_ASSERT((int)RF_WRAP_FRAMES == 0x0200, "RF_WRAP_FRAMES value mismatch");
STATIC_ASSERT((int)RF_HILIGHT == 0x0400, "RF_HILIGHT value mismatch");
STATIC_ASSERT((int)RF_BLINK == 0x0800, "RF_BLINK value mismatch");
STATIC_ASSERT((int)RF_FORCENOLOD == 0x1000, "RF_FORCENOLOD value mismatch");
STATIC_ASSERT((int)RF_ADDITIVE == 0x2000, "RF_ADDITIVE value mismatch");
STATIC_ASSERT((int)RF_TRANSLUCENT == 0x4000, "RF_TRANSLUCENT value mismatch");

// Legacy defines for backward compatibility (deprecated - use enum values)
#define	RF_MINLIGHT			((renderFxFlags_t)0x0001)
#define	RF_THIRD_PERSON		((renderFxFlags_t)0x0002)
#define	RF_FIRST_PERSON		((renderFxFlags_t)0x0004)
#define	RF_DEPTHHACK		((renderFxFlags_t)0x0008)
#define RF_CROSSHAIR		((renderFxFlags_t)0x0010)
#define	RF_NOSHADOW			((renderFxFlags_t)0x0040)
#define RF_LIGHTING_ORIGIN	((renderFxFlags_t)0x0080)
#define	RF_SHADOW_PLANE		((renderFxFlags_t)0x0100)
#define	RF_WRAP_FRAMES		((renderFxFlags_t)0x0200)
#define RF_HILIGHT			((renderFxFlags_t)0x0400)
#define RF_BLINK			((renderFxFlags_t)0x0800)
#define RF_FORCENOLOD		((renderFxFlags_t)0x1000)
#define RF_ADDITIVE			((renderFxFlags_t)0x2000)
// RF_TRANSLUCENT is defined in the enum above

// refdef flags
#define RDF_NOWORLDMODEL	0x0001		// used for player configuration screen
#define RDF_HYPERSPACE		0x0004		// teleportation effect

typedef struct {
	vec3_t		xyz;
	float		st[2];
	color4ub_t	modulate;
} polyVert_t;

typedef struct poly_s {
	qhandle_t			hShader;
	int					numVerts;
	polyVert_t			*verts;
} poly_t;

typedef enum {
	RT_MODEL,
	RT_POLY,
	RT_SPRITE,
	RT_BEAM,
	RT_RAIL_CORE,
	RT_RAIL_RINGS,
	RT_LIGHTNING,
	RT_PORTALSURFACE,		// doesn't draw anything, just info for portals

	RT_MAX_REF_ENTITY_TYPE
} refEntityType_t;

typedef struct {
	refEntityType_t	reType;
	int			renderfx;

	qhandle_t	hModel;				// opaque type outside refresh

	// most recent data
	vec3_t		lightingOrigin;		// so multi-part models can be lit identically (RF_LIGHTING_ORIGIN)
	float		shadowPlane;		// projection shadows go here, stencils go slightly lower

	vec3_t		axis[3];			// rotation vectors
	qboolean	nonNormalizedAxes;	// axis are not normalized, i.e. they have scale
	float		origin[3];			// also used as MODEL_BEAM's "from"
	int			frame;				// also used as MODEL_BEAM's diameter

	// previous data for frame interpolation
	float		oldorigin[3];		// also used as MODEL_BEAM's "to"
	int			oldframe;
	float		backlerp;			// 0.0 = current, 1.0 = old

	// texturing
	int			skinNum;			// inline skin index
	qhandle_t	customSkin;			// NULL for default skin
	qhandle_t	customShader;		// use one image for the entire thing

	// misc
	color4ub_t	shader;
	float		shaderTexCoord[2];	// texture coordinates used by tcMod entity modifiers

	// subtracted from refdef time to control effect start times
	floatint_t	shaderTime;			// -EC- set to union

	// extra sprite information
	float		radius;
	float		rotation;
} refEntity_t;


#define	MAX_RENDER_STRINGS			8
#define	MAX_RENDER_STRING_LENGTH	32

typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	// time in milliseconds for shader effects and other time dependent rendering issues
	int			time;

	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];

	// text messages for deform text shaders
	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];
} refdef_t;


typedef enum {
	STEREO_CENTER,
	STEREO_LEFT,
	STEREO_RIGHT
} stereoFrame_t;


/*
** glconfig_t
**
** Contains variables specific to the OpenGL configuration
** being run right now.  These are constant once the OpenGL
** subsystem is initialized.
*/
typedef enum {
	TC_NONE,
	TC_S3TC,  // this is for the GL_S3_s3tc extension.
	TC_S3TC_ARB  // this is for the GL_EXT_texture_compression_s3tc extension.
} textureCompression_t;

typedef enum {
	GLDRV_ICD,					// driver is integrated with window system
								// WARNING: there are tests that check for
								// > GLDRV_ICD for minidriverness, so this
								// should always be the lowest value in this
								// enum set
	GLDRV_STANDALONE,			// driver is a non-3Dfx standalone driver
	GLDRV_VOODOO,				// driver is a 3Dfx standalone driver
	GLDRV_OPENGL3				// modern OpenGL 3.3+ with GLSL shaders
} glDriverType_t;

typedef enum {
	GLHW_GENERIC,			// where everything works the way it should
	GLHW_3DFX_2D3D,			// Voodoo Banshee or Voodoo3, relevant since if this is
							// the hardware type then there can NOT exist a secondary
							// display adapter
	GLHW_RIVA128,			// where you can't interpolate alpha
	GLHW_RAGEPRO,			// where you can't modulate alpha on alpha textures
	GLHW_PERMEDIA2			// where you don't have src*dst
} glHardwareType_t;

typedef struct {
	char					renderer_string[MAX_STRING_CHARS];
	char					vendor_string[MAX_STRING_CHARS];
	char					version_string[MAX_STRING_CHARS];
	char					extensions_string[BIG_INFO_STRING];

	int						maxTextureSize;			// queried from GL
	int						numTextureUnits;		// multitexture ability

	int						colorBits, depthBits, stencilBits;

	glDriverType_t			driverType;
	glHardwareType_t		hardwareType;

	qboolean				deviceSupportsGamma;
	textureCompression_t	textureCompression;
	qboolean				textureEnvAddAvailable;

	int						vidWidth, vidHeight;
	// aspect is the screen's physical width / height, which may be different
	// than scrWidth / scrHeight if the pixels are non-square
	// normal screens should be 4/3, but wide aspect monitors may be 16/9
	float					windowAspect;

	int						displayFrequency;

	// synonymous with "does rendering consume the entire screen?", therefore
	// a Voodoo or Voodoo2 will have this set to TRUE, as will a Win32 ICD that
	// used CDS.
	qboolean				isFullscreen;
	qboolean				stereoEnabled;
	qboolean				smpActive;		// UNUSED, present for compatibility
} glconfig_t;

#define	myftol(x) ((int)(x))

#if defined(_WIN32)
#define OPENGL_DRIVER_NAME	"opengl32"
#elif defined(MACOS_X)
#define OPENGL_DRIVER_NAME	"/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#else
#define OPENGL_DRIVER_NAME	"libGL.so.1"
#endif

#endif	// __TR_TYPES_H
