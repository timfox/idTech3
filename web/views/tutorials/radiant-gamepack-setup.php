<?php
/**
 * Radiant Gamepack Setup for id Tech 3 (timfox/idTech3Radiant)
 */
$title = 'Radiant Gamepack Setup - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-gamepack-setup' => 'Radiant Gamepack Setup'
];
?>

<h1>Radiant Gamepack Setup (idTech3Radiant)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Configure GtkRadiant (timfox/idTech3Radiant.git) to target your engine assets (e.g., <code>release/</code> and <code>mymod/</code>). This enables correct shader paths, textures, and entities when mapping.</p>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>Radiant repo: <code>https://github.com/timfox/idTech3Radiant.git</code> (submodule in <code>radiant/</code>)</li>
        <li>Engine built; assets available under <code>release/</code> (and optionally <code>mymod/</code>)</li>
        <li>GTK2/gtkglext toolchain installed (Ubuntu/Debian):<br>
            <code>sudo apt-get install -y libgtk2.0-dev libgtkglext1-dev libxml2-dev libjpeg-dev libpng-dev libglib2.0-dev libx11-dev libxext-dev libgl1-mesa-dev libglu1-mesa-dev zlib1g-dev pkg-config build-essential subversion</code>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Create a Custom Game File</h2>
    <p>Add a game descriptor under <code>radiant/install/games/</code>, e.g., <code>idtech3-fork.game</code>:</p>
    <div class="code-block">
        <pre><code>{
    "name": "idTech3Fork",
    "enginepath": "../../release",
    "basegame": "base",
    "basegamename": "idTech3 Fork",
    "gamedir": "mymod",
    "gamename": "MyMod",
    "shaderpath": "scripts",
    "archivetypes": ["pk3"],
    "prefix": ".",
    "version": "1.0"
}</code></pre>
    </div>
    <p>Adjust <code>enginepath</code> to where your binaries/assets live; set <code>gamedir</code> to your mod folder.</p>
    <p>On first launch, select this game from the Radiant game list so it uses the correct paths.</p>
</div>

<div class="section">
    <h2>Shaderlist and Textures</h2>
    <ul>
        <li>Ensure <code>release/mymod/scripts/shaderlist.txt</code> contains your shaders.</li>
        <li>Point Radiant to textures under <code>release/mymod/textures/</code> (or shared <code>release/base/textures/</code>).</li>
        <li>If needed, symlink or copy shader/script folders into <code>radiant/install/base/</code> for quick testing.</li>
    </ul>
</div>

<div class="section">
    <h2>Entities Definition</h2>
    <p>Update or add an <code>entities.def</code> for your mod in <code>radiant/install/games/idtech3-fork.game</code> or referenced path. Include custom entities (vehicles, puzzles, etc.).</p>
</div>

<div class="section">
    <h2>Build Radiant</h2>
    <ol>
        <li>Install dependencies (GTK2/gtkglext etc.).</li>
        <li>From <code>radiant/</code>: <div class="code-block"><pre><code>../.venv/bin/scons target=radiant</code></pre></div></li>
        <li>Launch Radiant and select your custom game (idTech3Fork) on first run.</li>
    </ol>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>If shaders appear missing: verify <code>shaderlist.txt</code> and that shader files are under <code>scripts/</code>.</li>
        <li>If textures are black: confirm <code>enginepath</code>/<code>gamedir</code> paths and that pk3s are readable.</li>
        <li>If Radiant doesn’t list your game: check <code>.game</code> file placement and JSON syntax.</li>
        <li>If GTK/gtkglext are missing: install the packages listed above (GTK2 toolchain).</li>
    </ul>
</div>

