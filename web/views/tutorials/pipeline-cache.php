<?php
/**
 * Vulkan Pipeline Cache Guide (with FAQ)
 */
$title = 'Vulkan Pipeline Cache Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/pipeline-cache' => 'Vulkan Pipeline Cache Guide'
];
?>

<h1>Vulkan Pipeline Cache Guide (with FAQ)</h1>

<div class="section">
    <h2>Quick Facts</h2>
    <ul>
        <li>Cache file: <code>release/pipeline_cache_vk.bin</code></li>
        <li>Automatic: loaded on startup, saved on shutdown</li>
        <li>Benefit: faster pipeline creation after first run</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li>Keep one cache per build config (don’t mix debug/release caches).</li>
        <li>After shader or driver changes, delete the cache if you see pipeline errors; it will rebuild on next run.</li>
        <li>Ship the cache with release artifacts to reduce first-frame stalls on user machines.</li>
        <li>Run once on target hardware to warm the cache before distributing builds.</li>
        <li>Path reminder: <code>release/pipeline_cache_vk.bin</code> is created/updated automatically at shutdown.</li>
    </ul>
</div>

<div class="section">
    <h2>Rebuilding the Cache</h2>
    <ol>
        <li>Delete <code>release/pipeline_cache_vk.bin</code>.</li>
        <li>Launch the game; pipelines will be rebuilt and saved on exit.</li>
    </ol>
</div>

<div class="section">
    <h2>FAQ</h2>
    <ul>
        <li><strong>Do I need to enable it?</strong> No, it’s automatic.</li>
        <li><strong>When should I delete it?</strong> After driver changes or shader layout changes that trigger pipeline errors.</li>
        <li><strong>Can I ship it?</strong> Yes. Include it with builds to reduce warm-up stalls; if incompatible, delete and regenerate.</li>
        <li><strong>Corrupted cache?</strong> Delete the file; it will regenerate on next run.</li>
        <li><strong>Cache not written?</strong> Ensure the process can write to <code>release/</code>.</li>
    </ul>
</div>

