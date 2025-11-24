/*
===========================================================================
Client-side Dialog System Header

Renders dialog boxes with text, character names, and choices.
===========================================================================
*/

#ifndef _CG_DIALOG_H
#define _CG_DIALOG_H

#include "../qcommon/q_shared.h"
#include "cg_public.h"

#define MAX_DIALOG_CHOICES	4
#define MAX_DIALOG_TEXT		256
#define MAX_DIALOG_NAME		64

typedef struct {
	int id;
	int pageNum;
	char speaker[ MAX_DIALOG_NAME ];
	char text[ MAX_DIALOG_TEXT ];
	int numChoices;
	char choices[ MAX_DIALOG_CHOICES ][ MAX_DIALOG_TEXT ];
	int choiceTargets[ MAX_DIALOG_CHOICES ];
	qboolean active;
	int startTime;
} cg_dialog_t;

// Dialog functions
void CG_Dialog_Init( void );
void CG_Dialog_Shutdown( void );
void CG_Dialog_Show( int id, int pageNum, const char *speaker, const char *text, 
                     int numChoices, const char *choices[], const int targets[] );
void CG_Dialog_Close( void );
void CG_Dialog_Draw( void );
qboolean CG_Dialog_IsActive( void );
void CG_Dialog_HandleInput( int key );

#endif

