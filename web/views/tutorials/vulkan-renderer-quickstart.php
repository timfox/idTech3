<?php
/**
 * Vulkan Renderer Quick Start
 */
$title = 'Vulkan Renderer Quick Start - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/vulkan-renderer-quickstart' => 'Vulkan Renderer Quick Start'
];
?>

<h1>Vulkan Renderer Quick Start</h1>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>Vulkan SDK installed; validation layers available.</li>
        <li>Driver supports required extensions (swapchain, descriptor indexing if used).</li>
        <li>Shaders compiled with <code>glslangValidator</code> and validated with <code>spirv-val</code>.</li>
    </ul>
</div>

<div class="section">
    <h2>Key Files</h2>
    <ul>
        <li><code>src/renderervk/vk.c</code> – device setup, pipeline cache, queues.</li>
        <li><code>src/renderervk/tr_*.c</code> – render passes, materials, scene submission.</li>
        <li><code>src/renderervk/shaders/</code> – GLSL sources.</li>
    </ul>
</div>

<div class="section">
    <h2>Adding a Pass</h2>
    <ol>
        <li>Create/compile shaders to SPIR-V.</li>
        <li>Add descriptor set layouts and pipeline layout entries as needed.</li>
        <li>Create a pipeline (graphics or compute) using the shared pipeline cache.</li>
        <li>Add a render pass/subpass or use dynamic rendering; wire framebuffers/attachments.</li>
        <li>Record commands: bind descriptors, pipelines, vertex/index buffers, and draw/dispatch.</li>
    </ol>
</div>

<div class="section">
    <h2>Descriptors & Resources</h2>
    <ul>
        <li>Reuse descriptor pools where possible; avoid per-frame pool creation.</li>
        <li>Prefer dynamic UBOs/push constants for small per-draw data.</li>
        <li>Stage uploads through persistent staging buffers; use proper pipeline barriers.</li>
    </ul>
</div>

<div class="section">
    <h2>Validation & Debugging</h2>
    <ul>
        <li>Enable validation layers; fix reported issues before release.</li>
        <li>Use RenderDoc for capture/playback; validate barriers and descriptor bindings.</li>
        <li>Check pipeline layouts match shader descriptor expectations.</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Tips</h2>
    <ul>
        <li>Warm the pipeline cache (see pipeline cache usage guide).</li>
        <li>Batch state changes; minimize pipeline swaps and descriptor set rebinds.</li>
        <li>Use accurate stage masks/access masks to avoid over-synchronization.</li>
    </ul>
</div>

