/*
===========================================================================
Client-side Dialog System Implementation

Renders dialog boxes with text, character names, and choices.
===========================================================================
*/

#include "cg_local.h"
#include "cg_dialog.h"
#include <keycodes.h>

static cg_dialog_t cg_dialog;

/*
================
CG_Dialog_Init
Initialize dialog system
================
*/
void CG_Dialog_Init( void ) {
	memset( &cg_dialog, 0, sizeof( cg_dialog ) );
	cg_dialog.id = -1;
	cg_dialog.active = qfalse;
}

/*
================
CG_Dialog_Shutdown
Clean up dialog system
================
*/
void CG_Dialog_Shutdown( void ) {
	CG_Dialog_Close();
}

/*
================
CG_Dialog_Show
Show a dialog page
================
*/
void CG_Dialog_Show( int id, int pageNum, const char *speaker, const char *text, 
                     int numChoices, const char *choices[], const int targets[] ) {
	int i;
	
	cg_dialog.id = id;
	cg_dialog.pageNum = pageNum;
	cg_dialog.active = qtrue;
	cg_dialog.startTime = cg.time;
	
	if( speaker ) {
		Q_strncpyz( cg_dialog.speaker, speaker, sizeof( cg_dialog.speaker ) );
	} else {
		cg_dialog.speaker[ 0 ] = '\0';
	}
	
	if( text ) {
		Q_strncpyz( cg_dialog.text, text, sizeof( cg_dialog.text ) );
	} else {
		cg_dialog.text[ 0 ] = '\0';
	}
	
	cg_dialog.numChoices = numChoices;
	if( numChoices > MAX_DIALOG_CHOICES ) {
		numChoices = MAX_DIALOG_CHOICES;
	}
	
	for( i = 0; i < numChoices; i++ ) {
		if( choices[ i ] ) {
			Q_strncpyz( cg_dialog.choices[ i ], choices[ i ], sizeof( cg_dialog.choices[ i ] ) );
		} else {
			cg_dialog.choices[ i ][ 0 ] = '\0';
		}
		if( targets ) {
			cg_dialog.choiceTargets[ i ] = targets[ i ];
		} else {
			cg_dialog.choiceTargets[ i ] = -1;
		}
	}
}

/*
================
CG_Dialog_Close
Close the current dialog
================
*/
void CG_Dialog_Close( void ) {
	cg_dialog.active = qfalse;
	cg_dialog.id = -1;
}

/*
================
CG_Dialog_IsActive
Check if dialog is currently active
================
*/
qboolean CG_Dialog_IsActive( void ) {
	return cg_dialog.active;
}

/*
================
CG_Dialog_Draw
Draw the dialog box
================
*/
void CG_Dialog_Draw( void ) {
	int x, y, w, h;
	int textX, textY;
	int choiceY;
	int i;
	float color[4];
	float boxColor[4];
	char *textPtr;
	char lineBuffer[ 256 ];
	int lineHeight;
	int maxWidth;
	
	if( !cg_dialog.active ) {
		return;
	}
	
	// Dialog box dimensions (bottom of screen, virtual 640x480)
	maxWidth = 600;
	h = 120;
	w = maxWidth;
	x = ( 640 - w ) / 2;
	y = 480 - h - 20;
	
	// Draw dialog box background (semi-transparent black)
	boxColor[0] = 0.0f;
	boxColor[1] = 0.0f;
	boxColor[2] = 0.0f;
	boxColor[3] = 0.8f;
	trap_R_SetColor( boxColor );
	CG_DrawPic( x, y, w, h, cgs.media.whiteShader );
	
	// Draw border
	color[0] = 1.0f;
	color[1] = 1.0f;
	color[2] = 1.0f;
	color[3] = 1.0f;
	CG_DrawRect( x, y, w, h, 2.0f, color );
	
	// Draw speaker name if present
	textY = y + 12;
	if( cg_dialog.speaker[ 0 ] ) {
		textX = x + 16;
		CG_DrawStringExt( textX, textY, cg_dialog.speaker, color, qfalse, qtrue, 
		                  BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		textY += BIGCHAR_HEIGHT + 4;
	}
	
	// Draw dialog text (word wrap)
	textX = x + 16;
	textPtr = cg_dialog.text;
	lineHeight = BIGCHAR_HEIGHT;
	
	while( *textPtr && textY < y + h - ( cg_dialog.numChoices > 0 ? 40 : 20 ) ) {
		int lineLen = 0;
		char *lineStart = textPtr;
		char *lastSpace = NULL;
		int lastSpacePos = 0;
		
		// Find line break or word wrap
		while( *textPtr && lineLen < ( maxWidth - 32 ) / BIGCHAR_WIDTH ) {
			if( *textPtr == '\n' ) {
				textPtr++;
				break;
			}
			if( *textPtr == ' ' ) {
				lastSpace = textPtr;
				lastSpacePos = lineLen;
			}
			lineLen++;
			textPtr++;
		}
		
		// Word wrap if needed
		if( *textPtr && *textPtr != '\n' && lastSpace ) {
			textPtr = lastSpace + 1;
			lineLen = lastSpacePos;
		}
		
		// Copy line to buffer
		Q_strncpyz( lineBuffer, lineStart, lineLen + 1 );
		lineBuffer[ lineLen ] = '\0';
		
		// Draw line
		CG_DrawStringExt( textX, textY, lineBuffer, color, qfalse, qtrue,
		                  BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
		textY += lineHeight;
	}
	
	// Draw choices if present
	if( cg_dialog.numChoices > 0 ) {
		choiceY = y + h - 8 - ( cg_dialog.numChoices * ( lineHeight + 4 ) );
		for( i = 0; i < cg_dialog.numChoices; i++ ) {
			if( cg_dialog.choices[ i ][ 0 ] ) {
				char choiceText[ MAX_DIALOG_TEXT + 8 ];
				Com_sprintf( choiceText, sizeof( choiceText ), "%d. %s", i + 1, cg_dialog.choices[ i ] );
				CG_DrawStringExt( textX, choiceY, choiceText, color, qfalse, qtrue,
				                  BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
				choiceY += lineHeight + 4;
			}
		}
	} else {
		// Draw "Press USE to continue" hint
		char *hint = "Press USE to continue";
		int hintW = CG_DrawStrlen( hint ) * BIGCHAR_WIDTH;
		CG_DrawStringExt( x + w - hintW - 16, y + h - BIGCHAR_HEIGHT - 8, 
		                  hint, color, qfalse, qtrue, BIGCHAR_WIDTH, BIGCHAR_HEIGHT, 0 );
	}
	
	trap_R_SetColor( NULL );
}

/*
================
CG_Dialog_HandleInput
Handle input for dialog (USE key advances, number keys select choices)
================
*/
void CG_Dialog_HandleInput( int key ) {
	if( !cg_dialog.active ) {
		return;
	}
	
	// Number keys 1-4 select choices
	if( key >= '1' && key <= '4' ) {
		int choiceNum = key - '1';
		if( choiceNum < cg_dialog.numChoices ) {
			trap_SendClientCommand( va( "dialogchoice %d", choiceNum ) );
			return;
		}
	}
	
	// USE key (E) or ENTER advances dialog
	if( key == K_ENTER || key == K_KP_ENTER || key == K_E ) {
		if( cg_dialog.numChoices == 0 ) {
			trap_SendClientCommand( "dialognext" );
		}
	}
}

