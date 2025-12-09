<?php
/**
 * Object-Oriented Programming Improvements
 */
$title = 'Object-Oriented Programming Improvements - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/oop-improvements' => 'Object-Oriented Programming Improvements'
];
?>

<h1>Object-Oriented Programming Improvements</h1>

<div class="section">
    <h2>What’s Already Done</h2>
    <ul>
        <li><strong>ECS integration (EnTT):</strong> Engine exposes a C-callable ECS API with helpers for Transform, Physics, Health, and a Lifetime component that auto-despawns entities.</li>
        <li><strong>Lifetime system:</strong> Per-frame lifetime update runs before physics/scripts, destroying entities when their timers expire.</li>
        <li><strong>Entity facade (C++):</strong> A thin C++ wrapper over the ECS C API for cleaner gameplay code (create/find, set components, map to legacy indices).</li>
        <li><strong>Service locator/logger:</strong> Lightweight C++ logger interface with a default bridge to <code>Com_Printf</code>, enabling injectable logging without globals.</li>
        <li><strong>Optional OOP bridge:</strong> Class registration layer that can route classnames to C++ entity classes while preserving the legacy C ABI (behind a toggle).</li>
    </ul>
</div>

<div class="section">
    <h2>Where to Improve Next</h2>
    <ul>
        <li><strong>Subsystem interfaces:</strong> Define vtable-style interfaces for renderer, audio, input, filesystem, and network; inject implementations instead of using globals.</li>
        <li><strong>Resource ownership/RAII:</strong> Wrap GPU handles, file descriptors, and temporary buffers in small owner types with clear init/teardown to reduce leak-prone code paths.</li>
        <li><strong>Typed configs and flags:</strong> Centralize feature/config toggles into typed C++ structs or a small registry (e.g., using <code>std::string_view</code> and <code>std::unordered_map</code>) rather than ad hoc globals.</li>
        <li><strong>Modern std types:</strong> Adopt <code>std::expected</code> for loaders/parsers, <code>std::span</code> for contiguous buffers, and <code>std::string_view</code> for read-only strings in new C++ code.</li>
        <li><strong>Boundary hygiene:</strong> Keep C ABI stable; wrap C++23 helpers behind C-callable shims when crossing legacy VM/syscall boundaries.</li>
        <li><strong>Testing seams:</strong> Use interfaces/service locator to inject mocks for renderer/audio/fs in headless tests.</li>
    </ul>
</div>

<div class="section">
    <h2>Suggested Next Steps (Actionable)</h2>
    <ol>
        <li>Add small interface structs (function-pointer vtables) for filesystem and input, and route existing globals through them.</li>
        <li>Introduce RAII helpers for GL/VK handles (buffers, images, pipelines) in new code paths to simplify teardown.</li>
        <li>Migrate new parsers/loaders to <code>std::expected</code> and <code>std::span</code> to reduce error-prone pointer/length pairs.</li>
        <li>Expose a minimal service locator for subsystems (renderer/audio/fs/logger) and pass it through init instead of global lookups.</li>
        <li>Expand the Entity facade with convenience helpers (e.g., tag/components checks, safe lookup by index) while keeping the C ABI untouched.</li>
    </ol>
</div>

<div class="section">
    <h2>Notes</h2>
    <ul>
        <li>Prefer incremental refactors: wrap and redirect existing code paths instead of wholesale rewrites.</li>
        <li>Keep legacy VM and syscall interfaces stable; isolate C++23 usage behind clearly owned modules.</li>
        <li>Use logging and assertions sparingly in hot paths; keep debug builds strict and release builds lean.</li>
    </ul>
</div>
<?php
/**
 * Object-Oriented Programming Improvements
 */
$title = 'Object-Oriented Programming Improvements - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/oop-improvements' => 'Object-Oriented Programming Improvements'
];
?>

<h1>Object-Oriented Programming Improvements</h1>

<div class="section">
    <h2>What’s Already Done</h2>
    <ul>
        <li><strong>ECS integration (EnTT):</strong> Engine exposes a C-callable ECS API with helpers for Transform, Physics, Health, and a Lifetime component that auto-despawns entities.</li>
        <li><strong>Lifetime system:</strong> Per-frame lifetime update runs before physics/scripts, destroying entities when their timers expire.</li>
        <li><strong>Entity facade (C++):</strong> A thin C++ wrapper over the ECS C API for cleaner gameplay code (create/find, set components, map to legacy indices).</li>
        <li><strong>Service locator/logger:</strong> Lightweight C++ logger interface with a default bridge to <code>Com_Printf</code>, enabling injectable logging without globals.</li>
        <li><strong>Optional OOP bridge:</strong> Class registration layer that can route classnames to C++ entity classes while preserving the legacy C ABI (behind a toggle).</li>
    </ul>
</div>

<div class="section">
    <h2>Where to Improve Next</h2>
    <ul>
        <li><strong>Subsystem interfaces:</strong> Define vtable-style interfaces for renderer, audio, input, filesystem, and network; inject implementations instead of using globals.</li>
        <li><strong>Resource ownership/RAII:</strong> Wrap GPU handles, file descriptors, and temporary buffers in small owner types with clear init/teardown to reduce leak-prone code paths.</li>
        <li><strong>Typed configs and flags:</strong> Centralize feature/config toggles into typed C++ structs or a small registry (e.g., using <code>std::string_view</code> and <code>std::unordered_map</code>) rather than ad hoc globals.</li>
        <li><strong>Modern std types:</strong> Adopt <code>std::expected</code> for loaders/parsers, <code>std::span</code> for contiguous buffers, and <code>std::string_view</code> for read-only strings in new C++ code.</li>
        <li><strong>Boundary hygiene:</strong> Keep C ABI stable; wrap C++23 helpers behind C-callable shims when crossing legacy VM/syscall boundaries.</li>
        <li><strong>Testing seams:</strong> Use interfaces/service locator to inject mocks for renderer/audio/fs in headless tests.</li>
    </ul>
</div>

<div class="section">
    <h2>Suggested Next Steps (Actionable)</h2>
    <ol>
        <li>Add small interface structs (function-pointer vtables) for filesystem and input, and route existing globals through them.</li>
        <li>Introduce RAII helpers for GL/VK handles (buffers, images, pipelines) in new code paths to simplify teardown.</li>
        <li>Migrate new parsers/loaders to <code>std::expected</code> and <code>std::span</code> to reduce error-prone pointer/length pairs.</li>
        <li>Expose a minimal service locator for subsystems (renderer/audio/fs/logger) and pass it through init instead of global lookups.</li>
        <li>Expand the Entity facade with convenience helpers (e.g., tag/components checks, safe lookup by index) while keeping the C ABI untouched.</li>
    </ol>
</div>

<div class="section">
    <h2>Notes</h2>
    <ul>
        <li>Prefer incremental refactors: wrap and redirect existing code paths instead of wholesale rewrites.</li>
        <li>Keep legacy VM and syscall interfaces stable; isolate C++23 usage behind clearly owned modules.</li>
        <li>Use logging and assertions sparingly in hot paths; keep debug builds strict and release builds lean.</li>
    </ul>
</div>

