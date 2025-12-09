<?php
/**
 * Legacy GL Clustered Lighting SSBO Fix
 */
$title = 'GL Clustered Lighting SSBO Binding - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/gl-clustered-ssbo' => 'GL Clustered Lighting SSBO Binding'
];
?>

<h1>GL Clustered Lighting SSBO Binding</h1>

<div class="section">
    <h2>What Changed</h2>
    <p>The legacy OpenGL clustered lighting path now binds light cluster buffers as shader storage buffer objects (SSBOs) with correct binding points.</p>
</div>

<div class="section">
    <h2>Why It Matters</h2>
    <p>Without proper SSBO binding, clustered light data would not reach the GLSL shaders, causing missing or incorrect lighting in the GL renderer.</p>
</div>

<div class="section">
    <h2>Details</h2>
    <ul>
        <li><code>lcHeaderBuffer</code> and <code>lcIndexBuffer</code> are now bound to <code>GL_SHADER_STORAGE_BUFFER</code>.</li>
        <li><code>glBindBufferBase</code> binds them to binding points 6 (headers) and 7 (indices).</li>
        <li>The function pointer for <code>glBindBufferBase</code> is resolved at runtime via <code>glXGetProcAddressARB</code>.</li>
    </ul>
</div>

<div class="section">
    <h2>How to Use</h2>
    <ol>
        <li>Enable clustered lighting in GL: <div class="code-block"><pre><code>/set r_clusteredLight 1</code></pre></div></li>
        <li>Ensure your GL driver supports SSBOs; otherwise the code falls back to the legacy per-surface dlight path.</li>
        <li>No additional configuration is required; buffers are bound automatically each frame.</li>
    </ol>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>If your driver lacks SSBO support, clustered lighting will not engage; use the default lighting path.</li>
        <li>Validate with GL debug output to confirm SSBO binding if you suspect issues.</li>
    </ul>
</div>
<?php
/**
 * Legacy GL Clustered Lighting SSBO Fix
 */
$title = 'GL Clustered Lighting SSBO Binding - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/gl-clustered-ssbo' => 'GL Clustered Lighting SSBO Binding'
];
?>

<h1>GL Clustered Lighting SSBO Binding</h1>

<div class="section">
    <h2>What Changed</h2>
    <p>The legacy OpenGL clustered lighting path now binds light cluster buffers as shader storage buffer objects (SSBOs) with correct binding points.</p>
</div>

<div class="section">
    <h2>Why It Matters</h2>
    <p>Without proper SSBO binding, clustered light data would not reach the GLSL shaders, causing missing or incorrect lighting in the GL renderer.</p>
</div>

<div class="section">
    <h2>Details</h2>
    <ul>
        <li><code>lcHeaderBuffer</code> and <code>lcIndexBuffer</code> are now bound to <code>GL_SHADER_STORAGE_BUFFER</code>.</li>
        <li><code>glBindBufferBase</code> binds them to binding points 6 (headers) and 7 (indices).</li>
        <li>The function pointer for <code>glBindBufferBase</code> is resolved at runtime via <code>glXGetProcAddressARB</code>.</li>
    </ul>
</div>

<div class="section">
    <h2>How to Use</h2>
    <ol>
        <li>Enable clustered lighting in GL: <div class="code-block"><pre><code>/set r_clusteredLight 1</code></pre></div></li>
        <li>Ensure your GL driver supports SSBOs; otherwise the code falls back to the legacy per-surface dlight path.</li>
        <li>No additional configuration is required; buffers are bound automatically each frame.</li>
    </ol>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>If your driver lacks SSBO support, clustered lighting will not engage; use the default lighting path.</li>
        <li>Validate with GL debug output to confirm SSBO binding if you suspect issues.</li>
    </ul>
</div>

