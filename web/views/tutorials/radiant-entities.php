<?php
/**
 * Radiant Entity Definitions Guide
 */
$title = 'Radiant Entity Definitions Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-entities' => 'Radiant Entity Definitions'
];
?>

<h1>Radiant Entity Definitions Guide</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Entity definitions (<code>entities.def</code>) tell Radiant how to display, classify, and parameterize engine entities. Custom entities need entries here to expose keys and descriptions to mappers.</p>
</div>

<div class="section">
    <h2>Where to Place</h2>
    <ul>
        <li>Locate under your game’s Radiant game folder, e.g., <code>radiant/install/games/&lt;yourgame&gt;/entities.def</code>.</li>
        <li>Ensure the custom <code>.game</code> file points to this definition.</li>
    </ul>
</div>

<div class="section">
    <h2>Adding an Entity</h2>
    <div class="code-block">
        <pre><code>/*QUAKED func_door (0 .5 .8) ? START_OPEN CRUSHER
Example door entity.
-------- KEYS --------
speed    : float, opening speed
wait     : float, delay before closing
-------- SPAWNFLAGS --------
START_OPEN : starts open
CRUSHER    : damages blockers
*/</code></pre>
    </div>
    <p>Key parts:</p>
    <ul>
        <li><strong>Classname:</strong> matches engine classname (e.g., <code>func_door</code>).</li>
        <li><strong>Color/size:</strong> optional display hints.</li>
        <li><strong>Keys/Spawnflags:</strong> documented for mappers.</li>
    </ul>
</div>

<div class="section">
    <h2>Updating the Game File</h2>
    <p>In your <code>.game</code> file, ensure the entity definition is referenced:</p>
    <div class="code-block">
        <pre><code>{
    "name": "idTech3Fork",
    "enginepath": "../../release",
    "basegame": "base",
    "gamedir": "mymod",
    "entitydef": "entities.def",
    ...
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Shaderlist & Assets</h2>
    <ul>
        <li>Keep <code>shaderlist.txt</code> current so Radiant shows your materials.</li>
        <li>Ensure textures are under the paths Radiant sees (<code>textures/</code> in your game dir or pk3).</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>Missing entities in Radiant: check <code>entities.def</code> path and syntax.</li>
        <li>Wrong keys shown: update the definition block; restart Radiant.</li>
        <li>Missing shaders: confirm <code>shaderlist.txt</code> and asset paths.</li>
    </ul>
</div>

