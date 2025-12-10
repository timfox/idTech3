<?php
/**
 * Dynamic Resolution & Compute Particles (Plan)
 */
$title = 'Dynamic Resolution & Compute Particles (Plan) - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/dynres-plan' => 'Dynamic Resolution & Compute Particles (Plan)'
];
?>

<h1>Dynamic Resolution & Compute Particles (Plan)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>This document outlines the planned implementation for Vulkan dynamic resolution with TAA and a compute-based particle pipeline. The feature is scaffolded (cvars and state), but full functionality requires renderer changes noted below.</p>
</div>

<div class="section">
    <h2>Dynamic Resolution (Vulkan)</h2>
    <h3>What’s in place</h3>
    <ul>
        <li>Cvars: <code>r_dynRes_enable</code>, <code>r_dynRes_minScale</code>, <code>r_dynRes_maxScale</code>, <code>r_dynRes_targetMs</code>.</li>
        <li>Dyn-res state tracked in Vulkan renderer (currently fixed at 1.0 until scaling is wired).</li>
    </ul>
    <h3>Remaining work</h3>
    <ul>
        <li>Add scaled render targets (offscreen buffers) sized to dynamic resolution.</li>
        <li>Add a composite/upscale pass (with TAA) to present at full window size.</li>
        <li>Drive scale via GPU timing with hysteresis to avoid oscillation.</li>
        <li>Audit post passes to consume the scaled target and history correctly.</li>
    </ul>
</div>

<div class="section">
    <h2>Compute Particles (Vulkan)</h2>
    <h3>What’s in place</h3>
    <ul>
        <li>Cvars: <code>r_particles_enableCompute</code>, <code>r_particles_maxCount</code>.</li>
        <li>Placeholder state for GPU particles.</li>
    </ul>
    <h3>Remaining work</h3>
    <ul>
        <li>SSBO particle pool, compute update shader, and indirect draw path.</li>
        <li>Sprite material/shader, per-emitter LOD/cull.</li>
        <li>GPU timers for budgeting; default off until stable.</li>
    </ul>
</div>

<div class="section">
    <h2>Notes</h2>
    <ul>
        <li>Both features are Vulkan-only.</li>
        <li>Dynamic res will remain disabled until scaled attachments and composite are implemented.</li>
        <li>Compute particles will remain disabled until buffers/shaders are added.</li>
    </ul>
</div>

