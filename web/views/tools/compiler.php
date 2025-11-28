<?php
/**
 * Map Compiler Documentation
 */
$title = 'Map Compiler - id Tech 3 Documentation';
$breadcrumbs = [
    '/tools' => 'Tools',
    '/tools/compiler' => 'Map Compiler'
];
?>

<h1>Map Compiler (q3map2)</h1>

<div class="section">
    <h2>Overview</h2>
    <p><strong>q3map2</strong> is the map compiler for id Tech 3 engine. It converts .map files created in level editors like Q3Radiant into playable .bsp files that can be loaded by the game engine.</p>
    
    <div class="feature-list">
        <h3>Compilation Stages</h3>
        <ul>
            <li><strong>BSP:</strong> Convert brushes to BSP tree geometry</li>
            <li><strong>VIS:</strong> Calculate visibility information</li>
            <li><strong>LIGHT:</strong> Calculate lighting and shadows</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Basic Usage</h2>
    
    <h3>Standard Compilation</h3>
    <div class="code-block">
        <pre><code># Complete compilation process
q3map2 -bsp mymap.map
q3map2 -vis mymap.bsp  
q3map2 -light mymap.bsp</code></pre>
    </div>
    
    <h3>Fast Development Compilation</h3>
    <div class="code-block">
        <pre><code># Quick compilation for testing
q3map2 -bsp -meta mymap.map
q3map2 -vis -fast mymap.bsp
q3map2 -light -fast mymap.bsp</code></pre>
    </div>
</div>

<div class="section">
    <h2>BSP Stage Options</h2>
    
    <div class="code-block">
        <pre><code># Basic BSP compilation
q3map2 -bsp mymap.map

# BSP with optimizations
q3map2 -bsp -meta -verboseentities mymap.map

# BSP with custom settings
q3map2 -bsp -meta -samplesize 8 -np 4 mymap.map</code></pre>
    </div>
    
    <h3>Common BSP Flags</h3>
    <ul>
        <li><strong>-meta:</strong> Optimize brush merging</li>
        <li><strong>-verboseentities:</strong> Show detailed entity processing</li>
        <li><strong>-samplesize N:</strong> Set lightmap resolution</li>
        <li><strong>-np N:</strong> Use N processor cores</li>
    </ul>
</div>

<div class="section">
    <h2>VIS Stage Options</h2>
    
    <div class="code-block">
        <pre><code># Full VIS calculation (slow but optimal)
q3map2 -vis mymap.bsp

# Fast VIS for development
q3map2 -vis -fast mymap.bsp

# VIS with saved portal file
q3map2 -vis -saveprt mymap.bsp</code></pre>
    </div>
    
    <h3>VIS Optimization</h3>
    <ul>
        <li><strong>-fast:</strong> Quick VIS calculation</li>
        <li><strong>-saveprt:</strong> Save portal file for debugging</li>
        <li><strong>-nosort:</strong> Don't sort portals (faster)</li>
    </ul>
</div>

<div class="section">
    <h2>Light Stage Options</h2>
    
    <div class="code-block">
        <pre><code># Basic lighting
q3map2 -light mymap.bsp

# Fast lighting for development  
q3map2 -light -fast mymap.bsp

# High-quality lighting with bounces
q3map2 -light -bounce 2 -dirty -filter mymap.bsp

# Modern lighting features
q3map2 -light -bounce 8 -bouncegrid -dirty -filter -patchshadows mymap.bsp</code></pre>
    </div>
    
    <h3>Lighting Quality Options</h3>
    <ul>
        <li><strong>-fast:</strong> Quick lighting for testing</li>
        <li><strong>-bounce N:</strong> Light bouncing iterations</li>
        <li><strong>-dirty:</strong> Ambient occlusion</li>
        <li><strong>-filter:</strong> Smooth lightmap filtering</li>
        <li><strong>-patchshadows:</strong> Soft shadows</li>
    </ul>
</div>

<div class="section">
    <h2>Advanced Features</h2>
    
    <h3>HDR Lighting</h3>
    <div class="code-block">
        <pre><code># Compile with HDR support
q3map2 -light -bounce 4 -dirty -filter -gamma 2.2 -compensate 4.0 mymap.bsp</code></pre>
    </div>
    
    <h3>Performance Optimization</h3>
    <div class="code-block">
        <pre><code># Multi-threaded compilation
q3map2 -bsp -meta -np 8 mymap.map
q3map2 -vis -fast -np 8 mymap.bsp  
q3map2 -light -fast -bounce 2 -np 8 mymap.bsp</code></pre>
    </div>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common Issues</h3>
    
    <h4>Leak Errors</h4>
    <p>Map has holes allowing void to reach inside:</p>
    <ul>
        <li>Check for gaps in brush geometry</li>
        <li>Ensure all areas are properly sealed</li>
        <li>Use the .lin file to locate leaks</li>
    </ul>
    
    <h4>Light Compilation Failures</h4>
    <ul>
        <li>Reduce lightmap resolution with -samplesize</li>
        <li>Check for invalid light entities</li>
        <li>Verify surface shader properties</li>
    </ul>
    
    <h4>Long Compilation Times</h4>
    <ul>
        <li>Use -fast flags during development</li>
        <li>Reduce bounce count for testing</li>
        <li>Enable multi-threading with -np</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="tools/radiant">Q3Radiant Level Editor</a></li>
<li><a href="development/map-making">Map Making Guide</a></li>
<li><a href="tools/asset-tools">Asset Creation Tools</a></li>
    </ul>
</div> 