<?php
/**
 * Radiant Shaderlist Maintenance
 */
$title = 'Radiant Shaderlist Maintenance - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-shaderlist' => 'Radiant Shaderlist Maintenance'
];
?>

<h1>Radiant Shaderlist Maintenance</h1>

<div class="section">
    <h2>What It Is</h2>
    <p><code>shaderlist.txt</code> tells Radiant which shader files to load. If a shader file is not listed, its materials won’t appear in the texture browser.</p>
</div>

<div class="section">
    <h2>Location</h2>
    <ul>
        <li>Place <code>shaderlist.txt</code> under your game’s <code>scripts/</code> directory (e.g., <code>release/base/scripts/shaderlist.txt</code> or your mod’s <code>scripts/</code>).</li>
    </ul>
</div>

<div class="section">
    <h2>Adding Shaders</h2>
    <div class="code-block">
        <pre><code># Each line is a shader file (without .shader extension)
common
textures
environment
my_new_shaders</code></pre>
    </div>
    <p>Ensure the corresponding <code>*.shader</code> files live in <code>scripts/</code>.</p>
</div>

<div class="section">
    <h2>Common Pitfalls</h2>
    <ul>
        <li>Forgetting to add new shader files to <code>shaderlist.txt</code> (materials invisible in Radiant).</li>
        <li>Typos in names (Radiant silently skips missing shader files).</li>
        <li>Multiple shaderlists in different pk3s: ensure the active one contains all needed entries.</li>
    </ul>
</div>

<div class="section">
    <h2>Workflow Tips</h2>
    <ul>
        <li>Keep one authoritative <code>shaderlist.txt</code> per game/mod.</li>
        <li>When adding shaders, update <code>shaderlist.txt</code> and restart Radiant to reload materials.</li>
        <li>During packaging, ensure the updated shaderlist ships alongside your shaders.</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>Shader not visible: check it’s listed in <code>shaderlist.txt</code> and the <code>.shader</code> file is under <code>scripts/</code>.</li>
        <li>Black/missing textures: verify texture paths and that the shader references correct image files.</li>
    </ul>
</div>

