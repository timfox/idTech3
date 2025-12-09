<?php
/**
 * Radiant Texture Troubleshooting
 */
$title = 'Radiant Texture Troubleshooting - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-texture-troubleshooting' => 'Radiant Texture Troubleshooting'
];
?>

<h1>Radiant Texture Troubleshooting</h1>

<div class="section">
    <h2>Symptoms</h2>
    <ul>
        <li>Textures show as black/missing in Radiant.</li>
        <li>Only default/common textures appear.</li>
    </ul>
</div>

<div class="section">
    <h2>Checklist</h2>
    <ul>
        <li><strong>Shaderlist:</strong> Ensure your shader file is listed in <code>shaderlist.txt</code>.</li>
        <li><strong>Shader file location:</strong> <code>scripts/&lt;file&gt;.shader</code> exists and is readable.</li>
        <li><strong>Texture paths:</strong> Image files (tga/jpg/png) exist under <code>textures/...</code> matching the shader’s path.</li>
        <li><strong>Game paths:</strong> <code>enginepath</code>/<code>gamedir</code> in your <code>.game</code> file point to the correct asset roots.</li>
        <li><strong>PK3 visibility:</strong> If using pk3s, ensure they are in the paths Radiant scans (base/mod folder).</li>
    </ul>
</div>

<div class="section">
    <h2>q3map2 Compile (Lighting)</h2>
    <p>For in-game lighting, run a light compile after saving the map:</p>
    <div class="code-block">
        <pre><code>q3map2 -meta -v yourmap.map
q3map2 -vis -v yourmap.map
q3map2 -light -fast -v yourmap.map</code></pre>
    </div>
    <p>Missing lightmaps in-game can present as black surfaces.</p>
</div>

<div class="section">
    <h2>Troubleshooting Steps</h2>
    <ol>
        <li>Restart Radiant after updating <code>shaderlist.txt</code> or adding shaders.</li>
        <li>Verify the shader block names match the texture names you expect.</li>
        <li>Inspect paths inside the shader for correct <code>textures/</code> subdirectories.</li>
        <li>Check console output in Radiant for missing file warnings.</li>
    </ol>
</div>

<div class="section">
    <h2>Tips</h2>
    <ul>
        <li>Keep textures uncompressed (tga/png/jpg) with power-of-two dimensions for compatibility.</li>
        <li>Group related shaders in a single <code>.shader</code> file and keep names consistent.</li>
    </ul>
</div>

