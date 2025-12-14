/*
===========================================================================
Event System Implementation
===========================================================================
*/

#include "event_system.h"
#include "qcommon.h"

// Forward declarations for event handlers
static void Event_OnFrameStart(const event_t *event, void *userData);
static void Event_OnFrameEnd(const event_t *event, void *userData);

// ============================================================================
// Internal Data Structures
// ============================================================================

#define MAX_EVENT_SUBSCRIPTIONS 1024
#define MAX_EVENT_QUEUE_SIZE 256
#define MAX_EVENT_TYPES 512

// Event subscription
struct eventSubscription_s {
	uint32_t eventType;
	eventHandler_t handler;
	void *userData;
	eventPriority_t priority;
	qboolean active;
	struct eventSubscription_s *next;  // Linked list for multiple handlers per type
};

// Event type registry
typedef struct {
	char name[MAX_STRING_CHARS];
	uint32_t hash;
} eventTypeInfo_t;

// Global event system state
static struct {
	// Subscription management
	struct eventSubscription_s subscriptions[MAX_EVENT_SUBSCRIPTIONS];
	struct eventSubscription_s *subscriptionHash[MAX_EVENT_TYPES];  // Hash table for fast lookup

	// Event processing queues
	eventQueueEntry_t immediateQueue[MAX_EVENT_QUEUE_SIZE];
	eventQueueEntry_t deferredQueue[MAX_EVENT_QUEUE_SIZE];
	eventQueueEntry_t scheduledQueue[MAX_EVENT_QUEUE_SIZE];
	uint32_t immediateQueueSize;
	uint32_t deferredQueueSize;
	uint32_t scheduledQueueSize;

	// Event type registry
	eventTypeInfo_t eventTypes[MAX_EVENT_TYPES];
	uint32_t nextEventTypeId;

	// Category filtering
	qboolean categoryEnabled[EVENT_CATEGORY_COUNT];

	// Thread safety
	int lockCount;  // Simple recursive lock count

	// Statistics
	uint32_t eventsPublished;
	uint32_t eventsProcessed;
	uint32_t subscriptionsCreated;
	uint32_t subscriptionsDestroyed;

	// Memory management
	qboolean initialized;
} eventSystem;

// ============================================================================
// Internal Helper Functions
// ============================================================================

/*
================
Event_HashEventType
================
*/
static uint32_t Event_HashEventType(uint32_t eventType) {
	return eventType % MAX_EVENT_TYPES;
}

/*
================
Event_FindSubscriptionSlot
================
*/
static struct eventSubscription_s *Event_FindSubscriptionSlot(void) {
	for (int i = 0; i < MAX_EVENT_SUBSCRIPTIONS; i++) {
		if (!eventSystem.subscriptions[i].active) {
			return &eventSystem.subscriptions[i];
		}
	}
	return NULL;
}

/*
================
Event_AddToQueue
================
*/
static qboolean Event_AddToQueue(eventQueueEntry_t *queue, uint32_t *queueSize,
                                const event_t *event, eventHandler_t handler, void *userData) {
	if (*queueSize >= MAX_EVENT_QUEUE_SIZE) {
		Com_Printf("Event_AddToQueue: Queue overflow\n");
		return qfalse;
	}

	eventQueueEntry_t *entry = &queue[*queueSize];
	entry->event = (event_t *)event;  // Note: We take ownership of the event
	entry->timestamp = Sys_Milliseconds();
	entry->scheduledTime = 0;  // Default: not scheduled
	entry->handler = handler;
	entry->userData = userData;

	(*queueSize)++;
	return qtrue;
}

/*
================
Event_AddToScheduledQueue
================
*/
static qboolean Event_AddToScheduledQueue(const event_t *event, uint32_t delayMs,
                                         eventHandler_t handler, void *userData) {
	if (eventSystem.scheduledQueueSize >= MAX_EVENT_QUEUE_SIZE) {
		Com_Printf("Event_AddToScheduledQueue: Queue overflow\n");
		return qfalse;
	}

	eventQueueEntry_t *entry = &eventSystem.scheduledQueue[eventSystem.scheduledQueueSize];
	entry->event = (event_t *)event;  // Note: We take ownership of the event
	entry->timestamp = Sys_Milliseconds();
	entry->scheduledTime = entry->timestamp + delayMs;
	entry->handler = handler;
	entry->userData = userData;

	eventSystem.scheduledQueueSize++;
	return qtrue;
}

