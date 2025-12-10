<?php
/**
 * Radiant Material Blending Guide
 */
$title = 'Radiant Material Blending - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-material-blending' => 'Radiant Material Blending'
];
?>

<h1>Radiant Material Blending</h1>

<div class="section">
    <h2>What Is Material Blending?</h2>
    <p>Material blending lets you smoothly transition between two (or more) textures on a surface, e.g., grass-to-dirt or sand-to-rock. In id Tech 3 this is achieved via multi-stage shaders with blend maps/alpha and multitexture.</p>
</div>

<div class="section">
    <h2>Authoring a Blend Shader</h2>
    <div class="code-block">
        <pre><code>// scripts/terrain_blend.shader
textures/terrain/grass_dirt
{
    qer_editorimage textures/terrain/grass_dirt_ed.tga

    // Base layer (dirt)
    {
        map textures/terrain/dirt.tga
        rgbGen identity
    }

    // Blend layer (grass) using vertex alpha / blend map
    {
        map textures/terrain/grass.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        alphaGen vertex
    }
}</code></pre>
    </div>
    <ul>
        <li><strong>qer_editorimage:</strong> What Radiant shows in the texture browser.</li>
        <li><strong>Base pass:</strong> First map renders the underlying material.</li>
        <li><strong>Blend pass:</strong> Second map uses <code>blendFunc</code> with alpha (<code>alphaGen vertex</code> if using vertex colors/alphas).</li>
    </ul>
</div>

<div class="section">
    <h2>Using Blend Maps</h2>
    <p>You can drive blending with vertex alpha or an external blend map:</p>
    <div class="code-block">
        <pre><code>// Blend map example
    {
        map textures/terrain/grass.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        alphaGen const 1
        tcGen base
        tcMod scale 1 1
        alphaGen const 1
        // Use an alpha map for blending
        alphaFunc GE128
        map textures/terrain/grass_blend_alpha.tga
    }</code></pre>
    </div>
    <ul>
        <li>Common approach: paint vertex alpha in Radiant (terrain/mesh) or use a separate alpha map as a mask.</li>
        <li>For terrain, consider q3map2 terrain blending (alphaMod/vertex alpha) if using patches/meshes.</li>
    </ul>
</div>

<div class="section">
    <h2>Radiant Setup</h2>
    <ul>
        <li>Add the shader file name to <code>scripts/shaderlist.txt</code> (e.g., <code>terrain_blend</code>).</li>
        <li>Place <code>terrain_blend.shader</code> in <code>scripts/</code>.</li>
        <li>Place textures under <code>textures/terrain/</code> (or your path), and an editor image for browsing.</li>
        <li>Restart Radiant after adding shaders/shaderlist entries.</li>
    </ul>
</div>

<div class="section">
    <h2>In-Engine Considerations</h2>
    <ul>
        <li>Ensure multitexture is enabled (standard in id Tech 3 paths).</li>
        <li>Overdraw: Blend passes add cost; keep layers minimal.</li>
        <li>Lightmaps: If using lightmaps, include them in the base pass or a separate pass as needed.</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>Shader not visible in Radiant: ensure <code>shaderlist.txt</code> includes the shader file and restart Radiant.</li>
        <li>Black/missing textures: verify texture paths and file formats.</li>
        <li>Blend not showing: confirm alpha source (vertex alpha or alpha map) and <code>blendFunc</code> settings.</li>
    </ul>
</div>

