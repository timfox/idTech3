<?php
/**
 * Clustered Lighting Overview (GL & VK)
 */
$title = 'Clustered Lighting Overview - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/clustered-lighting-overview' => 'Clustered Lighting Overview'
];
?>

<h1>Clustered Lighting Overview (GL & VK)</h1>

<div class="section">
    <h2>What Is Clustered Lighting?</h2>
    <p>Clustered lighting partitions the view frustum into a 3D grid of “clusters” and bins dynamic lights into those cells. Shaders then iterate only the lights affecting the current cluster, reducing overdraw and wasted light evaluations.</p>
</div>

<div class="section">
    <h2>Grid Parameters</h2>
    <ul>
        <li>Tiles: computed from viewport size (e.g., 16×16 pixels per tile).</li>
        <li>Depth slices: logarithmic distribution from <code>znear</code> to <code>zfar</code> (default 16 slices).</li>
    </ul>
</div>

<div class="section">
    <h2>OpenGL Path</h2>
    <ul>
        <li>Buffers: <code>lcHeaderBuffer</code> (cluster headers) and <code>lcIndexBuffer</code> (light indices).</li>
        <li>Binding: SSBOs bound to points 6 (headers) and 7 (indices).</li>
        <li>Enable: <code>/set r_clusteredLight 1</code>; requires SSBO support.</li>
    </ul>
</div>

<div class="section">
    <h2>Vulkan Path</h2>
    <ul>
        <li>Uses compute/graphics pipelines to consume clustered data; descriptor-backed buffers for headers/indices.</li>
        <li>Pipeline cache aids faster startup; see Vulkan pipeline cache tutorial.</li>
    </ul>
</div>

<div class="section">
    <h2>Limitations</h2>
    <ul>
        <li>Max lights per cluster capped (see <code>LC_MAX_LIGHTS_PER_CLUSTER</code>).</li>
        <li>Fallback: if SSBOs are unsupported (GL) or clustering is disabled, engine uses legacy per-surface lighting.</li>
    </ul>
</div>

<div class="section">
    <h2>Tips</h2>
    <ul>
        <li>Keep light counts reasonable; very dense scenes may hit the per-cluster cap.</li>
        <li>For GL, ensure driver supports SSBOs; otherwise clustering won’t engage.</li>
        <li>For VK, warm up the pipeline cache after shader changes.</li>
    </ul>
</div>