/*
================
Event_ProcessQueue
================
*/
static void Event_ProcessQueue(eventQueueEntry_t *queue, uint32_t *queueSize) {
	// Sort by priority (simple bubble sort for now - optimize if needed)
	for (uint32_t i = 0; i < *queueSize; i++) {
		for (uint32_t j = i + 1; j < *queueSize; j++) {
			// Note: Priority comparison would need subscription lookup
			// For now, process in FIFO order
		}
	}

	// Process all events in queue
	for (uint32_t i = 0; i < *queueSize; i++) {
		eventQueueEntry_t *entry = &queue[i];

		if (entry->handler) {
			entry->handler(entry->event, entry->userData);
			eventSystem.eventsProcessed++;
		}

		// Clean up event data
		if (entry->event) {
			Event_Destroy(entry->event);
		}
	}

	*queueSize = 0;
}

/*
================
Event_ProcessScheduledQueue
================
Process scheduled events that have reached their scheduled time.
Returns number of events processed.
================
*/
static uint32_t Event_ProcessScheduledQueue(void) {
	if (!eventSystem.initialized || eventSystem.scheduledQueueSize == 0) {
		return 0;
	}

	uint32_t currentTime = Sys_Milliseconds();
	uint32_t processed = 0;
	uint32_t remaining = 0;

	// Process all scheduled events that are ready
	for (uint32_t i = 0; i < eventSystem.scheduledQueueSize; i++) {
		eventQueueEntry_t *entry = &eventSystem.scheduledQueue[i];

		if (entry->scheduledTime > 0 && currentTime >= entry->scheduledTime) {
			// Event is ready to process
			// Find subscribers for this event type
			uint32_t hash = Event_HashEventType(entry->event->type);
			struct eventSubscription_s *sub = eventSystem.subscriptionHash[hash];

			if (sub) {
				// Use provided handler or find subscribers
				if (entry->handler) {
					entry->handler(entry->event, entry->userData);
					eventSystem.eventsProcessed++;
				} else {
					// Find and call all subscribers
					while (sub) {
						if (sub->active && sub->eventType == entry->event->type) {
							sub->handler(entry->event, sub->userData);
							eventSystem.eventsProcessed++;
						}
						sub = sub->next;
					}
				}
			}

			// Clean up event
			if (entry->event) {
				Event_Destroy(entry->event);
			}
			processed++;
		} else {
			// Keep this event in the queue
			if (remaining != i) {
				eventSystem.scheduledQueue[remaining] = eventSystem.scheduledQueue[i];
			}
			remaining++;
		}
	}

	eventSystem.scheduledQueueSize = remaining;
	return processed;
}

// ============================================================================
// Public API Implementation
// ============================================================================

