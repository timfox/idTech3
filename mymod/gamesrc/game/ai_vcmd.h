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

/*****************************************************************************
 * name:		ai_vcmd.h
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /source/code/botai/ai_vcmd.c $
 *
 *****************************************************************************/

int BotVoiceChatCommand(bot_state_t *bs, int mode, char *voicechat);
void BotVoiceChat_Defend(bot_state_t *bs, int client, int mode);
void BotVoiceChat_GetFlag( bot_state_t *bs, int client, int mode );
void BotVoiceChat_Offense( bot_state_t *bs, int client, int mode );
void BotVoiceChat_DefendFlag( bot_state_t *bs, int client, int mode );
void BotVoiceChat_Patrol( bot_state_t *bs, int client, int mode );
void BotVoiceChat_Camp( bot_state_t *bs, int client, int mode );
void BotVoiceChat_FollowMe( bot_state_t *bs, int client, int mode );
void BotVoiceChat_FollowFlagCarrier( bot_state_t *bs, int client, int mode );
void BotVoiceChat_ReturnFlag( bot_state_t *bs, int client, int mode );
void BotVoiceChat_StartLeader( bot_state_t *bs, int client, int mode );
void BotVoiceChat_StopLeader( bot_state_t *bs, int client, int mode );
void BotVoiceChat_WhoIsLeader( bot_state_t *bs, int client, int mode );
void BotVoiceChat_WantOnDefense( bot_state_t *bs, int client, int mode );
void BotVoiceChat_WantOnOffense( bot_state_t *bs, int client, int mode );
void BotVoiceChat_Dummy( bot_state_t *bs, int client, int mode );


