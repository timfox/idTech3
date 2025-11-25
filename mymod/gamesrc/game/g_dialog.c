/*
===========================================================================
Dialog System Implementation

A text-based dialog system for displaying multi-page conversations
with character names and optional choices.
===========================================================================
*/

#include "g_local.h"
#include "g_dialog.h"
#ifdef USE_ENTT
#include "g_ecs.h"
#include "g_ecs_mod_components.h"
#include "../../../../src/qcommon/ecs.cpp" // For ECS namespace access
#include <entt/entt.hpp>
#endif

#define MAX_DIALOGS		32

static dialog_t dialogs[ MAX_DIALOGS ];
static int nextDialogId = 1;

/*
================
G_Dialog_Init
Initialize the dialog system
================
*/
void G_Dialog_Init( void ) {
	int i;
	
	memset( dialogs, 0, sizeof( dialogs ) );
	nextDialogId = 1;
	
	for( i = 0; i < MAX_DIALOGS; i++ ) {
		dialogs[ i ].id = -1;
		dialogs[ i ].active = qfalse;
		dialogs[ i ].clientNum = -1;
	}
}

/*
================
G_Dialog_Shutdown
Clean up dialogs on level shutdown
================
*/
void G_Dialog_Shutdown( void ) {
	int i;
	
	for( i = 0; i < MAX_DIALOGS; i++ ) {
		if( dialogs[ i ].active && dialogs[ i ].clientNum >= 0 ) {
			G_Dialog_Close( dialogs[ i ].clientNum );
		}
	}
	
	memset( dialogs, 0, sizeof( dialogs ) );
}

/*
================
G_Dialog_FindFreeSlot
Find a free dialog slot
================
*/
static int G_Dialog_FindFreeSlot( void ) {
	int i;
	
	for( i = 0; i < MAX_DIALOGS; i++ ) {
		if( dialogs[ i ].id == -1 ) {
			return i;
		}
	}
	
	return -1;
}

/*
================
G_Dialog_FindById
Find a dialog by ID
================
*/
static dialog_t *G_Dialog_FindById( int dialogId ) {
	int i;
	
	for( i = 0; i < MAX_DIALOGS; i++ ) {
		if( dialogs[ i ].id == dialogId ) {
			return &dialogs[ i ];
		}
	}
	
	return NULL;
}

/*
================
G_Dialog_FindByClient
Find active dialog for a client
================
*/
static dialog_t *G_Dialog_FindByClient( int clientNum ) {
	int i;
	
	for( i = 0; i < MAX_DIALOGS; i++ ) {
		if( dialogs[ i ].active && dialogs[ i ].clientNum == clientNum ) {
			return &dialogs[ i ];
		}
	}
	
	return NULL;
}

/*
================
G_Dialog_Create
Create a new dialog
================
*/
int G_Dialog_Create( int clientNum, const char *speaker, const char *text ) {
	int slot;
	dialog_t *dialog;
	
	slot = G_Dialog_FindFreeSlot();
	if( slot == -1 ) {
		G_Printf( "G_Dialog_Create: No free dialog slots\n" );
		return -1;
	}
	
	dialog = &dialogs[ slot ];
	memset( dialog, 0, sizeof( dialog_t ) );
	
	dialog->id = nextDialogId++;
	dialog->clientNum = clientNum;
	dialog->numPages = 1;
	dialog->currentPage = 0;
	dialog->active = qfalse;
	
	if( speaker ) {
		Q_strncpyz( dialog->pages[ 0 ].speaker, speaker, sizeof( dialog->pages[ 0 ].speaker ) );
	}
	if( text ) {
		Q_strncpyz( dialog->pages[ 0 ].text, text, sizeof( dialog->pages[ 0 ].text ) );
	}
	
	return dialog->id;
}

/*
================
G_Dialog_AddPage
Add a page to an existing dialog
================
*/
void G_Dialog_AddPage( int dialogId, const char *speaker, const char *text ) {
	dialog_t *dialog;
	
	dialog = G_Dialog_FindById( dialogId );
	if( !dialog ) {
		G_Printf( "G_Dialog_AddPage: Dialog %d not found\n", dialogId );
		return;
	}
	
	if( dialog->numPages >= MAX_DIALOG_PAGES ) {
		G_Printf( "G_Dialog_AddPage: Dialog %d has too many pages\n", dialogId );
		return;
	}
	
	if( speaker ) {
		Q_strncpyz( dialog->pages[ dialog->numPages ].speaker, speaker, 
		            sizeof( dialog->pages[ dialog->numPages ].speaker ) );
	}
	if( text ) {
		Q_strncpyz( dialog->pages[ dialog->numPages ].text, text, 
		            sizeof( dialog->pages[ dialog->numPages ].text ) );
	}
	
	dialog->numPages++;
}

/*
================
G_Dialog_AddChoice
Add a choice to a dialog page
================
*/
void G_Dialog_AddChoice( int dialogId, int pageNum, const char *text, int target ) {
	dialog_t *dialog;
	dialog_page_t *page;
	
	dialog = G_Dialog_FindById( dialogId );
	if( !dialog ) {
		G_Printf( "G_Dialog_AddChoice: Dialog %d not found\n", dialogId );
		return;
	}
	
	if( pageNum < 0 || pageNum >= dialog->numPages ) {
		G_Printf( "G_Dialog_AddChoice: Invalid page %d for dialog %d\n", pageNum, dialogId );
		return;
	}
	
	page = &dialog->pages[ pageNum ];
	if( page->numChoices >= MAX_DIALOG_CHOICES ) {
		G_Printf( "G_Dialog_AddChoice: Page %d has too many choices\n", pageNum );
		return;
	}
	
	if( text ) {
		Q_strncpyz( page->choices[ page->numChoices ].text, text, 
		            sizeof( page->choices[ page->numChoices ].text ) );
	}
	page->choices[ page->numChoices ].target = target;
	page->numChoices++;
}

