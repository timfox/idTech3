<?php
/**
 * OOP Entity Bridge (g_oopEntities) Tutorial
 */
$title = 'OOP Entity Bridge - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/oop-entities' => 'OOP Entity Bridge'
];
?>

<h1>OOP Entity Bridge (g_oopEntities)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The OOP bridge lets you register C++ entity classes and route map classnames through them while keeping the legacy <code>gentity_t</code> ABI. It is gated by the cvar <code>g_oopEntities</code>.</p>
</div>

<div class="section">
    <h2>Enabling</h2>
    <div class="code-block">
        <pre><code>/set g_oopEntities 1</code></pre>
    </div>
    <p>When enabled, the bridge initializes the EnTT registry and the C++ class registry during <code>G_InitGame</code>.</p>
</div>

<div class="section">
    <h2>Registering Classes</h2>
    <p>In C++ gamecode (e.g., <code>g_oop.cpp</code>):</p>
    <div class="code-block">
        <pre><code>oop::RegisterClass( oop::EntityClass{
    "func_door",
    []( gentity_t *self, entt::entity ) {
        // Option A: call legacy spawn
        SP_func_door(self);
        return std::make_unique<MyDoor>( self );
    }
});</code></pre>
    </div>
    <p>Use <code>RegisterClass</code> to map a map classname to a C++ factory.</p>
</div>

<div class="section">
    <h2>Lifecycle</h2>
    <ul>
        <li>On spawn: <code>G_OOP_CallSpawn</code> looks up the classname; if found, it creates a C++ instance and attaches it via an ECS component.</li>
        <li>On shutdown/restart: instances are destroyed and mappings are cleared.</li>
        <li>Fallback: if no class is registered, the legacy C spawn is used.</li>
    </ul>
</div>

<div class="section">
    <h2>When to Use</h2>
    <ul>
        <li>Porting selected entities to C++ while keeping map compatibility.</li>
        <li>Adding new behavior without editing legacy C spawns.</li>
        <li>Coexisting with ECS: you can attach components inside your C++ Spawn.</li>
    </ul>
</div>

<div class="section">
    <h2>Notes</h2>
    <ul>
        <li>Keep the C ABI stable for existing engine/syscall interactions.</li>
        <li>Use the facade/ECS setters to add components from C++ code.</li>
        <li>Guard complex features behind feature flags (see Blacksun feature flags tutorial).</li>
    </ul>
</div>

