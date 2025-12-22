/*
=============================================================================
Animation Event System Implementation
=============================================================================
*/

#include "../common/q_shared.h"
#include "../common/qcommon.h"
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
#ifdef USE_LUA
	extern void Lua_Events_Emit( const char *event_name, int num_args, ... );
	
	// Emit Lua event for animation events
	const char *eventName = NULL;
	switch ( eventType ) {
		case ANIM_EVENT_HIT_FRAME:
			eventName = "anim_hit_frame";
			break;
		case ANIM_EVENT_PARRY_WINDOW_OPEN:
			eventName = "anim_parry_window_open";
			break;
		case ANIM_EVENT_PARRY_WINDOW_CLOSE:
			eventName = "anim_parry_window_close";
			break;
		case ANIM_EVENT_RECOVER_START:
			eventName = "anim_recover_start";
			break;
		case ANIM_EVENT_RECOVER_END:
			eventName = "anim_recover_end";
			break;
		case ANIM_EVENT_FOOTSTEP:
			eventName = "anim_footstep";
			break;
		case ANIM_EVENT_WEAPON_FIRE:
			eventName = "anim_weapon_fire";
			break;
		case ANIM_EVENT_WEAPON_RELOAD:
			eventName = "anim_weapon_reload";
			break;
		case ANIM_EVENT_CUSTOM:
			eventName = "anim_custom";
			break;
		default:
			return;
	}
	
	if ( eventName ) {
		if ( customData && *customData ) {
			Lua_Events_Emit( eventName, 2, entityNum, customData );
		} else {
			Lua_Events_Emit( eventName, 1, entityNum );
		}
	}
#endif // USE_LUA
}

/*
=============================================================================
Register Lua Functions
=============================================================================
*/
void G_RegisterAnimationEventLua( void *luaState )
{
#ifdef USE_LUA
	if ( !luaState ) {
		return;
	}
	
	// Animation events are handled via the event system
	// Lua scripts can subscribe using Events.on("anim_hit_frame", function(entityNum) ... end)
	// No direct function registration needed - events are emitted automatically
	
	Com_Printf( "Animation events: Registered Lua integration via event system\n" );
#endif // USE_LUA
}

