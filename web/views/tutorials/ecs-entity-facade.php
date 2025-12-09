<?php
/**
 * ECS Entity Facade (C++ Wrapper)
 */
$title = 'ECS Entity Facade - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/ecs-entity-facade' => 'ECS Entity Facade'
];
?>

<h1>ECS Entity Facade (C++ Wrapper)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The Entity facade provides a C++ interface over the ECS C API, keeping gameplay code cleaner while remaining compatible with EnTT and legacy systems.</p>
</div>

<div class="section">
    <h2>Creating and Finding Entities</h2>
    <div class="code-block">
        <pre><code>// Create a new entity
auto e = Entity::Create();
if (!e) return;

// Fetch an existing entity from an index (e.g., sv/gentity index)
auto fromIndex = Entity::FromIndex(idx);
if (!fromIndex || !fromIndex->valid()) return;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Setting Components</h2>
    <div class="code-block">
        <pre><code>vec3_t pos = {0,0,0}, ang = {0,0,0}, scale = {1,1,1};
vec3_t vel = {0,0,0}, acc = {0,0,-9.8f};

e->setTransform(pos, ang, scale);
e->setPhysics(vel, acc, /*mass*/1.0f, /*friction*/0.2f, /*useBullet*/qfalse);
e->setHealth(100, 100, 25, 25);
e->setLifetime(5.0f);   // auto-despawn in 5 seconds
// e->clearLifetime();  // cancel despawn</code></pre>
    </div>
</div>

<div class="section">
    <h2>Mapping to Legacy Indices</h2>
    <p>Map ECS entities to legacy <code>gentity_t</code>/<code>svEntity_t</code> indices for networking.</p>
    <div class="code-block">
        <pre><code>if (e->mapToIndex(index)) {
    // now linked; NetworkComponent will carry the index/type
}
// Later...
e->unmapFromIndex(index);</code></pre>
    </div>
</div>

<div class="section">
    <h2>When to Use</h2>
    <ul>
        <li>Writing new gameplay systems in C++ without touching EnTT directly.</li>
        <li>Bridging legacy entities to ECS while keeping the C ABI stable.</li>
        <li>Attaching transient entities with lifetimes (e.g., VFX, temporary pickups).</li>
    </ul>
</div>

<div class="section">
    <h2>Notes</h2>
    <ul>
        <li>Facade methods return <code>bool</code>; check validity before use.</li>
        <li>Lifetime and component setters flag NetworkComponent for resync automatically.</li>
        <li>Behind the facade, calls route to the C ECS API and EnTT registry.</li>
    </ul>
</div>
<?php
/**
 * ECS Entity Facade (C++ Wrapper)
 */
$title = 'ECS Entity Facade - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/ecs-entity-facade' => 'ECS Entity Facade'
];
?>

<h1>ECS Entity Facade (C++ Wrapper)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The Entity facade provides a C++ interface over the ECS C API, keeping gameplay code cleaner while remaining compatible with EnTT and legacy systems.</p>
</div>

<div class="section">
    <h2>Creating and Finding Entities</h2>
    <div class="code-block">
        <pre><code>// Create a new entity
auto e = Entity::Create();
if (!e) return;

// Fetch an existing entity from an index (e.g., sv/gentity index)
auto fromIndex = Entity::FromIndex(idx);
if (!fromIndex || !fromIndex->valid()) return;</code></pre>
    </div>
</div>

<div class="section">
    <h2>Setting Components</h2>
    <div class="code-block">
        <pre><code>vec3_t pos = {0,0,0}, ang = {0,0,0}, scale = {1,1,1};
vec3_t vel = {0,0,0}, acc = {0,0,-9.8f};

e->setTransform(pos, ang, scale);
e->setPhysics(vel, acc, /*mass*/1.0f, /*friction*/0.2f, /*useBullet*/qfalse);
e->setHealth(100, 100, 25, 25);
e->setLifetime(5.0f);   // auto-despawn in 5 seconds
// e->clearLifetime();  // cancel despawn</code></pre>
    </div>
</div>

<div class="section">
    <h2>Mapping to Legacy Indices</h2>
    <p>Map ECS entities to legacy <code>gentity_t</code>/<code>svEntity_t</code> indices for networking.</p>
    <div class="code-block">
        <pre><code>if (e->mapToIndex(index)) {
    // now linked; NetworkComponent will carry the index/type
}
// Later...
e->unmapFromIndex(index);</code></pre>
    </div>
</div>

<div class="section">
    <h2>When to Use</h2>
    <ul>
        <li>Writing new gameplay systems in C++ without touching EnTT directly.</li>
        <li>Bridging legacy entities to ECS while keeping the C ABI stable.</li>
        <li>Attaching transient entities with lifetimes (e.g., VFX, temporary pickups).</li>
    </ul>
</div>

<div class="section">
    <h2>Notes</h2>
    <ul>
        <li>Facade methods return <code>bool</code>; check validity before use.</li>
        <li>Lifetime and component setters flag NetworkComponent for resync automatically.</li>
        <li>Behind the facade, calls route to the C ECS API and EnTT registry.</li>
    </ul>
</div>

