/*
===========================================================================
Event System - Central Event Bus API
===========================================================================
*/

#ifndef __EVENT_SYSTEM_H__
#define __EVENT_SYSTEM_H__

#include "q_shared.h"

// Event priority levels (higher numbers = higher priority)
typedef enum {
	EVENT_PRIORITY_LOWEST = 0,
	EVENT_PRIORITY_LOW = 25,
	EVENT_PRIORITY_NORMAL = 50,
	EVENT_PRIORITY_HIGH = 75,
	EVENT_PRIORITY_HIGHEST = 100,
	EVENT_PRIORITY_CRITICAL = 127
} eventPriority_t;

// Event categories for organization and filtering
typedef enum {
	EVENT_CATEGORY_ENGINE = 0,    // Engine-level events
	EVENT_CATEGORY_GAME,          // Game logic events
	EVENT_CATEGORY_ENTITY,        // Entity-related events
	EVENT_CATEGORY_NETWORK,       // Networking events
	EVENT_CATEGORY_UI,            // User interface events
	EVENT_CATEGORY_AUDIO,         // Audio system events
	EVENT_CATEGORY_RENDER,        // Rendering events
	EVENT_CATEGORY_SCRIPT,        // Scripting events
	EVENT_CATEGORY_DEBUG,         // Debug/development events

	EVENT_CATEGORY_COUNT
} eventCategory_t;

// Event phases for lifecycle management
typedef enum {
	EVENT_PHASE_IMMEDIATE = 0,   // Process immediately (same frame)
	EVENT_PHASE_DEFERRED,        // Process at end of frame
	EVENT_PHASE_SCHEDULED        // Process at specific time
} eventPhase_t;

// Base event structure - all events must inherit from this
typedef struct event_s {
	uint32_t type;              // Unique event type identifier
	eventCategory_t category;   // Event category for filtering
	uint32_t size;              // Size of event data (for validation)
	qboolean cancellable;       // Whether event can be cancelled
	qboolean cancelled;         // Whether event was cancelled
	void *data;                 // Event-specific data
} event_t;

// Event handler function signature
typedef void (*eventHandler_t)(const event_t *event, void *userData);

// Event subscription handle (for unsubscribing)
typedef struct eventSubscription_s *eventSubscription_t;

// Event queue entry for deferred/scheduled processing
typedef struct {
	event_t *event;
	uint32_t timestamp;        // When event was queued
	uint32_t scheduledTime;    // When event should be processed (for scheduled events)
	eventHandler_t handler;
	void *userData;
} eventQueueEntry_t;

// ============================================================================
// Core Event Bus API
// ============================================================================

/*
================
Event System Initialization
================
*/
void Event_Init(void);
void Event_Shutdown(void);

/*
================
Event Publishing
================
*/
qboolean Event_Publish(event_t *event);
qboolean Event_PublishImmediate(event_t *event);
qboolean Event_PublishDeferred(event_t *event);
qboolean Event_PublishScheduled(event_t *event, uint32_t delayMs);

/*
================
Event Subscription Management
================
*/
eventSubscription_t Event_Subscribe(uint32_t eventType, eventHandler_t handler,
                                   void *userData, eventPriority_t priority);
void Event_Unsubscribe(eventSubscription_t subscription);

/*
================
Event Categories and Filtering
================
*/
qboolean Event_IsCategoryEnabled(eventCategory_t category);
void Event_SetCategoryEnabled(eventCategory_t category, qboolean enabled);

/*
================
Event Processing
================
*/
void Event_ProcessImmediate(void);
void Event_ProcessDeferred(void);
void Event_ProcessScheduled(void);
void Event_ProcessAll(void);

/*
================
Event Factory Functions
================
*/
event_t *Event_Create(uint32_t type, eventCategory_t category, uint32_t size);
void Event_Destroy(event_t *event);

/*
================
Event Type Registration
================
*/
uint32_t Event_RegisterType(const char *typeName);
const char *Event_GetTypeName(uint32_t type);

/*
================
Event Debugging and Statistics
================
*/
void Event_PrintStats(void);
uint32_t Event_GetQueueSize(void);
uint32_t Event_GetSubscriptionCount(uint32_t eventType);

// Get event system statistics for debug overlays
typedef struct {
	uint32_t eventsPublished;
	uint32_t eventsProcessed;
	uint32_t subscriptionsCreated;
	uint32_t subscriptionsDestroyed;
	uint32_t activeSubscriptions;
	uint32_t immediateQueueSize;
	uint32_t deferredQueueSize;
	uint32_t scheduledQueueSize;
	uint32_t registeredEventTypes;
} eventSystemStats_t;