/*
================
Event_Init
================
*/
void Event_Init(void) {
	if (eventSystem.initialized) {
		return;
	}

	Com_Memset(&eventSystem, 0, sizeof(eventSystem));

	// Initialize category filtering (all enabled by default)
	for (int i = 0; i < EVENT_CATEGORY_COUNT; i++) {
		eventSystem.categoryEnabled[i] = qtrue;
	}

	// Reserve event type 0 as invalid
	eventSystem.nextEventTypeId = 1;

	// Register standard event types
	REGISTER_EVENT_TYPE(ENGINE_INIT);
	REGISTER_EVENT_TYPE(ENGINE_SHUTDOWN);
	REGISTER_EVENT_TYPE(ENGINE_FRAME_START);
	REGISTER_EVENT_TYPE(ENGINE_FRAME_END);
	REGISTER_EVENT_TYPE(ENGINE_MAP_LOAD);
	REGISTER_EVENT_TYPE(ENGINE_MAP_UNLOAD);

	REGISTER_EVENT_TYPE(GAME_START);
	REGISTER_EVENT_TYPE(GAME_END);
	REGISTER_EVENT_TYPE(GAME_PAUSE);
	REGISTER_EVENT_TYPE(GAME_RESUME);

	REGISTER_EVENT_TYPE(ENTITY_SPAWN);
	REGISTER_EVENT_TYPE(ENTITY_DESTROY);
	REGISTER_EVENT_TYPE(ENTITY_DAMAGE);
	REGISTER_EVENT_TYPE(ENTITY_DEATH);
	REGISTER_EVENT_TYPE(ENTITY_TOUCH);

	REGISTER_EVENT_TYPE(NET_CONNECT);
	REGISTER_EVENT_TYPE(NET_DISCONNECT);
	REGISTER_EVENT_TYPE(NET_MESSAGE);

	REGISTER_EVENT_TYPE(UI_OPEN_MENU);
	REGISTER_EVENT_TYPE(UI_CLOSE_MENU);
	REGISTER_EVENT_TYPE(UI_BUTTON_PRESS);

	REGISTER_EVENT_TYPE(DEBUG_LOG);
	REGISTER_EVENT_TYPE(DEBUG_WARNING);
	REGISTER_EVENT_TYPE(DEBUG_ERROR);

	eventSystem.initialized = qtrue;

	// Register some example event handlers for testing
	Event_Subscribe(EVENT_TYPE_ENGINE_FRAME_START, Event_OnFrameStart, NULL, EVENT_PRIORITY_NORMAL);
	Event_Subscribe(EVENT_TYPE_ENGINE_FRAME_END, Event_OnFrameEnd, NULL, EVENT_PRIORITY_NORMAL);

	if (com_developer && com_developer->integer) {
		Com_Printf("Event system initialized\n");
	}
}

/*
================
Event_Shutdown
================
*/
void Event_Shutdown(void) {
	if (!eventSystem.initialized) {
		return;
	}

	// Process any remaining events
	Event_ProcessAll();

	// Clean up subscriptions
	for (int i = 0; i < MAX_EVENT_SUBSCRIPTIONS; i++) {
		eventSystem.subscriptions[i].active = qfalse;
	}

	// Clear queues and free events
	for (uint32_t i = 0; i < eventSystem.immediateQueueSize; i++) {
		if (eventSystem.immediateQueue[i].event) {
			Event_Destroy(eventSystem.immediateQueue[i].event);
		}
	}
	eventSystem.immediateQueueSize = 0;

		for (uint32_t i = 0; i < eventSystem.deferredQueueSize; i++) {
			if (eventSystem.deferredQueue[i].event) {
				Event_Destroy(eventSystem.deferredQueue[i].event);
			}
		}
		eventSystem.deferredQueueSize = 0;

		// Clear scheduled queue
		for (uint32_t i = 0; i < eventSystem.scheduledQueueSize; i++) {
			if (eventSystem.scheduledQueue[i].event) {
				Event_Destroy(eventSystem.scheduledQueue[i].event);
			}
		}
		eventSystem.scheduledQueueSize = 0;

	eventSystem.initialized = qfalse;

	if (com_developer && com_developer->integer) {
		Com_Printf("Event system shutdown\n");
	}
}

/*
================
Event_Publish
================
*/
qboolean Event_Publish(event_t *event) {
	if (!eventSystem.initialized || !event) {
		return qfalse;
	}

	// Check if category is enabled
	if (!eventSystem.categoryEnabled[event->category]) {
		Event_Destroy(event);
		return qtrue;  // Not an error, just filtered out
	}

	eventSystem.eventsPublished++;

	// Find subscribers for this event type
	uint32_t hash = Event_HashEventType(event->type);
	struct eventSubscription_s *sub = eventSystem.subscriptionHash[hash];

	while (sub) {
		if (sub->active && sub->eventType == event->type) {
			// Add to immediate queue for processing
			if (!Event_AddToQueue(eventSystem.immediateQueue, &eventSystem.immediateQueueSize,
			                     event, sub->handler, sub->userData)) {
				Event_Destroy(event);
				return qfalse;
			}
		}
		sub = sub->next;
	}

	// If no subscribers, clean up the event
	if (eventSystem.immediateQueueSize == 0 ||
	    eventSystem.immediateQueue[eventSystem.immediateQueueSize - 1].event != event) {
		Event_Destroy(event);
	}

	return qtrue;
}

