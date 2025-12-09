<?php
/**
 * ECS Lifetime & Component Helpers Tutorial
 */
$title = 'ECS Lifetime & Component Helpers - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/ecs-lifetime' => 'ECS Lifetime & Component Helpers'
];
?>

<h1>ECS Lifetime & Component Helpers</h1>

<div class="section">
    <h2>What This Covers</h2>
    <ul>
        <li>Using the C-callable ECS setters for Transform, Physics, and Health</li>
        <li>Adding per-entity lifetimes that auto-despawn</li>
        <li>How the lifetime system runs each frame</li>
    </ul>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>Build with <code>USE_ENTT</code> enabled</li>
        <li>Call <code>ECS_Init()</code> during startup</li>
    </ul>
</div>

<div class="section">
    <h2>Creating and Populating Entities</h2>
    <div class="code-block">
        <pre><code>// Create an entity
ecs_entity_t e = ECS_CreateEntity();

// Set transform
vec3_t pos = {0, 0, 0}, ang = {0, 0, 0}, scale = {1, 1, 1};
ECS_SetTransform(e, pos, ang, scale);

// Set physics (enable Bullet by passing qtrue as last param)
vec3_t vel = {0, 0, 0}, accel = {0, 0, -9.8f};
ECS_SetPhysics(e, vel, accel, /*mass*/1.0f, /*friction*/0.2f, qfalse);

// Set health
ECS_SetHealth(e, /*hp*/100, /*max*/100, /*armor*/25, /*armor max*/25);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Auto-Despawning with Lifetime</h2>
    <p>Attach a LifetimeComponent to have the engine delete an entity after N seconds.</p>
    <div class="code-block">
        <pre><code>// Despawn after 5 seconds
ECS_SetLifetime(e, 5.0f);

// Cancel lifetime
ECS_ClearLifetime(e);</code></pre>
    </div>
    <p>The lifetime system runs at the start of <code>ECS_RunFrame(deltaTime)</code> before physics and scripts.</p>
</div>

<div class="section">
    <h2>Frame Loop</h2>
    <p>Call once per frame:</p>
    <div class="code-block">
        <pre><code>ECS_RunFrame(deltaTime);</code></pre>
    </div>
    <p>Order: Lifetime → Physics (and Bullet if enabled) → Health → Scripts → Network sync.</p>
</div>

<div class="section">
    <h2>Notes & Tips</h2>
    <ul>
        <li>Lifetime uses a simple countdown; when it reaches zero the entity is destroyed via ECS.</li>
        <li>Physics setter tears down Bullet bodies if you disable Bullet for an entity.</li>
        <li>Network components get <code>needsSync</code> flagged when setters change data.</li>
    </ul>
</div>
<?php
/**
 * ECS Lifetime & Component Helpers Tutorial
 */
$title = 'ECS Lifetime & Component Helpers - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/ecs-lifetime' => 'ECS Lifetime & Component Helpers'
];
?>

<h1>ECS Lifetime & Component Helpers</h1>

<div class="section">
    <h2>What This Covers</h2>
    <ul>
        <li>Using the C-callable ECS setters for Transform, Physics, and Health</li>
        <li>Adding per-entity lifetimes that auto-despawn</li>
        <li>How the lifetime system runs each frame</li>
    </ul>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>Build with <code>USE_ENTT</code> enabled</li>
        <li>Call <code>ECS_Init()</code> during startup</li>
    </ul>
</div>

<div class="section">
    <h2>Creating and Populating Entities</h2>
    <div class="code-block">
        <pre><code>// Create an entity
ecs_entity_t e = ECS_CreateEntity();

// Set transform
vec3_t pos = {0, 0, 0}, ang = {0, 0, 0}, scale = {1, 1, 1};
ECS_SetTransform(e, pos, ang, scale);

// Set physics (enable Bullet by passing qtrue as last param)
vec3_t vel = {0, 0, 0}, accel = {0, 0, -9.8f};
ECS_SetPhysics(e, vel, accel, /*mass*/1.0f, /*friction*/0.2f, qfalse);

// Set health
ECS_SetHealth(e, /*hp*/100, /*max*/100, /*armor*/25, /*armor max*/25);</code></pre>
    </div>
</div>

<div class="section">
    <h2>Auto-Despawning with Lifetime</h2>
    <p>Attach a LifetimeComponent to have the engine delete an entity after N seconds.</p>
    <div class="code-block">
        <pre><code>// Despawn after 5 seconds
ECS_SetLifetime(e, 5.0f);

// Cancel lifetime
ECS_ClearLifetime(e);</code></pre>
    </div>
    <p>The lifetime system runs at the start of <code>ECS_RunFrame(deltaTime)</code> before physics and scripts.</p>
</div>

<div class="section">
    <h2>Frame Loop</h2>
    <p>Call once per frame:</p>
    <div class="code-block">
        <pre><code>ECS_RunFrame(deltaTime);</code></pre>
    </div>
    <p>Order: Lifetime → Physics (and Bullet if enabled) → Health → Scripts → Network sync.</p>
</div>

<div class="section">
    <h2>Notes & Tips</h2>
    <ul>
        <li>Lifetime uses a simple countdown; when it reaches zero the entity is destroyed via ECS.</li>
        <li>Physics setter tears down Bullet bodies if you disable Bullet for an entity.</li>
        <li>Network components get <code>needsSync</code> flagged when setters change data.</li>
    </ul>
</div>