void Event_GetStats(eventSystemStats_t *stats);

/*
================
Thread Safety
================
*/
void Event_Lock(void);
void Event_Unlock(void);

// ============================================================================
// Convenience Macros
// ============================================================================

// Declare an event type (use in header files)
#define DECLARE_EVENT_TYPE(name) extern uint32_t EVENT_TYPE_##name

// Define an event type (use in source files)
#define DEFINE_EVENT_TYPE(name) uint32_t EVENT_TYPE_##name = 0

// Register an event type (call during initialization)
#define REGISTER_EVENT_TYPE(name) EVENT_TYPE_##name = Event_RegisterType(#name)

// Create and publish an event (immediate)
#define PUBLISH_EVENT(type, category, data, size) \
	do { \
		event_t *evt = Event_Create(type, category, size); \
		if (evt) { \
			const void *_evt_data = (data); \
			uint32_t _evt_size = (size); \
			if (_evt_size > 0 && _evt_data != NULL) { \
				Com_Memcpy(evt->data, _evt_data, _evt_size); \
			} \
			Event_Publish(evt); \
		} \
	} while(0)

// Create and publish an event with explicit phase
#define PUBLISH_EVENT_PHASE(type, category, phase, data, size) \
	do { \
		event_t *evt = Event_Create(type, category, size); \
		if (evt) { \
			if ((size) > 0 && (data) != NULL) { \
				Com_Memcpy(evt->data, (data), (size)); \
			} \
			if ((phase) == EVENT_PHASE_IMMEDIATE) { \
				Event_PublishImmediate(evt); \
			} else if ((phase) == EVENT_PHASE_DEFERRED) { \
				Event_PublishDeferred(evt); \
			} else { \
				Event_Publish(evt); \
			} \
		} \
	} while(0)

// Create and publish a scheduled event (delay in milliseconds)
#define PUBLISH_EVENT_SCHEDULED(type, category, delayMs, data, size) \
	do { \
		event_t *evt = Event_Create(type, category, size); \
		if (evt) { \
			if ((size) > 0 && (data) != NULL) { \
				Com_Memcpy(evt->data, (data), (size)); \
			} \
			Event_PublishScheduled(evt, delayMs); \
		} \
	} while(0)

// Subscribe to an event type
#define SUBSCRIBE_EVENT(type, handler, userData, priority) \
	Event_Subscribe(type, handler, userData, priority)

// ============================================================================
// Standard Event Types
// ============================================================================

// Engine events
DECLARE_EVENT_TYPE(ENGINE_INIT);
DECLARE_EVENT_TYPE(ENGINE_SHUTDOWN);
DECLARE_EVENT_TYPE(ENGINE_FRAME_START);
DECLARE_EVENT_TYPE(ENGINE_FRAME_END);
DECLARE_EVENT_TYPE(ENGINE_MAP_LOAD);
DECLARE_EVENT_TYPE(ENGINE_MAP_UNLOAD);

// Game events
DECLARE_EVENT_TYPE(GAME_START);
DECLARE_EVENT_TYPE(GAME_END);
DECLARE_EVENT_TYPE(GAME_PAUSE);
DECLARE_EVENT_TYPE(GAME_RESUME);

// Entity events
DECLARE_EVENT_TYPE(ENTITY_SPAWN);
DECLARE_EVENT_TYPE(ENTITY_DESTROY);
DECLARE_EVENT_TYPE(ENTITY_DAMAGE);
DECLARE_EVENT_TYPE(ENTITY_DEATH);
DECLARE_EVENT_TYPE(ENTITY_TOUCH);

// Network events
DECLARE_EVENT_TYPE(NET_CONNECT);
DECLARE_EVENT_TYPE(NET_DISCONNECT);
DECLARE_EVENT_TYPE(NET_MESSAGE);

// UI events
DECLARE_EVENT_TYPE(UI_OPEN_MENU);
DECLARE_EVENT_TYPE(UI_CLOSE_MENU);
DECLARE_EVENT_TYPE(UI_BUTTON_PRESS);

// Debug events
DECLARE_EVENT_TYPE(DEBUG_LOG);
DECLARE_EVENT_TYPE(DEBUG_WARNING);
DECLARE_EVENT_TYPE(DEBUG_ERROR);

#endif // __EVENT_SYSTEM_H__