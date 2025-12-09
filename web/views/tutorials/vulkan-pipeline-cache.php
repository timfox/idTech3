<?php
/**
 * Vulkan Pipeline Cache Persistence Tutorial
 */
$title = 'Vulkan Pipeline Cache Persistence - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/vulkan-pipeline-cache' => 'Vulkan Pipeline Cache Persistence'
];
?>

<h1>Vulkan Pipeline Cache Persistence</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The Vulkan renderer now loads and saves a pipeline cache to speed up shader/pipeline creation across runs. The cache lives on disk and is reused automatically.</p>
</div>

<div class="section">
    <h2>Cache Location</h2>
    <ul>
        <li>Path: <code>release/pipeline_cache_vk.bin</code></li>
        <li>Created on first run; subsequent launches reuse it</li>
    </ul>
</div>

<div class="section">
    <h2>How It Works</h2>
    <ol>
        <li>On startup, the renderer reads <code>pipeline_cache_vk.bin</code> (if present) and feeds it to <code>vkCreatePipelineCache</code>.</li>
        <li>Pipelines created during the session are added to the cache in memory.</li>
        <li>On shutdown, the cache is written back via <code>vkGetPipelineCacheData</code>.</li>
    </ol>
</div>

<div class="section">
    <h2>Usage Tips</h2>
    <ul>
        <li>No configuration needed—the cache is automatic.</li>
        <li>If you update drivers or see pipeline errors, delete <code>release/pipeline_cache_vk.bin</code> and restart to rebuild it.</li>
        <li>Keep the cache with build artifacts to avoid shader warm-up stalls on first frame.</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>Corrupted cache: delete the file; it will be regenerated.</li>
        <li>Disk permissions: ensure the process can write to <code>release/</code>.</li>
        <li>Validation errors after driver updates: clear the cache and rerun.</li>
    </ul>
</div>

