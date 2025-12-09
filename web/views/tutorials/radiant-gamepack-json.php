<?php
/**
 * Radiant Gamepack JSON Guide
 */
$title = 'Radiant Gamepack JSON Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-gamepack-json' => 'Radiant Gamepack JSON Guide'
];
?>

<h1>Radiant Gamepack JSON Guide</h1>

<div class="section">
    <h2>Purpose</h2>
    <p>The <code>.game</code> JSON file tells Radiant where to find your engine, base assets, and mod content. Properly configuring it ensures shaders, textures, and entities load correctly.</p>
</div>

<div class="section">
    <h2>Example</h2>
    <div class="code-block">
        <pre><code>{
    "name": "idTech3Fork",
    "enginepath": "../../release",
    "basegame": "base",
    "basegamename": "idTech3 Fork",
    "gamedir": "mymod",
    "gamename": "MyMod",
    "shaderpath": "scripts",
    "entitydef": "entities.def",
    "archivetypes": ["pk3"],
    "prefix": ".",
    "version": "1.0"
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Field Notes</h2>
    <ul>
        <li><strong>enginepath:</strong> path to built assets/binaries (e.g., <code>release/</code>).</li>
        <li><strong>basegame:</strong> base data folder (usually <code>base</code>).</li>
        <li><strong>gamedir:</strong> your mod folder (e.g., <code>mymod</code>).</li>
        <li><strong>shaderpath:</strong> relative path to shaders (default <code>scripts</code>).</li>
        <li><strong>entitydef:</strong> entity definition file (e.g., <code>entities.def</code>).</li>
        <li><strong>archivetypes:</strong> archive types to load (e.g., <code>pk3</code>).</li>
    </ul>
</div>

<div class="section">
    <h2>Placement</h2>
    <p>Place the <code>.game</code> file under <code>radiant/install/games/</code>. Radiant scans this directory to list available games.</p>
</div>

<div class="section">
    <h2>Validation</h2>
    <ul>
        <li>Ensure JSON is valid (no trailing commas, proper quotes).</li>
        <li>Confirm paths are correct relative to the <code>.game</code> file.</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>Game not shown: check JSON validity and placement under <code>install/games/</code>.</li>
        <li>Missing shaders/textures: verify <code>enginepath</code>/<code>gamedir</code> and <code>shaderpath</code>.</li>
    </ul>
</div>

