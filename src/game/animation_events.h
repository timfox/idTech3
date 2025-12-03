/*
=============================================================================
Animation Event System
Provides gameplay hooks for animation-driven events (hit frames, parry windows)
=============================================================================
*/

#pragma once

#include "../qcommon/q_shared.h"

// Animation event types
typedef enum {
	ANIM_EVENT_HIT_FRAME,        // Hit frame - damage dealt
	ANIM_EVENT_PARRY_WINDOW_OPEN, // Parry window opens
	ANIM_EVENT_PARRY_WINDOW_CLOSE, // Parry window closes
	ANIM_EVENT_RECOVER_START,     // Recovery starts
	ANIM_EVENT_RECOVER_END,       // Recovery ends
	ANIM_EVENT_FOOTSTEP,          // Footstep sound/effect
	ANIM_EVENT_WEAPON_FIRE,       // Weapon fire
	ANIM_EVENT_WEAPON_RELOAD,     // Weapon reload
	ANIM_EVENT_CUSTOM,            // Custom event (with string parameter)
	
	ANIM_EVENT_MAX
} anim_event_type_t;

// Animation event callback function type
typedef void (*anim_event_callback_t)( int entityNum, anim_event_type_t eventType, const char *customData );

// Animation event registration
void G_RegisterAnimationEvent( int entityNum, anim_event_type_t eventType, anim_event_callback_t callback );
void G_UnregisterAnimationEvent( int entityNum, anim_event_type_t eventType );

// Trigger animation event (called from animation system)
void G_TriggerAnimationEvent( int entityNum, anim_event_type_t eventType, const char *customData );

// Lua integration
void G_RegisterAnimationEventLua( void *luaState );

