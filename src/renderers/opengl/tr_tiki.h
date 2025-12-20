#ifndef __TR_TIKI_H__
#define __TR_TIKI_H__

#include "../common/q_shared.h"
#include "../common/qfiles.h"

// TIKI file format identifiers
#define TIKI_IDENT			(('T'<<24)+('I'<<16)+('K'<<8)+'I')
#define TIKI_VERSION		1

// TIKI model structures
typedef struct {
	int			ident;
	int			version;
	int			numBones;
	int			numFrames;
	int			numSurfaces;
	int			numTags;
	int			ofsBones;
	int			ofsFrames;
	int			ofsSurfaces;
	int			ofsTags;
	int			ofsEnd;
} tikiHeader_t;

typedef struct {
	char		name[64];
	int			parent;
	vec3_t		position;
	quat_t		rotation;
	vec3_t		scale;
} tikiBone_t;

typedef struct {
	char		name[64];
	tikiBone_t	*bones;
} tikiFrame_t;

typedef struct tikiSurface_s {
	char		name[64];
	int			numVerts;
	int			numTris;
	int			numBoneWeights;
	int			ofsVerts;
	int			ofsTris;
	int			ofsBoneWeights;
	int			ofsEnd;
} tikiSurface_t;

typedef struct {
	vec3_t		position;
	vec3_t		normal;
	vec2_t		st;
	byte		color[4];
} tikiVertex_t;

typedef struct {
	int			indices[3];
} tikiTriangle_t;

typedef struct {
	int			boneIndex;
	float		weight;
} tikiBoneWeight_t;

typedef struct {
	char		name[64];
	vec3_t		origin;
	quat_t		angles;
} tikiTag_t;

// In-memory TIKI model structure
typedef struct {
	tikiHeader_t	*header;
	tikiBone_t		*bones;
	tikiFrame_t		*frames;
	tikiSurface_t	*surfaces;
	tikiTag_t		*tags;
	int				numBones;
	int				numFrames;
	int				numSurfaces;
	int				numTags;
} tikiData_t;

// TIKI model loading functions
qboolean R_LoadTIKI(model_t *mod, void *buffer, int filesize, const char *mod_name);
qhandle_t R_RegisterTIKI(const char *name, model_t *mod);

#endif // __TR_TIKI_H__

