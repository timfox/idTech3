# Event Phases - Explicit Event Timing

## Overview

The event system supports three explicit event phases for controlling when events are processed:

1. **Immediate** - Processed in the same frame/tick
2. **Deferred** - Processed at the end of the frame
3. **Scheduled** - Processed after a specified delay

## Event Phases

### Immediate Phase (`EVENT_PHASE_IMMEDIATE`)

Events published with immediate phase are processed synchronously in the current frame/tick. This is the default behavior for `Event_Publish()`.

**Use cases:**
- Critical events that must be handled immediately
- Events that affect the current frame's rendering or logic
- Low-latency requirements

**Example:**
```c
// Publish immediately (default)
PUBLISH_EVENT(EVENT_TYPE_ENTITY_DAMAGE, EVENT_CATEGORY_ENTITY, &damageData, sizeof(damageData));

// Or explicitly
event_t *evt = Event_Create(EVENT_TYPE_ENTITY_DAMAGE, EVENT_CATEGORY_ENTITY, sizeof(damageData));
// ... set event data ...
Event_PublishImmediate(evt);
```

### Deferred Phase (`EVENT_PHASE_DEFERRED`)

Events published with deferred phase are queued and processed at the end of the frame, after all immediate events have been handled.

**Use cases:**
- Non-critical events that can wait until frame end
- Events that should not interrupt current processing
- Batch processing of similar events

**Example:**
```c
// Publish deferred
Event_PublishDeferred(evt);

// Or use macro with phase
PUBLISH_EVENT_PHASE(EVENT_TYPE_UI_UPDATE, EVENT_CATEGORY_UI, EVENT_PHASE_DEFERRED, &uiData, sizeof(uiData));
```

### Scheduled Phase (`EVENT_PHASE_SCHEDULED`)

Events published with scheduled phase are queued and processed after a specified delay (in milliseconds).

**Use cases:**
- Delayed actions (e.g., "destroy entity in 5 seconds")
- Timed events (e.g., "spawn enemy after 10 seconds")
- Debouncing/throttling
- Scripted sequences with timing

**Example:**
```c
// Schedule event for 1000ms (1 second) in the future
Event_PublishScheduled(evt, 1000);

// Or use macro
PUBLISH_EVENT_SCHEDULED(EVENT_TYPE_ENTITY_DESTROY, EVENT_CATEGORY_ENTITY, 5000, NULL, 0);
```

## Processing Order

Events are processed in the following order each frame:

1. **Immediate events** - Processed synchronously when published
2. **Scheduled events** - Processed if their scheduled time has been reached
3. **Deferred events** - Processed at end of frame

This order ensures:
- Critical immediate events are handled first
- Scheduled events are checked each frame
- Non-critical deferred events are batched at frame end

## API Reference

### Functions

```c
// Publish with explicit phase
qboolean Event_PublishImmediate(event_t *event);
qboolean Event_PublishDeferred(event_t *event);
qboolean Event_PublishScheduled(event_t *event, uint32_t delayMs);

// Process events by phase
void Event_ProcessImmediate(void);
void Event_ProcessDeferred(void);
void Event_ProcessScheduled(void);
void Event_ProcessAll(void);  // Processes all phases in order
```

### Macros

```c
// Standard immediate publish
PUBLISH_EVENT(type, category, data, size)

// Publish with explicit phase
PUBLISH_EVENT_PHASE(type, category, phase, data, size)

// Publish scheduled event
PUBLISH_EVENT_SCHEDULED(type, category, delayMs, data, size)
```

## Best Practices

### When to Use Immediate Phase

- **Critical gameplay events**: Damage, death, spawn
- **Frame-dependent events**: Input, rendering triggers
- **Low-latency requirements**: Network events, audio triggers

### When to Use Deferred Phase

- **UI updates**: Menu changes, HUD updates
- **Non-critical notifications**: Log messages, debug info
- **Batch operations**: Multiple similar events that can be processed together

### When to Use Scheduled Phase

- **Delayed actions**: Timed destruction, delayed spawns
- **Scripted sequences**: Cutscenes, timed encounters
- **Debouncing**: Input handling, network throttling

## Deterministic Ordering

For deterministic behavior (important for networking and replay):

1. **Immediate events** are processed in publish order within the same frame
2. **Scheduled events** are processed in scheduled time order (earliest first)
3. **Deferred events** are processed in publish order

**Note**: Priority-based ordering is planned for future enhancement. Currently, events within the same phase are processed FIFO.

## Thread Safety

All event phase operations are **not thread-safe** by default. For multi-threaded code:

- Use `Event_Lock()` / `Event_Unlock()` around event operations
- Or publish from background threads and process on main thread
- Consider using deferred phase for cross-thread events

## Performance Considerations

- **Immediate events**: Lowest latency, but can block current processing
- **Deferred events**: Better for batching, minimal frame impact
- **Scheduled events**: Efficient time-based queuing, checked each frame

**Recommendation**: Use immediate for critical events, deferred for non-critical, scheduled for timed events.

## Example: Entity Destruction with Delay

```c
// Entity takes damage
void Entity_TakeDamage(entity_t *ent, int damage) {
    // Immediate: Apply damage now
    ent->health -= damage;
    
    if (ent->health <= 0) {
        // Scheduled: Destroy entity after 2 seconds (for death animation)
        event_t *deathEvt = Event_Create(EVENT_TYPE_ENTITY_DEATH, EVENT_CATEGORY_ENTITY, 0);
        Event_PublishScheduled(deathEvt, 2000);
    }
}
```

## Integration with Frame Loop

The event system is integrated into the engine frame loop:

```c
void Com_Frame(void) {
    // Frame start
    PUBLISH_EVENT(EVENT_TYPE_ENGINE_FRAME_START, EVENT_CATEGORY_ENGINE, NULL, 0);
    
    // ... frame processing ...
    
    // Process all event phases
    Event_ProcessAll();  // Processes immediate, scheduled, then deferred
    
    // Frame end
    PUBLISH_EVENT(EVENT_TYPE_ENGINE_FRAME_END, EVENT_CATEGORY_ENGINE, NULL, 0);
}
```

## See Also

- `docs/EVENT_SYSTEM.md` - General event system documentation
- `src/qcommon/event_system.h` - Event system API
- `src/qcommon/event_system.c` - Event system implementation
