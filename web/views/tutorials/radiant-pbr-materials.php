<?php
/**
 * Radiant PBR / Enhanced Materials Guide
 */
$title = 'Radiant PBR / Enhanced Materials - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/radiant-pbr-materials' => 'Radiant PBR / Enhanced Materials'
];
?>

<h1>Radiant PBR / Enhanced Materials</h1>

<div class="section">
    <h2>Overview</h2>
    <p>This guide explains how to author and preview enhanced/PBR materials for engines that extend id Tech 3 rendering (e.g., Vulkan PBR). Radiant itself is a brush editor; you must still provide shader definitions, textures, and shaderlist entries so materials show up correctly.</p>
</div>

<div class="section">
    <h2>Typical PBR Texture Set</h2>
    <ul>
        <li><strong>Albedo/base color</strong> (sRGB)</li>
        <li><strong>Normal</strong> (tangent-space, linear)</li>
        <li><strong>Roughness / Gloss</strong> (linear; sometimes packed with metal/AO)</li>
        <li><strong>Metalness</strong> (if metallic workflow)</li>
        <li><strong>Ambient Occlusion</strong> (optional)</li>
        <li><strong>Emission</strong> (optional, sRGB)</li>
    </ul>
</div>

<div class="section">
    <h2>Example Shader (engine-extended PBR)</h2>
    <div class="code-block">
        <pre><code>// scripts/pbr_example.shader
textures/pbr/metal_panel
{
    qer_editorimage textures/pbr/metal_panel_ed.tga

    // Base color
    {
        map textures/pbr/metal_panel_albedo.tga
        rgbGen identity
    }

    // Normal map
    {
        map textures/pbr/metal_panel_normal.tga
        normalMap
        rgbGen identity
    }

    // PBR params (engine-specific keywords)
    {
        map textures/pbr/metal_panel_mra.tga // Metal/Rough/AO packed
        metalRoughAOMap
    }

    // Optional emission
    {
        map textures/pbr/metal_panel_emissive.tga
        rgbGen identity
        blendFunc GL_ONE GL_ONE
    }
}</code></pre>
    </div>
    <p><strong>Note:</strong> The exact PBR keywords (<code>normalMap</code>, <code>metalRoughAOMap</code>, etc.) depend on your engine’s shader parser. Adjust to match your renderer’s conventions.</p>
</div>

<div class="section">
    <h2>Radiant Setup</h2>
    <ul>
        <li>Add the shader file name to <code>scripts/shaderlist.txt</code> (e.g., <code>pbr_example</code>).</li>
        <li>Place shader files in <code>scripts/</code>; textures under <code>textures/...</code>.</li>
        <li>Include an <code>qer_editorimage</code> for browse icons in Radiant.</li>
        <li>Restart Radiant after updating shaderlist or adding shaders.</li>
    </ul>
</div>

<div class="section">
    <h2>Authoring Tips</h2>
    <ul>
        <li>Keep consistent naming: <code>_albedo</code>, <code>_normal</code>, <code>_mra</code> (Metal/Rough/AO packed), <code>_emissive</code>.</li>
        <li>Use linear formats for data maps (normal/roughness/metal/ao), sRGB for color/emissive.</li>
        <li>If your pipeline doesn’t support packed maps, supply separate roughness/metal/AO textures and adjust the shader.</li>
    </ul>
</div>

<div class="section">
    <h2>In-Engine Considerations</h2>
    <ul>
        <li>Ensure your renderer binds the extra PBR textures (normal, rough/metal/AO) and understands the shader keywords.</li>
        <li>Lightmaps: if your engine uses lightmaps with PBR, decide whether to modulate or replace with dynamic lighting.</li>
        <li>Fallbacks: provide a simple diffuse-only stage for tools/pipelines that lack PBR support.</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    <ul>
        <li>Material not visible in Radiant: verify <code>shaderlist.txt</code> entry and restart Radiant.</li>
        <li>Black/missing textures: check paths and file formats; ensure correct case-sensitive names.</li>
        <li>Normal maps look inverted: confirm channel convention (OpenGL vs DirectX) and swizzle if needed.</li>
    </ul>
</div>

