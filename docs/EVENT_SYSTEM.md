# Event System Documentation

## Overview

The id Tech 3 Event System provides a modern, type-safe event-driven architecture for decoupling game systems and enabling modular, extensible code. It replaces traditional callback-based systems with a centralized event bus that supports priority-based event handling, category filtering, and thread-safe operations.

## Core Concepts

### Events
Events are structured data objects that represent occurrences in the game engine. Each event has:
- **Type**: Unique identifier for the event kind
- **Category**: Grouping for filtering and organization
- **Data**: Type-safe payload specific to the event
- **Metadata**: Cancellable flag, size validation, etc.

### Event Bus
The central event bus manages:
- **Publishing**: Broadcasting events to interested subscribers
- **Subscribing**: Registering handlers for specific event types
- **Processing**: Delivering events with priority ordering
- **Filtering**: Category-based event filtering

### Event Categories
Events are organized into categories for logical grouping:
- `ENGINE`: Core engine events (init, shutdown, frames)
- `GAME`: Game logic events (start, pause, resume)
- `ENTITY`: Entity lifecycle and interactions
- `NETWORK`: Network connection and messaging
- `UI`: User interface interactions
- `AUDIO`: Audio system events
- `RENDER`: Rendering pipeline events
- `SCRIPT`: Scripting system events
- `DEBUG`: Development and debugging events

## API Reference

### Initialization & Shutdown

```c
// Initialize the event system
void Event_Init(void);

// Shutdown and cleanup
void Event_Shutdown(void);
```

### Publishing Events

```c
// Publishing functions
qboolean Event_Publish(event_t *event);                    // Immediate (default)
qboolean Event_PublishImmediate(event_t *event);            // Immediate
qboolean Event_PublishDeferred(event_t *event);             // Deferred
qboolean Event_PublishScheduled(event_t *event, uint32_t delayMs);  // Scheduled

// Create and publish an event immediately
PUBLISH_EVENT(EVENT_TYPE_ENTITY_SPAWN, EVENT_CATEGORY_ENTITY, &entityData, sizeof(entityData));

// Publish an event for deferred processing
event_t *event = Event_Create(EVENT_TYPE_DEBUG_LOG, EVENT_CATEGORY_DEBUG, sizeof(logData));
Event_PublishDeferred(event);

// Force immediate processing
Event_ProcessImmediate();
Event_ProcessDeferred();
Event_ProcessAll();
```

### Subscribing to Events

```c
// Subscribe with default priority
eventSubscription_t sub = Event_Subscribe(EVENT_TYPE_ENTITY_DAMAGE,
                                         OnEntityDamaged,
                                         myUserData,
                                         EVENT_PRIORITY_NORMAL);

// Unsubscribe when done
Event_Unsubscribe(sub);
```

### Event Handler Signature

```c
void MyEventHandler(const event_t *event, void *userData) {
    // Cast event data to expected type
    const MyEventData *data = (const MyEventData *)event->data;

    // Handle the event
    ProcessEventData(data, userData);
}
```

### Category Filtering

```c
// Disable debug events in release builds
Event_SetCategoryEnabled(EVENT_CATEGORY_DEBUG, qfalse);

// Check if category is enabled
if (Event_IsCategoryEnabled(EVENT_CATEGORY_UI)) {
    // Publish UI event
}
```

## Standard Event Types

### Engine Events
- `ENGINE_INIT`: Engine initialization complete
- `ENGINE_SHUTDOWN`: Engine shutting down
- `ENGINE_FRAME_START`: Beginning of frame processing
- `ENGINE_FRAME_END`: End of frame processing
- `ENGINE_MAP_LOAD`: Map loading started
- `ENGINE_MAP_UNLOAD`: Map unloading complete

### Game Events
- `GAME_START`: Game session started
- `GAME_END`: Game session ended
- `GAME_PAUSE`: Game paused
- `GAME_RESUME`: Game resumed

### Entity Events
- `ENTITY_SPAWN`: Entity created
- `ENTITY_DESTROY`: Entity destroyed
- `ENTITY_DAMAGE`: Entity took damage
- `ENTITY_DEATH`: Entity died
- `ENTITY_TOUCH`: Entity touched another

### Network Events
- `NET_CONNECT`: Network connection established
- `NET_DISCONNECT`: Network connection lost
- `NET_MESSAGE`: Network message received

### UI Events
- `UI_OPEN_MENU`: Menu opened
- `UI_CLOSE_MENU`: Menu closed
- `UI_BUTTON_PRESS`: Button pressed

### Debug Events
- `DEBUG_LOG`: Debug log message
- `DEBUG_WARNING`: Debug warning
- `DEBUG_ERROR`: Debug error

## Advanced Features

### Event Priorities

Events are processed in priority order (higher numbers first):
- `EVENT_PRIORITY_LOWEST = 0`
- `EVENT_PRIORITY_LOW = 25`
- `EVENT_PRIORITY_NORMAL = 50` (default)
- `EVENT_PRIORITY_HIGH = 75`
- `EVENT_PRIORITY_HIGHEST = 100`
- `EVENT_PRIORITY_CRITICAL = 127`

### Event Cancellation

Cancellable events can be stopped by handlers:

