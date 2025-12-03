/*
=============================================================================
Animation Event System Implementation
=============================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/q_common.h"
#include "g_local.h"
#include "animation_events.h"

#define MAX_ANIMATION_EVENT_CALLBACKS 256

typedef struct {
	int entityNum;
	anim_event_type_t eventType;
	anim_event_callback_t callback;
	qboolean active;
} anim_event_callback_entry_t;

static anim_event_callback_entry_t animEventCallbacks[MAX_ANIMATION_EVENT_CALLBACKS];
static uint32_t animEventCallbackCount = 0;

/*
=============================================================================
Register Animation Event Callback
=============================================================================
*/
void G_RegisterAnimationEvent( int entityNum, anim_event_type_t eventType, anim_event_callback_t callback )
{
	if ( animEventCallbackCount >= MAX_ANIMATION_EVENT_CALLBACKS ) {
		Com_Printf( "WARNING: Animation event callback limit reached\n" );
		return;
	}
	
	anim_event_callback_entry_t *entry = &animEventCallbacks[animEventCallbackCount];
	entry->entityNum = entityNum;
	entry->eventType = eventType;
	entry->callback = callback;
	entry->active = qtrue;
	
	animEventCallbackCount++;
}

/*
=============================================================================
Unregister Animation Event Callback
=============================================================================
*/
void G_UnregisterAnimationEvent( int entityNum, anim_event_type_t eventType )
{
	for ( uint32_t i = 0; i < animEventCallbackCount; i++ ) {
		anim_event_callback_entry_t *entry = &animEventCallbacks[i];
		if ( entry->active && entry->entityNum == entityNum && entry->eventType == eventType ) {
			entry->active = qfalse;
			// Compact array
			for ( uint32_t j = i; j < animEventCallbackCount - 1; j++ ) {
				animEventCallbacks[j] = animEventCallbacks[j + 1];
			}
			animEventCallbackCount--;
			break;
		}
	}
}

/*
=============================================================================
Trigger Animation Event
=============================================================================
*/
void G_TriggerAnimationEvent( int entityNum, anim_event_type_t eventType, const char *customData )
{
	for ( uint32_t i = 0; i < animEventCallbackCount; i++ ) {
		anim_event_callback_entry_t *entry = &animEventCallbacks[i];
		if ( entry->active && entry->entityNum == entityNum && entry->eventType == eventType ) {
			if ( entry->callback ) {
				entry->callback( entityNum, eventType, customData );
			}
		}
	}
	
	// Also trigger Lua callbacks if Lua is enabled
	// TODO: Implement Lua callback system
}

/*
=============================================================================
Register Lua Functions
=============================================================================
*/
void G_RegisterAnimationEventLua( void *luaState )
{
	// TODO: Register Lua functions for animation events
	// Example:
	// lua_register(luaState, "OnHitFrame", lua_on_hit_frame);
	// lua_register(luaState, "OnParryWindow", lua_on_parry_window);
}