/*
================
G_Dialog_SendToClient
Send dialog data to client
================
*/
static void G_Dialog_SendToClient( dialog_t *dialog ) {
	dialog_page_t *page;
	char cmd[ MAX_STRING_CHARS ];
	int i;
	
	if( !dialog || dialog->currentPage < 0 || dialog->currentPage >= dialog->numPages ) {
		return;
	}
	
	page = &dialog->pages[ dialog->currentPage ];
	
	// Send dialog command: dialog <id> <page> <speaker> <text> <numChoices> [choices...]
	Com_sprintf( cmd, sizeof( cmd ), "dialog %d %d \"%s\" \"%s\" %d", 
	             dialog->id, dialog->currentPage, page->speaker, page->text, page->numChoices );
	
	for( i = 0; i < page->numChoices; i++ ) {
		Q_strcat( cmd, sizeof( cmd ), va( " \"%s\" %d", 
		         page->choices[ i ].text, page->choices[ i ].target ) );
	}
	
	trap_SendServerCommand( dialog->clientNum, cmd );
}

/*
================
G_Dialog_Show
Show a dialog to a client
================
*/
void G_Dialog_Show( int dialogId, int clientNum ) {
	dialog_t *dialog;
	dialog_t *existing;
	
	if( clientNum < 0 || clientNum >= level.maxclients ) {
		return;
	}
	
	// Close any existing dialog for this client
	existing = G_Dialog_FindByClient( clientNum );
	if( existing ) {
		G_Dialog_Close( clientNum );
	}
	
	dialog = G_Dialog_FindById( dialogId );
	if( !dialog ) {
		G_Printf( "G_Dialog_Show: Dialog %d not found\n", dialogId );
		return;
	}
	
	dialog->clientNum = clientNum;
	dialog->currentPage = 0;
	dialog->active = qtrue;
	
	G_Dialog_SendToClient( dialog );
}

/*
================
G_Dialog_Close
Close a dialog for a client
================
*/
void G_Dialog_Close( int clientNum ) {
	dialog_t *dialog;
	
	dialog = G_Dialog_FindByClient( clientNum );
	if( !dialog ) {
		return;
	}
	
	trap_SendServerCommand( clientNum, "dialogclose" );
	dialog->active = qfalse;
	dialog->clientNum = -1;
}

/*
================
G_Dialog_NextPage
Advance to next page of dialog
================
*/
void G_Dialog_NextPage( int clientNum ) {
	dialog_t *dialog;
	
	dialog = G_Dialog_FindByClient( clientNum );
	if( !dialog ) {
		return;
	}
	
	dialog->currentPage++;
	if( dialog->currentPage >= dialog->numPages ) {
		// Dialog finished
		G_Dialog_Close( clientNum );
		return;
	}
	
	G_Dialog_SendToClient( dialog );
}

/*
================
G_Dialog_SelectChoice
Select a choice in the current dialog page
================
*/
void G_Dialog_SelectChoice( int clientNum, int choiceNum ) {
	dialog_t *dialog;
	dialog_page_t *page;
	gentity_t *target;
	
	dialog = G_Dialog_FindByClient( clientNum );
	if( !dialog ) {
		return;
	}
	
	if( dialog->currentPage < 0 || dialog->currentPage >= dialog->numPages ) {
		return;
	}
	
	page = &dialog->pages[ dialog->currentPage ];
	if( choiceNum < 0 || choiceNum >= page->numChoices ) {
		return;
	}
	
	// Trigger target entity if specified
	if( page->choices[ choiceNum ].target >= 0 && 
	    page->choices[ choiceNum ].target < MAX_GENTITIES ) {
		target = &g_entities[ page->choices[ choiceNum ].target ];
		if( target && target->use ) {
			target->use( target, target, &g_entities[ clientNum ] );
		}
	}
	
	// Advance to next page or close
	G_Dialog_NextPage( clientNum );
}

/*
================
Use_Target_Dialog
Entity use function for target_dialog
================
*/
void Use_Target_Dialog( gentity_t *ent, gentity_t *other, gentity_t *activator ) {
	(void)other; // Unused parameter - required by function signature
	int dialogId;
	int clientNum;
	
	if( !activator || !activator->client ) {
		return;
	}
	
	clientNum = activator - g_entities;
	
	// Create dialog from entity properties
	dialogId = G_Dialog_Create( clientNum, ent->message2, ent->message );
	if( dialogId == -1 ) {
		return;
	}
	
	// Show the dialog
	G_Dialog_Show( dialogId, clientNum );
}

/*
================
SP_target_dialog
Spawn function for target_dialog entity
================
*/
void SP_target_dialog( gentity_t *ent ) {
	// message = dialog text
	// message2 = speaker name
	// target = entity to trigger when dialog completes
	ent->use = Use_Target_Dialog;
}

