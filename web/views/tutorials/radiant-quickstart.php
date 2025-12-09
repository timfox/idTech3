<?php
/**
 * Radiant Quick Start
 */
$title = 'Radiant Quick Start - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-quickstart' => 'Radiant Quick Start'
];
?>

<h1>Radiant Quick Start</h1>

<div class="section">
    <h2>Steps</h2>
    <ol>
        <li>Install GTK2/gtkglext and build tools (see GTK dependencies tutorial).</li>
        <li>Create a venv and install <code>scons</code>.</li>
        <li>Build Radiant from <code>radiant/</code>: <div class="code-block"><pre><code>../.venv/bin/scons target=radiant</code></pre></div></li>
        <li>Add a custom <code>.game</code> file pointing to your engine assets (see gamepack setup).</li>
        <li>Launch Radiant from <code>radiant/install/</code> and pick your game on first run.</li>
        <li>Ensure <code>shaderlist.txt</code> and <code>entities.def</code> are configured for your content.</li>
    </ol>
</div>

<div class="section">
    <h2>Paths to Verify</h2>
    <ul>
        <li><code>enginepath</code> in your <code>.game</code> file points to <code>release/</code> (or your build artifacts).</li>
        <li><code>gamedir</code> is your mod folder (e.g., <code>mymod</code>).</li>
        <li><code>shaderlist.txt</code> includes all shaders you want visible in Radiant.</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>Missing shaders: check <code>shaderlist.txt</code> and that shader files are under <code>scripts/</code>.</li>
        <li>Black textures: verify texture paths/pk3s and <code>enginepath</code>/<code>gamedir</code>.</li>
        <li>Game not listed: ensure the <code>.game</code> file is in <code>install/games/</code> and JSON is valid.</li>
    </ul>
</div>