/*
================
Event_PublishImmediate
================
*/
qboolean Event_PublishImmediate(event_t *event) {
	if (!Event_Publish(event)) {
		return qfalse;
	}

	Event_ProcessImmediate();
	return qtrue;
}

/*
================
Event_PublishDeferred
================
*/
qboolean Event_PublishDeferred(event_t *event) {
	if (!eventSystem.initialized || !event) {
		return qfalse;
	}

	// Check if category is enabled
	if (!eventSystem.categoryEnabled[event->category]) {
		Event_Destroy(event);
		return qtrue;
	}

	eventSystem.eventsPublished++;

	return Event_AddToQueue(eventSystem.deferredQueue, &eventSystem.deferredQueueSize,
	                        event, NULL, NULL);
}

/*
================
Event_PublishScheduled
================
Publish an event to be processed after a delay (in milliseconds).
================
*/
qboolean Event_PublishScheduled(event_t *event, uint32_t delayMs) {
	if (!eventSystem.initialized || !event) {
		return qfalse;
	}

	// Check if category is enabled
	if (!eventSystem.categoryEnabled[event->category]) {
		Event_Destroy(event);
		return qtrue;
	}

	eventSystem.eventsPublished++;

	// Find subscribers for this event type to determine handlers
	uint32_t hash = Event_HashEventType(event->type);
	struct eventSubscription_s *sub = eventSystem.subscriptionHash[hash];

	if (sub && sub->active && sub->eventType == event->type) {
		// Use first subscriber's handler
		return Event_AddToScheduledQueue(event, delayMs, sub->handler, sub->userData);
	} else {
		// No handler found, use NULL handler (will find subscribers when processing)
		return Event_AddToScheduledQueue(event, delayMs, NULL, NULL);
	}
}

/*
================
Event_Subscribe
================
*/
eventSubscription_t Event_Subscribe(uint32_t eventType, eventHandler_t handler,
                                   void *userData, eventPriority_t priority) {
	if (!eventSystem.initialized || !handler) {
		return NULL;
	}

	struct eventSubscription_s *slot = Event_FindSubscriptionSlot();
	if (!slot) {
		Com_Printf("Event_Subscribe: No free subscription slots\n");
		return NULL;
	}

	slot->eventType = eventType;
	slot->handler = handler;
	slot->userData = userData;
	slot->priority = priority;
	slot->active = qtrue;

	// Add to hash table
	uint32_t hash = Event_HashEventType(eventType);
	slot->next = eventSystem.subscriptionHash[hash];
	eventSystem.subscriptionHash[hash] = slot;

	eventSystem.subscriptionsCreated++;

	return slot;
}

/*
================
Event_Unsubscribe
================
*/
void Event_Unsubscribe(eventSubscription_t subscription) {
	if (!subscription || !eventSystem.initialized) {
		return;
	}

	if (subscription->active) {
		subscription->active = qfalse;
		eventSystem.subscriptionsDestroyed++;
	}
}

/*
================
Event_IsCategoryEnabled
================
*/
qboolean Event_IsCategoryEnabled(eventCategory_t category) {
	if (category >= EVENT_CATEGORY_COUNT) {
		return qfalse;
	}
	return eventSystem.categoryEnabled[category];
}

/*
================
Event_SetCategoryEnabled
================
*/
void Event_SetCategoryEnabled(eventCategory_t category, qboolean enabled) {
	if (category >= EVENT_CATEGORY_COUNT) {
		return;
	}
	eventSystem.categoryEnabled[category] = enabled;
}

/*
================
Event_ProcessImmediate
================
*/
void Event_ProcessImmediate(void) {
	if (!eventSystem.initialized) {
		return;
	}

	Event_ProcessQueue(eventSystem.immediateQueue, &eventSystem.immediateQueueSize);
}