```c
void OnCancellableEvent(const event_t *event, void *userData) {
    if (shouldCancel(event)) {
        *(qboolean *)&event->cancelled = qtrue;  // Mark as cancelled
    }
}
```

### Thread Safety

The event system supports thread-safe operations:

```c
// Lock for multi-threaded access
Event_Lock();
// ... thread-safe operations ...
Event_Unlock();
```

### Performance Monitoring

Built-in statistics and debugging:

```c
// Console command to show event statistics
event_stats

// Programmatic access
uint32_t queueSize = Event_GetQueueSize();
uint32_t subscriberCount = Event_GetSubscriptionCount(EVENT_TYPE_ENTITY_SPAWN);
```

## Usage Examples

### Entity Damage System

```c
// Define damage event data
typedef struct {
    int entityId;
    int damageAmount;
    int damageType;
    vec3_t damageOrigin;
} entityDamageData_t;

// Publish damage event
entityDamageData_t damageData = {
    .entityId = victimId,
    .damageAmount = 50,
    .damageType = DAMAGE_BULLET,
    .damageOrigin = {0, 0, 0}
};

PUBLISH_EVENT(EVENT_TYPE_ENTITY_DAMAGE, EVENT_CATEGORY_ENTITY,
              &damageData, sizeof(damageData));

// Handle damage event
void OnEntityDamaged(const event_t *event, void *userData) {
    const entityDamageData_t *damage = (const entityDamageData_t *)event->data;

    // Apply damage to entity
    ApplyDamage(damage->entityId, damage->damageAmount, damage->damageType);

    // Play damage sound
    PlayDamageSound(damage->damageOrigin);
}
```

### UI Event Handling

```c
// Subscribe to UI events
Event_Subscribe(EVENT_TYPE_UI_BUTTON_PRESS, OnButtonPressed,
                menuSystem, EVENT_PRIORITY_HIGH);

// Handle button press
void OnButtonPressed(const event_t *event, void *userData) {
    uiSystem_t *ui = (uiSystem_t *)userData;
    const uiButtonData_t *button = (const uiButtonData_t *)event->data;

    // Process button action
    ProcessUIButton(button->buttonId, button->action);
}
```

### Network Message Processing

```c
// Subscribe to network messages
Event_Subscribe(EVENT_TYPE_NET_MESSAGE, OnNetworkMessage,
                networkSystem, EVENT_PRIORITY_CRITICAL);

// Handle network messages
void OnNetworkMessage(const event_t *event, void *userData) {
    const netMessageData_t *msg = (const netMessageData_t *)event->data;

    // Process network message
    ProcessNetworkMessage(msg->clientId, msg->messageType, msg->data, msg->size);
}
```

## Integration Guidelines

### Engine Integration

1. **Initialize early**: Call `Event_Init()` during engine startup
2. **Process regularly**: Call `Event_ProcessAll()` each frame
3. **Shutdown cleanly**: Call `Event_Shutdown()` during engine shutdown

### Module Integration

1. **Register event types**: Use `REGISTER_EVENT_TYPE()` for custom events
2. **Subscribe during init**: Register handlers when module initializes
3. **Unsubscribe on shutdown**: Clean up subscriptions when module shuts down

### Threading Considerations

1. **Main thread processing**: Event processing should occur on main thread
2. **Lock for publishing**: Use `Event_Lock()` when publishing from other threads
3. **Deferred events**: Use deferred publishing for cross-thread communication

## Best Practices

### Event Design
- **Type safety**: Use strongly-typed event data structures
- **Minimal data**: Keep event payloads small and focused
- **Clear naming**: Use descriptive event type names
- **Category consistency**: Group related events in appropriate categories

### Handler Implementation
- **Idempotent**: Handlers should be safe to call multiple times
- **Fast execution**: Avoid blocking operations in handlers
- **Error handling**: Handle invalid event data gracefully
- **Memory management**: Don't store references to event data

### Performance Considerations
- **Subscription management**: Unsubscribe unused handlers
- **Event frequency**: Avoid high-frequency events for common operations
- **Memory usage**: Reuse event data structures when possible
- **Category filtering**: Disable unused event categories

## Migration Guide

### From Callbacks

**Old callback system:**
```c
// Direct function calls
OnEntityDamaged(victim, damage, attacker);

// Tightly coupled systems
damageSystem->processDamage(victim, damage, attacker);
```

**New event system:**
```c
// Publish event (decoupled)
PUBLISH_EVENT(EVENT_TYPE_ENTITY_DAMAGE, EVENT_CATEGORY_ENTITY,
              &damageData, sizeof(damageData));

// Subscribe to handle (flexible)
Event_Subscribe(EVENT_TYPE_ENTITY_DAMAGE, OnEntityDamaged,
                damageSystem, EVENT_PRIORITY_NORMAL);
```

### Benefits of Migration
- **Decoupling**: Systems no longer need direct knowledge of each other
- **Extensibility**: New handlers can be added without modifying publishers
- **Testability**: Events can be easily mocked and verified
- **Debugging**: Event flow is traceable and loggable

This event system provides a modern, scalable architecture for id Tech 3 that supports the complex interactions required by modern game development while maintaining the performance characteristics expected from a real-time engine.