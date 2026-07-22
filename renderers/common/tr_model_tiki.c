/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Clean-room TIKI .tik parser (FAKK2/EF2-style sections).
===========================================================================
*/

#include "tr_model_tiki.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TIKI_MAX_FILE ( 1024 * 1024 )

static const char *s_allowedCmds[] = {
	"sound", "sound_vo", "voice", "effect", "fx", "footstep",
	"tag", "shout", "dialogue", "babble", "particle", "viewkick",
	NULL
};

qboolean R_Tiki_IsAllowedFrameCmd( const char *cmd ) {
	int i;
	if ( !cmd || !cmd[0] ) {
		return qfalse;
	}
	for ( i = 0; s_allowedCmds[i]; i++ ) {
		if ( !Q_stricmp( cmd, s_allowedCmds[i] ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void Tiki_Trim( char *s ) {
	char *e;
	while ( *s == ' ' || *s == '\t' || *s == '\r' ) {
		memmove( s, s + 1, strlen( s ) );
	}
	e = s + strlen( s );
	while ( e > s && ( e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' ) ) {
		*--e = '\0';
	}
}

static qboolean Tiki_PathSafe( const char *path ) {
	if ( !path || !path[0] ) {
		return qfalse;
	}
	if ( strstr( path, ".." ) || path[0] == '/' || path[0] == '\\' || strchr( path, ':' ) ) {
		return qfalse;
	}
	return qtrue;
}

static int Tiki_ReadLE32( const byte *p ) {
	return (int)( p[0] | ( p[1] << 8 ) | ( p[2] << 16 ) | ( p[3] << 24 ) );
}

qboolean R_Tiki_ParseTanHeader( const byte *data, int size, tikiTanHeader_t *out ) {
	if ( !data || !out || size < 24 ) {
		return qfalse;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	Com_Memcpy( out->ident, data, 4 );
	if ( out->ident[0] != 'T' || out->ident[1] != 'A' || out->ident[2] != 'N' ) {
		/* also accept "TAN\0" style */
		if ( !( out->ident[0] == 'T' && out->ident[1] == 'A' && out->ident[2] == 'N' && out->ident[3] == ' ' ) ) {
			return qfalse;
		}
	}
	out->version = Tiki_ReadLE32( data + 4 );
	out->numFrames = Tiki_ReadLE32( data + 8 );
	out->numBones = Tiki_ReadLE32( data + 12 );
	out->frameRate = Tiki_ReadLE32( data + 16 );
	out->ofsFrames = Tiki_ReadLE32( data + 20 );
	if ( out->version < 1 || out->version > 32 ) {
		return qfalse;
	}
	if ( out->numFrames < 0 || out->numFrames > 4096 ) {
		return qfalse;
	}
	if ( out->numBones < 0 || out->numBones > 512 ) {
		return qfalse;
	}
	return qtrue;
}

qboolean R_Tiki_Parse( const char *buf, int bufLen, tikiDef_t *out, char *err, int errSize ) {
	char line[1024];
	int i = 0;
	int section = 0; /* 0 none, 1 setup, 2 animations, 3 init */
	tikiAnim_t *curAnim = NULL;
	int braceDepth = 0;

	if ( err && errSize > 0 ) {
		err[0] = '\0';
	}
	if ( !buf || bufLen <= 0 || !out ) {
		if ( err && errSize > 0 ) {
			Q_strncpyz( err, "invalid args", errSize );
		}
		return qfalse;
	}
	if ( bufLen > TIKI_MAX_FILE ) {
		if ( err && errSize > 0 ) {
			Q_strncpyz( err, "file too large", errSize );
		}
		return qfalse;
	}

	Com_Memset( out, 0, sizeof( *out ) );
	out->scale = 1.0f;
	out->used = qtrue;

	while ( i < bufLen ) {
		int j = 0;
		char *tok;
		char *rest;

		while ( i < bufLen && ( buf[i] == '\n' || buf[i] == '\r' ) ) {
			i++;
		}
		if ( i >= bufLen ) {
			break;
		}
		j = 0;
		while ( i < bufLen && buf[i] != '\n' && buf[i] != '\r' && j < (int)sizeof( line ) - 1 ) {
			line[j++] = buf[i++];
		}
		line[j] = '\0';
		Tiki_Trim( line );
		if ( !line[0] || line[0] == '#' || line[0] == '/' ) {
			continue;
		}

		/* brace tracking for nested animation blocks */
		{
			const char *p;
			for ( p = line; *p; p++ ) {
				if ( *p == '{' ) {
					braceDepth++;
				} else if ( *p == '}' ) {
					braceDepth--;
					if ( braceDepth < 0 ) {
						braceDepth = 0;
					}
					if ( braceDepth == 0 ) {
						curAnim = NULL;
					}
				}
			}
		}

		tok = line;
		rest = line;
		while ( *rest && *rest != ' ' && *rest != '\t' ) {
			rest++;
		}
		if ( *rest ) {
			*rest++ = '\0';
			Tiki_Trim( rest );
		}

		if ( !Q_stricmp( tok, "setup" ) ) {
			section = 1;
			curAnim = NULL;
			continue;
		}
		if ( !Q_stricmp( tok, "animations" ) || !Q_stricmp( tok, "anim" ) ) {
			section = 2;
			continue;
		}
		if ( !Q_stricmp( tok, "init" ) ) {
			section = 3;
			continue;
		}

		if ( section == 1 || section == 3 ) {
			if ( !Q_stricmp( tok, "skelmodel" ) || !Q_stricmp( tok, "skeleton" ) ) {
				if ( !Tiki_PathSafe( rest ) ) {
					if ( err && errSize > 0 ) {
						Q_strncpyz( err, "unsafe skelmodel path", errSize );
					}
					return qfalse;
				}
				Q_strncpyz( out->skelmodel, rest, sizeof( out->skelmodel ) );
			} else if ( !Q_stricmp( tok, "path" ) || !Q_stricmp( tok, "model" ) || !Q_stricmp( tok, "mesh" ) ) {
				if ( !Tiki_PathSafe( rest ) ) {
					if ( err && errSize > 0 ) {
						Q_strncpyz( err, "unsafe mesh path", errSize );
					}
					return qfalse;
				}
				Q_strncpyz( out->mesh, rest, sizeof( out->mesh ) );
			} else if ( !Q_stricmp( tok, "scale" ) ) {
				out->scale = (float)atof( rest );
				if ( out->scale <= 0.0f ) {
					out->scale = 1.0f;
				}
			} else if ( !Q_stricmp( tok, "origin" ) ) {
				sscanf( rest, "%f %f %f", &out->origin[0], &out->origin[1], &out->origin[2] );
			} else if ( !Q_stricmp( tok, "lod" ) ) {
				if ( out->numLods < TIKI_MAX_LODS ) {
					char path[TIKI_PATH_SIZE];
					float dist = 0.0f;
					if ( sscanf( rest, "%s %f", path, &dist ) >= 1 && Tiki_PathSafe( path ) ) {
						Q_strncpyz( out->lods[out->numLods].path, path, sizeof( out->lods[0].path ) );
						out->lods[out->numLods].distance = dist;
						out->numLods++;
					}
				}
			} else if ( !Q_stricmp( tok, "surface" ) ) {
				if ( out->numSurfaces < TIKI_MAX_SURFACE ) {
					char name[TIKI_NAME_SIZE];
					char shader[MAX_QPATH];
					if ( sscanf( rest, "%s %s", name, shader ) == 2 ) {
						Q_strncpyz( out->surfaces[out->numSurfaces].name, name, sizeof( out->surfaces[0].name ) );
						Q_strncpyz( out->surfaces[out->numSurfaces].shader, shader, sizeof( out->surfaces[0].shader ) );
						out->numSurfaces++;
					}
				}
			}
			continue;
		}

		if ( section == 2 ) {
			/* animation alias: name { ... }  or  alias name path */
			if ( !Q_stricmp( tok, "{" ) || !Q_stricmp( tok, "}" ) ) {
			continue;
		}
		if ( !Q_stricmp( tok, "alias" ) ) {
				char aname[TIKI_NAME_SIZE];
				char apath[TIKI_PATH_SIZE];
				if ( sscanf( rest, "%s %s", aname, apath ) == 2 && out->numAnims < TIKI_MAX_ANIMS && Tiki_PathSafe( apath ) ) {
					tikiAnim_t *a = &out->anims[out->numAnims++];
					Com_Memset( a, 0, sizeof( *a ) );
					Q_strncpyz( a->name, aname, sizeof( a->name ) );
					Q_strncpyz( a->alias, aname, sizeof( a->alias ) );
					Q_strncpyz( a->path, apath, sizeof( a->path ) );
					a->frameRate = 20.0f;
					a->used = qtrue;
					curAnim = a;
				}
				continue;
			}
			if ( rest[0] == '{' || ( rest[0] == '\0' && braceDepth > 0 ) ) {
				/* "walk {" style */
				if ( out->numAnims < TIKI_MAX_ANIMS ) {
					tikiAnim_t *a = &out->anims[out->numAnims++];
					Com_Memset( a, 0, sizeof( *a ) );
					Q_strncpyz( a->name, tok, sizeof( a->name ) );
					Q_strncpyz( a->alias, tok, sizeof( a->alias ) );
					a->frameRate = 20.0f;
					a->used = qtrue;
					curAnim = a;
				}
				continue;
			}
			if ( curAnim ) {
				if ( !Q_stricmp( tok, "path" ) || !Q_stricmp( tok, "file" ) || !Q_stricmp( tok, "tan" ) ) {
					if ( Tiki_PathSafe( rest ) ) {
						Q_strncpyz( curAnim->path, rest, sizeof( curAnim->path ) );
					}
				} else if ( !Q_stricmp( tok, "firstframe" ) || !Q_stricmp( tok, "first" ) ) {
					curAnim->firstFrame = atoi( rest );
				} else if ( !Q_stricmp( tok, "numframes" ) || !Q_stricmp( tok, "frames" ) ) {
					curAnim->numFrames = atoi( rest );
				} else if ( !Q_stricmp( tok, "framerate" ) || !Q_stricmp( tok, "rate" ) ) {
					curAnim->frameRate = (float)atof( rest );
				} else if ( !Q_stricmp( tok, "cmd" ) || !Q_stricmp( tok, "server" ) || !Q_stricmp( tok, "client" ) ) {
					int frame = 0;
					char cmd[32];
					char arg[MAX_QPATH];
					arg[0] = '\0';
					if ( sscanf( rest, "%d %31s %63s", &frame, cmd, arg ) >= 2 ) {
						if ( R_Tiki_IsAllowedFrameCmd( cmd ) && curAnim->numCmds < TIKI_MAX_FRAMECMDS ) {
							curAnim->cmds[curAnim->numCmds].frame = frame;
							Q_strncpyz( curAnim->cmds[curAnim->numCmds].cmd, cmd, sizeof( curAnim->cmds[0].cmd ) );
							Q_strncpyz( curAnim->cmds[curAnim->numCmds].arg, arg, sizeof( curAnim->cmds[0].arg ) );
							curAnim->numCmds++;
						}
					}
				}
			} else if ( tok[0] && rest[0] && Tiki_PathSafe( rest ) && out->numAnims < TIKI_MAX_ANIMS ) {
				/* bare "walk models/x.tan" */
				tikiAnim_t *a = &out->anims[out->numAnims++];
				Com_Memset( a, 0, sizeof( *a ) );
				Q_strncpyz( a->name, tok, sizeof( a->name ) );
				Q_strncpyz( a->alias, tok, sizeof( a->alias ) );
				Q_strncpyz( a->path, rest, sizeof( a->path ) );
				a->frameRate = 20.0f;
				a->used = qtrue;
			}
		}
	}

	if ( !out->mesh[0] && !out->skelmodel[0] && out->numLods == 0 ) {
		if ( err && errSize > 0 ) {
			Q_strncpyz( err, "no mesh/skelmodel", errSize );
		}
		return qfalse;
	}
	return qtrue;
}
