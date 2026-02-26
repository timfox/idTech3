/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

UTF-8 decoding/encoding and built-in emoji registry.

Decoding follows RFC 3629 (restricted to U+0000..U+10FFFF).
The emoji table maps Unicode codepoints to atlas grid positions.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "q_utf8.h"

uint32_t Q_UTF8_Decode( const char **pp ) {
	const unsigned char *s = (const unsigned char *)*pp;
	uint32_t cp;
	int extra;

	if ( s[0] < 0x80 ) {
		cp = s[0];
		extra = 0;
	} else if ( ( s[0] & 0xE0 ) == 0xC0 ) {
		cp = s[0] & 0x1F;
		extra = 1;
	} else if ( ( s[0] & 0xF0 ) == 0xE0 ) {
		cp = s[0] & 0x0F;
		extra = 2;
	} else if ( ( s[0] & 0xF8 ) == 0xF0 ) {
		cp = s[0] & 0x07;
		extra = 3;
	} else {
		*pp = (const char *)( s + 1 );
		return Q_UTF8_INVALID;
	}

	for ( int i = 0; i < extra; i++ ) {
		if ( ( s[1 + i] & 0xC0 ) != 0x80 ) {
			*pp = (const char *)( s + 1 );
			return Q_UTF8_INVALID;
		}
		cp = ( cp << 6 ) | ( s[1 + i] & 0x3F );
	}

	*pp = (const char *)( s + 1 + extra );

	if ( cp > 0x10FFFF || ( cp >= 0xD800 && cp <= 0xDFFF ) ) {
		return Q_UTF8_INVALID;
	}

	return cp;
}

