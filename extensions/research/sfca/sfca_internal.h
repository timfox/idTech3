#pragma once

#include "sfca/sfca.h"

#define SFCA_MAX_CELLS 8192
#define SFCA_MAX_HISTORY 4096

typedef struct {
	int height;
	int width;
	byte cur[SFCA_MAX_CELLS];
	byte nxt[SFCA_MAX_CELLS];
	int colSum[256];
	int rowSum[256];
	int colBlur[256];
	int rowBlur[256];
	float field[SFCA_MAX_CELLS];
	float qField[SFCA_MAX_CELLS];
} sfca_workspace_t;

void SFCA_ComputeField( sfca_workspace_t *ws, const sfca_intervals_t *iv );
void SFCA_Step( sfca_workspace_t *ws, const sfca_intervals_t *iv );
float SFCA_ChangeRate( const byte *a, const byte *b, int height, int width );

int SFCA_InInterval( float q, float lo, float hi );
