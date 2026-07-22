/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Clean-room Babble graph parser. Line-oriented interchange format:

  graph <name>
  start <node>
  node <name>
    speaker <id>
    loc <key>
    voice <path>
    duration <sec>
    next <node>
    choice <label> <nextNode>
    choice_loc <key> <nextNode>
===========================================================================
*/

#include "babble.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define BABBLE_MAX_FILE ( 512 * 1024 )
#define BABBLE_MAX_INCLUDE_DEPTH 8

static void Babble_Trim( char *s ) {
	char *e;
	while ( *s == ' ' || *s == '\t' || *s == '\r' ) {
		memmove( s, s + 1, strlen( s ) );
	}
	e = s + strlen( s );
	while ( e > s && ( e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' ) ) {
		*--e = '\0';
	}
}

static qboolean Babble_PathSafe( const char *path ) {
	if ( !path || !path[0] ) {
		return qfalse;
	}
	if ( strstr( path, ".." ) || path[0] == '/' || path[0] == '\\' ) {
		return qfalse;
	}
	if ( strchr( path, ':' ) ) {
		return qfalse;
	}
	return qtrue;
}

static babbleGraph_t *Babble_EnsureGraph( babbleGraph_t *graphs, int maxGraphs, int *count, const char *name ) {
	int i;
	for ( i = 0; i < *count; i++ ) {
		if ( !Q_stricmp( graphs[i].name, name ) ) {
			return &graphs[i];
		}
	}
	if ( *count >= maxGraphs ) {
		return NULL;
	}
	Com_Memset( &graphs[*count], 0, sizeof( graphs[0] ) );
	Q_strncpyz( graphs[*count].name, name, sizeof( graphs[0].name ) );
	graphs[*count].used = qtrue;
	return &graphs[(*count)++];
}

static babbleNode_t *Babble_EnsureNode( babbleGraph_t *g, const char *name ) {
	int i;
	for ( i = 0; i < g->numNodes; i++ ) {
		if ( !Q_stricmp( g->nodes[i].name, name ) ) {
			return &g->nodes[i];
		}
	}
	if ( g->numNodes >= BABBLE_MAX_NODES ) {
		return NULL;
	}
	Com_Memset( &g->nodes[g->numNodes], 0, sizeof( g->nodes[0] ) );
	Q_strncpyz( g->nodes[g->numNodes].name, name, sizeof( g->nodes[0].name ) );
	g->nodes[g->numNodes].used = qtrue;
	g->nodes[g->numNodes].duration = 2.0f;
	g->nodes[g->numNodes].weight = 1.0f;
	return &g->nodes[g->numNodes++];
}

int Babble_ParseBuffer( const char *buf, int bufLen, babbleGraph_t *graphs, int maxGraphs, char *err, int errSize ) {
	char line[1024];
	int i = 0;
	int graphCount = 0;
	babbleGraph_t *curG = NULL;
	babbleNode_t *curN = NULL;

	if ( err && errSize > 0 ) {
		err[0] = '\0';
	}
	if ( !buf || bufLen <= 0 || !graphs || maxGraphs <= 0 ) {
		if ( err && errSize > 0 ) {
			Q_strncpyz( err, "invalid args", errSize );
		}
		return 0;
	}
	if ( bufLen > BABBLE_MAX_FILE ) {
		if ( err && errSize > 0 ) {
			Q_strncpyz( err, "file too large", errSize );
		}
		return 0;
	}

	Com_Memset( graphs, 0, sizeof( graphs[0] ) * maxGraphs );

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
		Babble_Trim( line );
		if ( !line[0] || line[0] == '#' || line[0] == ';' ) {
			continue;
		}

		tok = line;
		rest = line;
		while ( *rest && *rest != ' ' && *rest != '\t' ) {
			rest++;
		}
		if ( *rest ) {
			*rest++ = '\0';
			Babble_Trim( rest );
		}

		if ( !Q_stricmp( tok, "graph" ) ) {
			curG = Babble_EnsureGraph( graphs, maxGraphs, &graphCount, rest );
			curN = NULL;
			if ( !curG ) {
				if ( err && errSize > 0 ) {
					Q_strncpyz( err, "too many graphs", errSize );
				}
				return 0;
			}
			continue;
		}
		if ( !curG ) {
			if ( err && errSize > 0 ) {
				Q_strncpyz( err, "directive before graph", errSize );
			}
			return 0;
		}
		if ( !Q_stricmp( tok, "start" ) ) {
			Q_strncpyz( curG->startNode, rest, sizeof( curG->startNode ) );
			continue;
		}
		if ( !Q_stricmp( tok, "node" ) ) {
			curN = Babble_EnsureNode( curG, rest );
			if ( !curN ) {
				if ( err && errSize > 0 ) {
					Q_strncpyz( err, "too many nodes", errSize );
				}
				return 0;
			}
			if ( !curG->startNode[0] ) {
				Q_strncpyz( curG->startNode, rest, sizeof( curG->startNode ) );
			}
			continue;
		}
		if ( !curN ) {
			if ( err && errSize > 0 ) {
				Q_strncpyz( err, "field before node", errSize );
			}
			return 0;
		}
		if ( !Q_stricmp( tok, "speaker" ) ) {
			Q_strncpyz( curN->speaker, rest, sizeof( curN->speaker ) );
		} else if ( !Q_stricmp( tok, "loc" ) || !Q_stricmp( tok, "text_key" ) ) {
			Q_strncpyz( curN->locKey, rest, sizeof( curN->locKey ) );
		} else if ( !Q_stricmp( tok, "voice" ) || !Q_stricmp( tok, "sound" ) ) {
			if ( !Babble_PathSafe( rest ) ) {
				if ( err && errSize > 0 ) {
					Q_strncpyz( err, "unsafe voice path", errSize );
				}
				return 0;
			}
			Q_strncpyz( curN->voice, rest, sizeof( curN->voice ) );
		} else if ( !Q_stricmp( tok, "anim" ) || !Q_stricmp( tok, "animation" ) ) {
			Q_strncpyz( curN->anim, rest, sizeof( curN->anim ) );
		} else if ( !Q_stricmp( tok, "next" ) ) {
			Q_strncpyz( curN->nextNode, rest, sizeof( curN->nextNode ) );
		} else if ( !Q_stricmp( tok, "duration" ) ) {
			curN->duration = (float)atof( rest );
			if ( curN->duration < 0.0f ) {
				curN->duration = 0.0f;
			}
			if ( curN->duration > 600.0f ) {
				curN->duration = 600.0f;
			}
		} else if ( !Q_stricmp( tok, "weight" ) ) {
			curN->weight = (float)atof( rest );
		} else if ( !Q_stricmp( tok, "cooldown" ) ) {
			curN->cooldown = (float)atof( rest );
		} else if ( !Q_stricmp( tok, "choice" ) || !Q_stricmp( tok, "choice_loc" ) ) {
			char label[BABBLE_CHOICE_LABEL];
			char *nextTok;
			if ( curN->numChoices >= BABBLE_MAX_CHOICES ) {
				continue;
			}
			Q_strncpyz( label, rest, sizeof( label ) );
			nextTok = label + strlen( label );
			while ( nextTok > label && nextTok[-1] != ' ' && nextTok[-1] != '\t' ) {
				nextTok--;
			}
			if ( nextTok > label && ( nextTok[-1] == ' ' || nextTok[-1] == '\t' ) ) {
				nextTok[-1] = '\0';
				Babble_Trim( label );
				Babble_Trim( nextTok );
				if ( !Q_stricmp( tok, "choice_loc" ) ) {
					Q_strncpyz( curN->choices[curN->numChoices].labelLoc, label,
						sizeof( curN->choices[0].labelLoc ) );
					curN->choices[curN->numChoices].label[0] = '\0';
				} else {
					Q_strncpyz( curN->choices[curN->numChoices].label, label,
						sizeof( curN->choices[0].label ) );
				}
				Q_strncpyz( curN->choices[curN->numChoices].nextNode, nextTok,
					sizeof( curN->choices[0].nextNode ) );
				curN->numChoices++;
			}
		} else {
			/* unknown field: ignore for forward compat */
		}
	}

	return graphCount;
}
