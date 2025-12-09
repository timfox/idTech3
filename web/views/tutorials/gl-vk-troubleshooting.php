<?php
/**
 * GL/VK Troubleshooting Guide
 */
$title = 'GL/VK Troubleshooting - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/gl-vk-troubleshooting' => 'GL/VK Troubleshooting'
];
?>

<h1>GL/VK Troubleshooting</h1>

<div class="section">
    <h2>Common GL Issues</h2>
    <ul>
        <li><strong>Missing SSBO support:</strong> Clustered lighting won’t engage; ensure driver supports SSBOs or disable <code>r_clusteredLight</code>.</li>
        <li><strong>Black textures:</strong> Check texture path/pk3 availability and shaderlist entries.</li>
        <li><strong>GL errors:</strong> Use debug context if available and watch console for GL error spam.</li>
    </ul>
</div>

<div class="section">
    <h2>Common Vulkan Issues</h2>
    <ul>
        <li><strong>Validation errors:</strong> Enable validation layers; fix reported stage/access mask mismatches and descriptor binding errors.</li>
        <li><strong>Pipeline creation failures:</strong> Delete <code>release/pipeline_cache_vk.bin</code> and relaunch; recompile shaders with <code>glslangValidator</code>/<code>spirv-val</code>.</li>
        <li><strong>Swapchain resize:</strong> Ensure swapchain recreation paths handle window resizes; watch for stale framebuffers.</li>
        <li><strong>Descriptor issues:</strong> Confirm descriptor set layouts match shaders; check binding counts and types.</li>
    </ul>
</div>

<div class="section">
    <h2>Debug Tools</h2>
    <ul>
        <li><strong>RenderDoc:</strong> Capture frames; inspect resources, pipelines, and barriers.</li>
        <li><strong>VK Validation Layers:</strong> Keep enabled during development; silence only after resolving issues.</li>
        <li><strong>glslangValidator / spirv-val:</strong> Validate shaders before packaging.</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Checks</h2>
    <ul>
        <li><strong>Over-synchronization:</strong> Tighten pipeline barriers; avoid ALL_COMMANDS stage masks.</li>
        <li><strong>Descriptor churn:</strong> Reuse descriptor sets/pools when possible.</li>
        <li><strong>Pipeline churn:</strong> Warm the pipeline cache and minimize pipeline permutations.</li>
    </ul>
</div>

<div class="section">
    <h2>When to Reset Caches</h2>
    <ul>
        <li>After driver updates or shader changes that cause pipeline errors (delete <code>pipeline_cache_vk.bin</code>).</li>
        <li>After changing shader bindings/layouts; rebuild and validate.</li>
    </ul>
</div>

