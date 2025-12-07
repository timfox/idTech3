# Entity OOP Plan (C++-first)

Goal: adopt a Half-Life–style entity OOP model using C++ for new gamecode while keeping compatibility with existing C/QVM entry points via a stable C ABI shim.

## Boundary Rules
- Engine/VM ↔ gamecode surface stays `extern "C"` with POD structs (no RTTI/exceptions across the boundary).
- Net/save structs (`gentity_t`, `playerState_t`, snapshots) remain C-compatible layouts.
- No C++ exceptions crossing the C boundary; treat gamecode as `-fno-exceptions` (or catch/translate internally).
- Keep allocator and logging calls in the C ABI layer to avoid mixing allocators across boundaries.

## Core Types (C++)
- `class BaseEntity` with virtuals: `Spawn()`, `Precache()`, `Think(float dt)`, `ScheduleNextThink(int msec)`, `Touch(BaseEntity*)`, `Use(BaseEntity*)`, `TakeDamage(const DamageInfo&)`, `OnPain(const DamageInfo&)`, `OnDeath(const DamageInfo&)`, `Save(SaveWriter&)`, `Load(SaveReader&)`.
- `struct EntityClass` descriptor: `const char* classname`, factory func, optional flags/caps, pointer to a field table for net/save (offset, type, version, default).
- `struct DamageInfo`, `struct EventContext` as small PODs usable from C.

## Factory/Registry
- A global registry mapping classname → `EntityClass`.
- Fallback path calls legacy C spawn funcs when no registered class exists.
- Registration via a small macro/helper to avoid static-init order hazards (explicit `RegisterEntities()` call).

## Mixins/Reuse
- Prefer mixins/components for common behaviors (movement modes, doors/triggers, health/damage handling, render binding) instead of deep inheritance.
- Keep base class small; store class-local state via composition inside each derived type.

## Messaging/Event Bus
- Lightweight `FireOutput(name, payload)` and `OnInput(name, payload)` with optional filters.
- Simple event bus to decouple entities (publish/subscribe within the level).

## Net/Save Descriptors
- Per-class field table reused for both snapshot encode/decode and save/load.
- Include versioning/defaulting to allow schema evolution.

## Rollout Plan
1) Add C ABI shim headers (`extern "C"`) that expose spawn/think hooks calling into C++.
2) Implement registry + `BaseEntity` skeleton in C++.
3) Bridge legacy entities: start with `func_door`, `trigger_multiple`, one NPC; keep a cvar to toggle legacy vs new path.
4) Verify snapshot/save compatibility; add regression tests for serialization.
5) Gradually move shared behaviors into mixins; keep legacy path intact until confidence is high.

## Backward Compatibility
- Default behavior stays on the legacy C/QVM path; new C++ entities only engage when explicitly enabled (cvar or build flag).
- Fallback: if a classname is unregistered in the C++ registry, spawn falls back to the legacy C spawn function.
- Snapshot/save format remains unchanged; new per-class field tables must map onto existing serialized fields or supply defaults when absent.
- Keep QVM builds untouched: C ABI shims are headers-only for QVM; C++ code is only built for native gamecode targets.
- Provide a kill-switch cvar (e.g., `g_oopEntities 0/1`) to toggle C++ entities at runtime for A/B validation.

## Build Notes
- Build new gamecode objects as C++; keep engine-side build unchanged.
- Ensure headers included from C code are guarded for C++ (`extern "C"` on APIs, C-compatible PODs only).
- Consider `-fno-exceptions -fno-rtti` for gamecode to keep binaries small and boundary-safe.

