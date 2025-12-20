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
 * name:		ai_team.h
 *
 * desc:		Quake3 bot AI
 *
 * $Archive: /source/code/botai/ai_chat.c $
 *
 *****************************************************************************/

void BotTeamAI(bot_state_t *bs);
int BotGetTeamMateTaskPreference(bot_state_t *bs, int teammate);
void BotSetTeamMateTaskPreference(bot_state_t *bs, int teammate, int preference);
void BotVoiceChat(bot_state_t *bs, int toclient, char *voicechat);
void BotVoiceChatOnly(bot_state_t *bs, int toclient, char *voicechat);
int BotValidTeamLeader( bot_state_t *bs );
int BotNumTeamMates( bot_state_t *bs );
int BotClientTravelTimeToGoal( int client, bot_goal_t *goal );
int BotSortTeamMatesByBaseTravelTime( bot_state_t *bs, int *teammates, int maxteammates );
int BotSortTeamMatesByRelativeTravelTime2ddA( bot_state_t *bs, int *teammates, int maxteammates );
int BotSortTeamMatesByTaskPreference( bot_state_t *bs, int *teammates, int numteammates );
void BotSayTeamOrderAlways( bot_state_t *bs, int toclient );
void BotSayTeamOrder( bot_state_t *bs, int toclient );
void BotSayVoiceTeamOrder( bot_state_t *bs, int toclient, char *voicechat );
void BotCTFOrders_BothFlagsNotAtBase( bot_state_t *bs );
void BotCTFOrders_FlagNotAtBase( bot_state_t *bs );
void BotCTFOrders_EnemyFlagNotAtBase( bot_state_t *bs );
void BotDDorders_Standard( bot_state_t *bs );
void BotCTFOrders_BothFlagsAtBase( bot_state_t *bs );
void BotCTFOrders( bot_state_t *bs );
void BotDDorders( bot_state_t *bs );
void BotCreateGroup( bot_state_t *bs, int *teammates, int groupsize );
void BotTeamOrders( bot_state_t *bs );
void Bot1FCTFOrders_FlagAtCenter( bot_state_t *bs );
void Bot1FCTFOrders_TeamHasFlag( bot_state_t *bs );
void Bot1FCTFOrders_EnemyHasFlag( bot_state_t *bs );
void Bot1FCTFOrders_EnemyDroppedFlag( bot_state_t *bs );
void Bot1FCTFOrders( bot_state_t *bs );
void BotObeliskOrders( bot_state_t *bs );
void BotHarvesterOrders( bot_state_t *bs );
int FindHumanTeamLeader( bot_state_t *bs );