/*
================
Event_ProcessDeferred
================
*/
void Event_ProcessDeferred(void) {
	if (!eventSystem.initialized) {
		return;
	}

	Event_ProcessQueue(eventSystem.deferredQueue, &eventSystem.deferredQueueSize);
}

/*
================
Event_ProcessScheduled
================
*/
void Event_ProcessScheduled(void) {
	if (!eventSystem.initialized) {
		return;
	}

	Event_ProcessScheduledQueue();
}

/*
================
Event_ProcessAll
================
*/
void Event_ProcessAll(void) {
	Event_ProcessImmediate();
	Event_ProcessDeferred();
	Event_ProcessScheduled();
}

/*
================
Event_Create
================
*/
event_t *Event_Create(uint32_t type, eventCategory_t category, uint32_t size) {
	if (!eventSystem.initialized) {
		return NULL;
	}

	uint32_t totalSize = sizeof(event_t) + size;
	event_t *event = Z_TagMalloc(totalSize, TAG_GENERAL);

	if (!event) {
		return NULL;
	}

	Com_Memset(event, 0, totalSize);
	event->type = type;
	event->category = category;
	event->size = size;
	event->cancellable = qfalse;
	event->cancelled = qfalse;

	if (size > 0) {
		event->data = (char *)event + sizeof(event_t);
	}

	return event;
}

/*
================
Event_Destroy
================
*/
void Event_Destroy(event_t *event) {
	if (event) {
		Z_Free(event);
	}
}

/*
================
Event_RegisterType
================
*/
uint32_t Event_RegisterType(const char *typeName) {
	if (!eventSystem.initialized || !typeName || eventSystem.nextEventTypeId >= MAX_EVENT_TYPES) {
		return 0;
	}

	// Check if already registered
	for (uint32_t i = 1; i < eventSystem.nextEventTypeId; i++) {
		if (Q_stricmp(eventSystem.eventTypes[i].name, typeName) == 0) {
			return i;
		}
	}

	uint32_t typeId = eventSystem.nextEventTypeId++;
	Q_strncpyz(eventSystem.eventTypes[typeId].name, typeName, sizeof(eventSystem.eventTypes[typeId].name));
	eventSystem.eventTypes[typeId].hash = MSG_HashKey(typeName, 0);

	return typeId;
}

/*
================
Event_GetTypeName
================
*/
const char *Event_GetTypeName(uint32_t type) {
	if (type >= MAX_EVENT_TYPES) {
		return "INVALID";
	}
	return eventSystem.eventTypes[type].name;
}

/*
================
Event_GetStats
================
*/
void Event_GetStats(eventSystemStats_t *stats) {
	if (!stats) {
		return;
	}

	// Count active subscriptions
	uint32_t activeSubs = 0;
	for (int i = 0; i < MAX_EVENT_SUBSCRIPTIONS; i++) {
		if (eventSystem.subscriptions[i].active) {
			activeSubs++;
		}
	}

	stats->eventsPublished = eventSystem.eventsPublished;
	stats->eventsProcessed = eventSystem.eventsProcessed;
	stats->subscriptionsCreated = eventSystem.subscriptionsCreated;
	stats->subscriptionsDestroyed = eventSystem.subscriptionsDestroyed;
	stats->activeSubscriptions = activeSubs;
	stats->immediateQueueSize = eventSystem.immediateQueueSize;
	stats->deferredQueueSize = eventSystem.deferredQueueSize;
	stats->scheduledQueueSize = eventSystem.scheduledQueueSize;
	stats->registeredEventTypes = eventSystem.nextEventTypeId;
}

