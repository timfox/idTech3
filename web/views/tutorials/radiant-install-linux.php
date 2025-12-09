<?php
/**
 * Radiant Install (Linux)
 */
$title = 'Radiant Install (Linux) - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-install-linux' => 'Radiant Install (Linux)'
];
?>

<h1>Radiant Install (Linux)</h1>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>GTK2 + gtkglext development packages.</li>
        <li>Build tools: <code>build-essential</code>, <code>pkg-config</code>, <code>subversion</code>.</li>
        <li>Python 3 and <code>scons</code> (can be in a virtualenv).</li>
    </ul>
</div>

<div class="section">
    <h2>Install Dependencies (Debian/Ubuntu)</h2>
    <div class="code-block">
        <pre><code>sudo apt-get update
sudo apt-get install -y \
  libgtk2.0-dev libgtkglext1-dev libxml2-dev libjpeg-dev libpng-dev \
  libglib2.0-dev libx11-dev libxext-dev libgl1-mesa-dev libglu1-mesa-dev \
  zlib1g-dev pkg-config build-essential subversion python3-venv</code></pre>
    </div>
</div>

<div class="section">
    <h2>Clone and Build</h2>
    <ol>
        <li>Clone the repo (or update the submodule) to <code>radiant/</code>.</li>
        <li>Create a venv and install <code>scons</code>: <code>python3 -m venv .venv && .venv/bin/pip install scons</code></li>
        <li>Build: <div class="code-block"><pre><code>cd radiant
../.venv/bin/scons target=radiant</code></pre></div></li>
    </ol>
</div>

<div class="section">
    <h2>First Launch</h2>
    <ul>
        <li>Run the built Radiant binary in <code>radiant/install/</code>.</li>
        <li>Select your custom game from the game list (see gamepack setup tutorial).</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li><strong>Missing GTK headers:</strong> Reinstall the dependency list above.</li>
        <li><strong>GL errors on start:</strong> Ensure a working OpenGL stack (Mesa/NVIDIA/AMD drivers).</li>
        <li><strong>Game not listed:</strong> Verify your <code>.game</code> file is under <code>install/games/</code> and JSON is valid.</li>
    </ul>
</div>

