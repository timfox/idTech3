<?php
/**
 * Vulkan Pipeline Cache FAQ
 */
$title = 'Vulkan Pipeline Cache FAQ - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/vk-pipeline-cache-faq' => 'Vulkan Pipeline Cache FAQ'
];
?>

<h1>Vulkan Pipeline Cache FAQ</h1>

<div class="section">
    <h2>Where is the cache stored?</h2>
    <p><code>release/pipeline_cache_vk.bin</code>. Created on first run, reused on subsequent runs.</p>
</div>

<div class="section">
    <h2>Do I need to enable it?</h2>
    <p>No. It is automatic—loaded at startup, saved at shutdown.</p>
</div>

<div class="section">
    <h2>When should I delete it?</h2>
    <ul>
        <li>After GPU driver updates that change pipeline compatibility.</li>
        <li>If you see pipeline creation errors or validation warnings referencing cache data.</li>
    </ul>
</div>

<div class="section">
    <h2>Can I ship it with builds?</h2>
    <p>Yes. Keep the cache alongside build artifacts to reduce pipeline warm-up stalls on first launch. If incompatible, Vulkan will rebuild on next run after you delete the file.</p>
</div>

<div class="section">
    <h2>What if the cache is corrupted?</h2>
    <p>Delete the file; it will be regenerated.</p>
</div>

<div class="section">
    <h2>Any performance tips?</h2>
    <ul>
        <li>Run the app once after shader changes to pre-seed the cache.</li>
        <li>Keep the cache per build configuration; avoid mixing debug/release caches.</li>
    </ul>
</div>

