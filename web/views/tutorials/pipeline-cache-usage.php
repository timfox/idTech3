<?php
// This page is superseded by pipeline-cache.php; kept as a redirect/stub for compatibility.
header('Location: /tutorials/pipeline-cache');
exit;
<?php
/**
 * Vulkan Pipeline Cache Usage Guide
 */
$title = 'Vulkan Pipeline Cache Usage Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/pipeline-cache-usage' => 'Vulkan Pipeline Cache Usage'
];
?>

<h1>Vulkan Pipeline Cache Usage Guide</h1>

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
    <h2>Troubleshooting</h2>
    <ul>
        <li><strong>Validation/pipeline errors after a driver update:</strong> delete the cache and relaunch.</li>
        <li><strong>Cache not written:</strong> ensure the process can write to <code>release/</code>.</li>
        <li><strong>Corruption:</strong> delete the cache; it will regenerate.</li>
    </ul>
</div>

