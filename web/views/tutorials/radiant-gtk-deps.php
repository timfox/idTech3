<?php
/**
 * Radiant GTK Dependencies
 */
$title = 'Radiant GTK Dependencies - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-gtk-deps' => 'Radiant GTK Dependencies'
];
?>

<h1>Radiant GTK Dependencies</h1>

<div class="section">
    <h2>Required Libraries</h2>
    <ul>
        <li>GTK2 development headers: <code>libgtk2.0-dev</code></li>
        <li>gtkglext: <code>libgtkglext1-dev</code></li>
        <li>XML: <code>libxml2-dev</code></li>
        <li>Images: <code>libjpeg-dev</code>, <code>libpng-dev</code></li>
        <li>Core GL/X11: <code>libglib2.0-dev</code>, <code>libx11-dev</code>, <code>libxext-dev</code>, <code>libgl1-mesa-dev</code>, <code>libglu1-mesa-dev</code></li>
        <li>Compression/build tools: <code>zlib1g-dev</code>, <code>pkg-config</code>, <code>build-essential</code>, <code>subversion</code></li>
    </ul>
</div>

<div class="section">
    <h2>Install (Debian/Ubuntu)</h2>
    <div class="code-block">
        <pre><code>sudo apt-get update
sudo apt-get install -y \
  libgtk2.0-dev libgtkglext1-dev libxml2-dev libjpeg-dev libpng-dev \
  libglib2.0-dev libx11-dev libxext-dev libgl1-mesa-dev libglu1-mesa-dev \
  zlib1g-dev pkg-config build-essential subversion</code></pre>
    </div>
</div>

<div class="section">
    <h2>Virtualenv + SCons</h2>
    <div class="code-block">
        <pre><code>python3 -m venv .venv
. .venv/bin/activate
pip install scons</code></pre>
    </div>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li><strong>Missing headers:</strong> Reinstall the packages above.</li>
        <li><strong>Display/GL issues:</strong> Ensure working OpenGL drivers and X11 environment.</li>
    </ul>
</div>

