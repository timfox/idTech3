/*
===========================================================================
Dialog System Header

A text-based dialog system for displaying multi-page conversations
with character names and optional choices.
===========================================================================
*/

#ifndef _G_DIALOG_H
#define _G_DIALOG_H

#include "g_local.h"

#define MAX_DIALOG_PAGES		16
#define MAX_DIALOG_TEXT			256
#define MAX_DIALOG_NAME			64
#define MAX_DIALOG_CHOICES		4
#define MAX_DIALOG_CHOICE_TEXT	64

typedef struct dialog_choice {
	char text[ MAX_DIALOG_CHOICE_TEXT ];
	int target;		// Entity number to trigger when selected, or -1 for none
} dialog_choice_t;

typedef struct dialog_page {
	char text[ MAX_DIALOG_TEXT ];
	char speaker[ MAX_DIALOG_NAME ];
	int numChoices;
	dialog_choice_t choices[ MAX_DIALOG_CHOICES ];
} dialog_page_t;

typedef struct dialog {
	int id;							// Unique dialog ID
	int numPages;
	dialog_page_t pages[ MAX_DIALOG_PAGES ];
	int currentPage;
	qboolean active;
	int clientNum;					// Client viewing this dialog
} dialog_t;

// Dialog management
void G_Dialog_Init( void );
void G_Dialog_Shutdown( void );
int G_Dialog_Create( int clientNum, const char *speaker, const char *text );
void G_Dialog_AddPage( int dialogId, const char *speaker, const char *text );
void G_Dialog_AddChoice( int dialogId, int pageNum, const char *text, int target );
void G_Dialog_Show( int dialogId, int clientNum );
void G_Dialog_Close( int clientNum );
void G_Dialog_NextPage( int clientNum );
void G_Dialog_SelectChoice( int clientNum, int choiceNum );

// Entity support
void Use_Target_Dialog( gentity_t *ent, gentity_t *other, gentity_t *activator );
void SP_target_dialog( gentity_t *ent );

#endif