int Q_UTF8_Encode( uint32_t cp, char *out ) {
	if ( cp < 0x80 ) {
		out[0] = (char)cp;
		return 1;
	} else if ( cp < 0x800 ) {
		out[0] = (char)( 0xC0 | ( cp >> 6 ) );
		out[1] = (char)( 0x80 | ( cp & 0x3F ) );
		return 2;
	} else if ( cp < 0x10000 ) {
		out[0] = (char)( 0xE0 | ( cp >> 12 ) );
		out[1] = (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
		out[2] = (char)( 0x80 | ( cp & 0x3F ) );
		return 3;
	} else if ( cp <= 0x10FFFF ) {
		out[0] = (char)( 0xF0 | ( cp >> 18 ) );
		out[1] = (char)( 0x80 | ( ( cp >> 12 ) & 0x3F ) );
		out[2] = (char)( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
		out[3] = (char)( 0x80 | ( cp & 0x3F ) );
		return 4;
	}
	out[0] = '?';
	return 1;
}

int Q_UTF8_Width( const char *s ) {
	unsigned char c = (unsigned char)*s;
	if ( c < 0x80 ) return 1;
	if ( ( c & 0xE0 ) == 0xC0 ) return 2;
	if ( ( c & 0xF0 ) == 0xE0 ) return 3;
	if ( ( c & 0xF8 ) == 0xF0 ) return 4;
	return 1;
}

int Q_UTF8_Strlen( const char *s ) {
	int count = 0;
	while ( *s ) {
		s += Q_UTF8_Width( s );
		count++;
	}
	return count;
}

/* ---- Emoji Registry ---- */

static emojiEntry_t emojiTable[] = {
	/* Smileys & People */
	{ 0x1F600, 0, 0, "grinning" },
	{ 0x1F601, 1, 0, "grin" },
	{ 0x1F602, 2, 0, "joy" },
	{ 0x1F603, 3, 0, "smiley" },
	{ 0x1F604, 4, 0, "smile" },
	{ 0x1F605, 5, 0, "sweat_smile" },
	{ 0x1F606, 6, 0, "laughing" },
	{ 0x1F607, 7, 0, "innocent" },
	{ 0x1F608, 8, 0, "smiling_imp" },
	{ 0x1F609, 9, 0, "wink" },
	{ 0x1F60A, 10, 0, "blush" },
	{ 0x1F60B, 11, 0, "yum" },
	{ 0x1F60C, 12, 0, "relieved" },
	{ 0x1F60D, 13, 0, "heart_eyes" },
	{ 0x1F60E, 14, 0, "sunglasses" },
	{ 0x1F60F, 15, 0, "smirk" },
	{ 0x1F610, 16, 0, "neutral_face" },
	{ 0x1F611, 17, 0, "expressionless" },
	{ 0x1F612, 18, 0, "unamused" },
	{ 0x1F613, 19, 0, "sweat" },
	{ 0x1F614, 20, 0, "pensive" },
	{ 0x1F615, 21, 0, "confused" },
	{ 0x1F616, 22, 0, "confounded" },
	{ 0x1F617, 23, 0, "kissing" },
	{ 0x1F618, 24, 0, "kissing_heart" },
	{ 0x1F619, 25, 0, "kissing_smiling_eyes" },
	{ 0x1F61A, 26, 0, "kissing_closed_eyes" },
	{ 0x1F61B, 27, 0, "stuck_out_tongue" },
	{ 0x1F61C, 28, 0, "stuck_out_tongue_wink" },
	{ 0x1F61D, 29, 0, "stuck_out_tongue_closed" },
	{ 0x1F61E, 30, 0, "disappointed" },
	{ 0x1F61F, 31, 0, "worried" },
	{ 0x1F620, 0, 1, "angry" },
	{ 0x1F621, 1, 1, "rage" },
	{ 0x1F622, 2, 1, "cry" },
	{ 0x1F623, 3, 1, "persevere" },
	{ 0x1F624, 4, 1, "triumph" },
	{ 0x1F625, 5, 1, "disappointed_relieved" },
	{ 0x1F626, 6, 1, "frowning" },
	{ 0x1F627, 7, 1, "anguished" },
	{ 0x1F628, 8, 1, "fearful" },
	{ 0x1F629, 9, 1, "weary" },
	{ 0x1F62A, 10, 1, "sleepy" },
	{ 0x1F62B, 11, 1, "tired_face" },
	{ 0x1F62C, 12, 1, "grimacing" },
	{ 0x1F62D, 13, 1, "sob" },
	{ 0x1F62E, 14, 1, "open_mouth" },
	{ 0x1F62F, 15, 1, "hushed" },
	{ 0x1F630, 16, 1, "cold_sweat" },
	{ 0x1F631, 17, 1, "scream" },
	{ 0x1F632, 18, 1, "astonished" },
	{ 0x1F633, 19, 1, "flushed" },
	{ 0x1F634, 20, 1, "sleeping" },
	{ 0x1F635, 21, 1, "dizzy_face" },
	{ 0x1F636, 22, 1, "no_mouth" },
	{ 0x1F637, 23, 1, "mask" },

	/* Gestures */
	{ 0x1F44D, 24, 1, "thumbsup" },
	{ 0x1F44E, 25, 1, "thumbsdown" },
	{ 0x1F44A, 26, 1, "fist" },
	{ 0x1F44B, 27, 1, "wave" },
	{ 0x1F44F, 28, 1, "clap" },
	{ 0x1F64F, 29, 1, "pray" },
	{ 0x270C,  30, 1, "v" },
	{ 0x1F4AA, 31, 1, "muscle" },

	/* Hearts & Symbols */
	{ 0x2764,  0, 2, "heart" },
	{ 0x1F494, 1, 2, "broken_heart" },
	{ 0x1F495, 2, 2, "two_hearts" },
	{ 0x1F496, 3, 2, "sparkling_heart" },
	{ 0x1F497, 4, 2, "heartpulse" },
	{ 0x1F498, 5, 2, "cupid" },
	{ 0x1F499, 6, 2, "blue_heart" },
	{ 0x1F49A, 7, 2, "green_heart" },
	{ 0x1F49B, 8, 2, "yellow_heart" },
	{ 0x1F49C, 9, 2, "purple_heart" },
	{ 0x2B50,  10, 2, "star" },
	{ 0x1F31F, 11, 2, "star2" },
	{ 0x1F525, 12, 2, "fire" },
	{ 0x1F4A5, 13, 2, "boom" },
	{ 0x1F4A9, 14, 2, "poop" },
	{ 0x1F480, 15, 2, "skull" },
	{ 0x2620,  16, 2, "skull_crossbones" },
	{ 0x1F47B, 17, 2, "ghost" },
	{ 0x1F47E, 18, 2, "space_invader" },
	{ 0x1F916, 19, 2, "robot" },
	{ 0x1F3AE, 20, 2, "video_game" },
	{ 0x1F3AF, 21, 2, "dart" },
	{ 0x1F3C6, 22, 2, "trophy" },
	{ 0x1F3C5, 23, 2, "medal" },

	/* Gaming & Combat */
	{ 0x1F52B, 24, 2, "gun" },
	{ 0x1F4A3, 25, 2, "bomb" },
	{ 0x1F52A, 26, 2, "knife" },
	{ 0x2694,  27, 2, "crossed_swords" },
	{ 0x1F6E1, 28, 2, "shield" },
	{ 0x1F3F9, 29, 2, "bow_and_arrow" },
	{ 0x26A1,  30, 2, "zap" },
	{ 0x1F4A2, 31, 2, "anger" },

	/* Misc Popular */
	{ 0x2705,  0, 3, "white_check_mark" },
	{ 0x274C,  1, 3, "x" },
	{ 0x2757,  2, 3, "exclamation" },
	{ 0x2753,  3, 3, "question" },
	{ 0x1F4AC, 4, 3, "speech_balloon" },
	{ 0x1F4AD, 5, 3, "thought_balloon" },
	{ 0x1F389, 6, 3, "tada" },
	{ 0x1F38A, 7, 3, "confetti_ball" },
	{ 0x1F4AF, 8, 3, "100" },
	{ 0x1F44C, 9, 3, "ok_hand" },
	{ 0x270B,  10, 3, "raised_hand" },
	{ 0x1F91D, 11, 3, "handshake" },
	{ 0x1F440, 12, 3, "eyes" },
	{ 0x1F648, 13, 3, "see_no_evil" },
	{ 0x1F649, 14, 3, "hear_no_evil" },
	{ 0x1F64A, 15, 3, "speak_no_evil" },

	/* Nature */
	{ 0x1F315, 16, 3, "full_moon" },
	{ 0x1F31A, 17, 3, "new_moon_face" },
	{ 0x2600,  18, 3, "sunny" },
	{ 0x26C5,  19, 3, "partly_sunny" },
	{ 0x2601,  20, 3, "cloud" },
	{ 0x1F327, 21, 3, "rain" },
	{ 0x26A0,  22, 3, "warning" },
	{ 0x1F6AB, 23, 3, "no_entry_sign" },

	/* Flags & Misc */
	{ 0x1F1FA, 24, 3, "flag_u" },
	{ 0x1F1F8, 25, 3, "flag_s" },
	{ 0x1F3C1, 26, 3, "checkered_flag" },
	{ 0x1F6A9, 27, 3, "triangular_flag" },
	{ 0x1F4E2, 28, 3, "loudspeaker" },
	{ 0x1F514, 29, 3, "bell" },
	{ 0x1F3B5, 30, 3, "musical_note" },
	{ 0x1F3B6, 31, 3, "notes" },
};

static int emojiTableCount = 0;
static qboolean emojiInitialized = qfalse;

void Q_Emoji_Init( void ) {
	emojiTableCount = (int)( sizeof( emojiTable ) / sizeof( emojiTable[0] ) );
	emojiInitialized = qtrue;
	Com_Printf( "Emoji system: %d emoji registered (atlas %dx%d)\n",
		emojiTableCount, Q_EMOJI_ATLAS_SIZE, Q_EMOJI_ATLAS_SIZE );
}

int Q_Emoji_Count( void ) {
	return emojiTableCount;
}

const emojiEntry_t *Q_Emoji_GetByIndex( int index ) {
	if ( index < 0 || index >= emojiTableCount ) return NULL;
	return &emojiTable[index];
}

qboolean Q_UTF8_IsEmoji( uint32_t codepoint ) {
	return Q_Emoji_Lookup( codepoint ) != NULL;
}

const emojiEntry_t *Q_Emoji_Lookup( uint32_t codepoint ) {
	if ( !emojiInitialized ) return NULL;
	for ( int i = 0; i < emojiTableCount; i++ ) {
		if ( emojiTable[i].codepoint == codepoint ) {
			return &emojiTable[i];
		}
	}
	return NULL;
}

const emojiEntry_t *Q_Emoji_FindByShortcode( const char *shortcode ) {
	if ( !emojiInitialized || !shortcode ) return NULL;
	for ( int i = 0; i < emojiTableCount; i++ ) {
		if ( Q_stricmp( emojiTable[i].shortcode, shortcode ) == 0 ) {
			return &emojiTable[i];
		}
	}
	return NULL;
}
