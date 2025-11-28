<?php
/**
 * Q3Radiant Level Editor Documentation
 */
$title = 'Q3Radiant - id Tech 3 Documentation';
$breadcrumbs = [
    '/tools' => 'Tools',
    '/tools/radiant' => 'Q3Radiant'
];
?>

<h1>Q3Radiant Level Editor</h1>

<div class="section">
    <h2>Overview</h2>
    <p><strong>Q3Radiant</strong> is the official level editor for Quake III Arena and the id Tech 3 engine. It's a powerful tool for creating 3D levels, designing maps, and placing entities. Modern alternatives include NetRadiant and GtkRadiant.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li>Real-time 3D map editing</li>
            <li>Brush-based geometry creation</li>
            <li>Texture application and alignment</li>
            <li>Entity placement and configuration</li>
            <li>Lighting preview and setup</li>
            <li>Integrated shader editor</li>
            <li>Auto-compilation support</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Installation</h2>
    
    <h3>Original Q3Radiant</h3>
    <p>Available with the Quake III Arena editing tools or as part of the id Software development kit.</p>
    
    <h3>Modern Alternatives</h3>
    <ul>
        <li><strong>NetRadiant:</strong> Cross-platform modern editor with improved features</li>
        <li><strong>GtkRadiant:</strong> Updated version with GTK interface</li>
        <li><strong>J.A.C.K:</strong> Commercial editor with advanced features</li>
    </ul>
</div>

<div class="section">
    <h2>Basic Configuration</h2>
    
    <p>Configure Radiant for your id Tech 3 project:</p>
    <div class="code-block">
        <pre><code>// Basic Radiant configuration
// File: q3radiant.ini or preferences

[GameSetup]
BasePath=/path/to/quake3
GamePath=baseq3
MapPath=maps
EntityDef=scripts/entities.def

[Display]
GridSize=8
TextureScale=0.5
ViewportLayout=4way</code></pre>
    </div>
</div>

<div class="section">
    <h2>Interface Overview</h2>
    
    <h3>Main Views</h3>
    <ul>
        <li><strong>3D View:</strong> Real-time 3D preview of your map</li>
        <li><strong>Top View:</strong> Bird's eye view for layout planning</li>
        <li><strong>Front/Side Views:</strong> Orthogonal views for precise alignment</li>
        <li><strong>Texture Browser:</strong> Browse and select textures</li>
    </ul>
    
    <h3>Essential Tools</h3>
    <ul>
        <li><strong>Selection Tool:</strong> Select and manipulate brushes</li>
        <li><strong>Brush Tool:</strong> Create new geometry</li>
        <li><strong>Entity Tool:</strong> Place game entities</li>
        <li><strong>Surface Inspector:</strong> Adjust texture properties</li>
    </ul>
</div>

<div class="section">
    <h2>Basic Workflow</h2>
    
    <div class="step">
        <h3>1. Create Basic Geometry</h3>
        <p>Start with simple brushes to define your level layout:</p>
        <ul>
            <li>Create room structures with wall brushes</li>
            <li>Add floor and ceiling brushes</li>
            <li>Use the grid for alignment</li>
        </ul>
    </div>
    
    <div class="step">
        <h3>2. Apply Textures</h3>
        <p>Select appropriate textures for different surfaces:</p>
        <ul>
            <li>Wall textures for vertical surfaces</li>
            <li>Floor textures for horizontal surfaces</li>
            <li>Special textures for liquids and effects</li>
        </ul>
    </div>
    
    <div class="step">
        <h3>3. Add Entities</h3>
        <p>Place game entities to make your map functional:</p>
        <ul>
            <li><strong>info_player_start:</strong> Player spawn point</li>
            <li><strong>light:</strong> Light sources</li>
            <li><strong>weapon_*:</strong> Weapon pickups</li>
            <li><strong>item_*:</strong> Item pickups</li>
        </ul>
    </div>
    
    <div class="step">
        <h3>4. Configure Advanced Features</h3>
        <p>Set up advanced rendering features:</p>
        <div class="code-block">
            <pre><code>// Worldspawn entity properties for modern features
"raymarchSky" "1"          // Enable raymarching sky
"globalIllumination" "1"   // Enable GI
"hdrLighting" "1"          // Enable HDR</code></pre>
        </div>
    </div>
</div>

<div class="section">
    <h2>Best Practices</h2>
    
    <h3>Geometry Guidelines</h3>
    <ul>
        <li>Keep brush geometry simple and aligned to grid</li>
        <li>Avoid unnecessarily complex brushes</li>
        <li>Use proper scaling for player movement</li>
        <li>Ensure all areas are properly sealed</li>
    </ul>
    
    <h3>Performance Optimization</h3>
    <ul>
        <li>Use vis-blocking to improve performance</li>
        <li>Optimize brush count in visible areas</li>
        <li>Use detail brushes for small decorative elements</li>
        <li>Plan lighting carefully to avoid overdraw</li>
    </ul>
</div>

<div class="section">
    <h2>Compilation Process</h2>
    
    <p>After creating your map, compile it for use in the engine:</p>
    <div class="code-block">
        <pre><code># Basic compilation workflow
q3map2 -bsp mymap.map
q3map2 -vis mymap.bsp
q3map2 -light -fast mymap.bsp

# Advanced compilation with modern features
q3map2 -bsp -meta -verboseentities mymap.map
q3map2 -vis -saveprt mymap.bsp
q3map2 -light -fast -bounce 2 -dirty mymap.bsp</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="development/map-making">Complete Map Making Guide</a></li>
<li><a href="tools/compiler">Map Compiler Documentation</a></li>
<li><a href="tools/asset-tools">Asset Creation Tools</a></li>
<li><a href="rendering/vulkan">Vulkan Renderer Features</a></li>
    </ul>
</div> 