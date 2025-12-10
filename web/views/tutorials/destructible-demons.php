<?php
/**
 * Destructible Demons & FX Pipeline Overview
 */
$title = 'Destructible Demons & FX Pipeline - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/destructible-demons' => 'Destructible Demons & FX Pipeline'
];
?>

<h1>Destructible Demons & FX Pipeline</h1>

<div class="section">
    <h2>Overview</h2>
    <p>This page outlines a high-end feature set combining procedural dismemberment, GPU-accelerated particles with Alembic caching, and adaptive resolution to sustain performance under heavy effects.</p>
</div>

<div class="section">
    <h2>Destructible Demons</h2>
    <ul>
        <li><strong>Procedural, physics-based dismemberment:</strong> Multi-layer enemy geometry allows dynamic limb loss based on hit location/force.</li>
        <li><strong>Gore layers:</strong> Secondary meshes/decals for wounds and splatters that reveal inner layers.</li>
        <li><strong>Integration points:</strong> Hit detection feeds a damage model; physics constraints or breakables detach limbs; animation state blends to “maimed” poses.</li>
        <li><strong>Authoring:</strong> Enemies need segmented meshes, break joints, and gore mask data; VFX hooks for blood decals/particles.</li>
    </ul>
</div>

<div class="section">
    <h2>GPU-Accelerated Particles & Alembic Caching</h2>
    <ul>
        <li><strong>Compute-driven particles:</strong> Large-scale explosions, volumetrics, and debris simulated on GPU to reduce CPU cost.</li>
        <li><strong>Alembic-cached animations:</strong> Complex, “writhing” FX (e.g., tentacles) baked to Alembic and played back on GPU for stability and scale.</li>
        <li><strong>Throughput goal:</strong> Twice the gameplay area size handling dense FX by offloading to GPU.</li>
        <li><strong>Content:</strong> Particle emitters authored for compute, Alembic caches exported at reasonable sampling rates; LODs for distant playback.</li>
    </ul>
</div>

<div class="section">
    <h2>Dynamic Resolution & Adaptive Tech</h2>
    <ul>
        <li><strong>Target:</strong> Sustained 60 FPS on mid/high hardware (e.g., ~1800p on Xbox One X) by adapting resolution under load.</li>
        <li><strong>Temporal upsampling/TAA:</strong> Preserves visual stability at lower internal resolutions.</li>
        <li><strong>Strategy:</strong> Monitor GPU frame time; drop internal render resolution when heavy FX/dismemberment hits; recover when load drops.</li>
    </ul>
</div>

<div class="section">
    <h2>Integration Considerations</h2>
    <ul>
        <li><strong>Pipeline:</strong> Requires renderer support for compute particles, dynamic resolution, and per-object LODs; gameplay needs damage-to-dismemberment mapping.</li>
        <li><strong>Physics:</strong> Limb detachment should be stable; consider joint break thresholds and post-break ragdoll.</li>
        <li><strong>Content pipeline:</strong> Alembic export path, segmented enemy rigs, authored gore layers, particle authoring tools.</li>
        <li><strong>Performance:</strong> Budget GPU memory for caches (particles, Alembic, gore decals) and tune dynamic res thresholds.</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Tips</h2>
    <ul>
        <li>Use GPU timers to gate dynamic resolution changes; avoid oscillation with hysteresis.</li>
        <li>Clamp particle/Alembic LODs at distance; cull off-screen FX aggressively.</li>
        <li>Limit active dismembered ragdolls; pool gore decals/particles to prevent leaks.</li>
    </ul>
</div>