/*
================
Event_PrintStats
================
*/
void Event_PrintStats(void) {
	eventSystemStats_t stats;
	Event_GetStats(&stats);
	
	Com_Printf("=== Event System Statistics ===\n");
	Com_Printf("Events Published: %u\n", stats.eventsPublished);
	Com_Printf("Events Processed: %u\n", stats.eventsProcessed);
	Com_Printf("Subscriptions Created: %u\n", stats.subscriptionsCreated);
	Com_Printf("Subscriptions Destroyed: %u\n", stats.subscriptionsDestroyed);
	Com_Printf("Immediate Queue Size: %u\n", stats.immediateQueueSize);
	Com_Printf("Deferred Queue Size: %u\n", stats.deferredQueueSize);
	Com_Printf("Scheduled Queue Size: %u\n", stats.scheduledQueueSize);
	Com_Printf("Active Subscriptions: %u\n", stats.activeSubscriptions);
	Com_Printf("Registered Event Types: %u\n", stats.registeredEventTypes);
}

/*
================
Event_GetQueueSize
================
*/
uint32_t Event_GetQueueSize(void) {
	return eventSystem.immediateQueueSize + eventSystem.deferredQueueSize;
}

/*
================
Event_GetSubscriptionCount
================
*/
uint32_t Event_GetSubscriptionCount(uint32_t eventType) {
	uint32_t count = 0;
	uint32_t hash = Event_HashEventType(eventType);
	struct eventSubscription_s *sub = eventSystem.subscriptionHash[hash];

	while (sub) {
		if (sub->active && sub->eventType == eventType) {
			count++;
		}
		sub = sub->next;
	}

	return count;
}

/*
================
Event_Lock/Event_Unlock
================
*/
void Event_Lock(void) {
	eventSystem.lockCount++;
}

void Event_Unlock(void) {
	if (eventSystem.lockCount > 0) {
		eventSystem.lockCount--;
	}
}

// ============================================================================
// Example Event Handlers (for testing and demonstration)
// ============================================================================

/*
================
Event_OnFrameStart
================
*/
static void Event_OnFrameStart(const event_t *event, void *userData) {
	(void)event;
	(void)userData;

	// Example: Could update performance counters, start frame profiling, etc.
	if (com_developer && com_developer->integer >= 2) {
		Com_Printf("Frame start event received\n");
	}
}

/*
================
Event_OnFrameEnd
================
*/
static void Event_OnFrameEnd(const event_t *event, void *userData) {
	(void)event;
	(void)userData;

	// Example: Could finalize frame statistics, process deferred events, etc.
	if (com_developer && com_developer->integer >= 2) {
		Com_Printf("Frame end event received\n");
	}
}

// ============================================================================
// Event Type Definitions (auto-generated)
// ============================================================================

DEFINE_EVENT_TYPE(ENGINE_INIT);
DEFINE_EVENT_TYPE(ENGINE_SHUTDOWN);
DEFINE_EVENT_TYPE(ENGINE_FRAME_START);
DEFINE_EVENT_TYPE(ENGINE_FRAME_END);
DEFINE_EVENT_TYPE(ENGINE_MAP_LOAD);
DEFINE_EVENT_TYPE(ENGINE_MAP_UNLOAD);

DEFINE_EVENT_TYPE(GAME_START);
DEFINE_EVENT_TYPE(GAME_END);
DEFINE_EVENT_TYPE(GAME_PAUSE);
DEFINE_EVENT_TYPE(GAME_RESUME);

DEFINE_EVENT_TYPE(ENTITY_SPAWN);
DEFINE_EVENT_TYPE(ENTITY_DESTROY);
DEFINE_EVENT_TYPE(ENTITY_DAMAGE);
DEFINE_EVENT_TYPE(ENTITY_DEATH);
DEFINE_EVENT_TYPE(ENTITY_TOUCH);

DEFINE_EVENT_TYPE(NET_CONNECT);
DEFINE_EVENT_TYPE(NET_DISCONNECT);
DEFINE_EVENT_TYPE(NET_MESSAGE);

DEFINE_EVENT_TYPE(UI_OPEN_MENU);
DEFINE_EVENT_TYPE(UI_CLOSE_MENU);
DEFINE_EVENT_TYPE(UI_BUTTON_PRESS);

DEFINE_EVENT_TYPE(DEBUG_LOG);
DEFINE_EVENT_TYPE(DEBUG_WARNING);
DEFINE_EVENT_TYPE(DEBUG_ERROR);