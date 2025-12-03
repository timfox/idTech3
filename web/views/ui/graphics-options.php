<?php
/**
 * Graphics Options UI Changes Documentation
 */
$title = 'Graphics Options UI Changes - id Tech 3 Documentation';
$breadcrumbs = [
    '/ui' => 'UI',
    '/ui/graphics-options' => 'Graphics Options UI Changes'
];
?>

<div class="content-section">
    <h1>Graphics Options UI Changes</h1>
    
    <blockquote>
        <strong>Implementation Summary:</strong> This document describes the new rendering features and UI enhancements added to the Graphics Options menu, including ray tracing, DLSS, mesh shaders, and more.
    </blockquote>

    <div class="section">
        <h2>What Was Added</h2>
        <p>The Graphics Options menu has been significantly expanded with <strong>26 new menu items</strong> covering modern rendering features:</p>
        
        <h3>1. Ray Tracing Options (11 items)</h3>
        <ul>
            <li><strong>Ray Tracing:</strong> Enable/Disable</li>
            <li><strong>RT Samples:</strong> 1-8</li>
            <li><strong>RT Max Depth:</strong> 1-8</li>
            <li><strong>RT Temporal:</strong> Enable/Disable</li>
            <li><strong>RT Temp Alpha:</strong> 0-10</li>
            <li><strong>RT Denoise:</strong> Enable/Disable</li>
            <li><strong>Denoise Mode:</strong> SVGF/ReLAX</li>
            <li><strong>Denoise Iter:</strong> 1-8</li>
            <li><strong>RT Global Illum:</strong> Enable/Disable</li>
            <li><strong>GI Bounces:</strong> 1-8</li>
            <li><strong>GI Intensity:</strong> 0-20</li>
        </ul>

        <h3>2. DLSS Options (3 items)</h3>
        <ul>
            <li><strong>DLSS:</strong> Enable/Disable</li>
            <li><strong>DLSS Quality:</strong> Performance/Balanced/Quality/Ultra Quality</li>
            <li><strong>DLSS Sharpen:</strong> 0-10</li>
        </ul>

        <h3>3. Compute Post-Processing (1 item)</h3>
        <ul>
            <li><strong>Compute PostProc:</strong> Enable/Disable</li>
        </ul>

        <h3>4. Mesh Shaders (2 items)</h3>
        <ul>
            <li><strong>Mesh Shaders:</strong> Enable/Disable</li>
            <li><strong>Meshlet Size:</strong> 1-4</li>
        </ul>

        <h3>5. Virtual Texturing (3 items)</h3>
        <ul>
            <li><strong>Virtual Textures:</strong> Enable/Disable</li>
            <li><strong>VT Page Size:</strong> 2-8</li>
            <li><strong>VT Cache Size:</strong> 1-16</li>
        </ul>

        <h3>6. Advanced Materials (3 items)</h3>
        <ul>
            <li><strong>Clearcoat:</strong> Enable/Disable</li>
            <li><strong>Material Aniso:</strong> Enable/Disable</li>
            <li><strong>Subsurface Scat:</strong> Enable/Disable</li>
        </ul>

        <h3>7. GPU Particles (3 items)</h3>
        <ul>
            <li><strong>GPU Particles:</strong> Enable/Disable</li>
            <li><strong>Max Particles:</strong> 1-50</li>
            <li><strong>Particle Culling:</strong> Enable/Disable</li>
        </ul>

        <div class="note">
            <strong>Total:</strong> 26 new menu items added to the Graphics Options menu.
        </div>
    </div>

    <div class="section">
        <h2>Scrolling Implementation</h2>
        <p>To accommodate the expanded menu, vertical scrolling has been implemented:</p>
        
        <ul>
            <li><strong>Mouse Wheel Scrolling:</strong> Scroll up/down with mouse wheel</li>
            <li><strong>Visual Scrollbar:</strong> Scrollbar appears on the right side when content exceeds visible area</li>
            <li><strong>Smooth Scrolling:</strong> 2 items per wheel tick for precise navigation</li>
            <li><strong>Keyboard Navigation:</strong> All items remain accessible via arrow keys</li>
        </ul>

        <h3>How It Works</h3>
        <p>The scrolling system tracks the current scroll position and adjusts the visible menu items accordingly. The scrollbar provides visual feedback about the current position within the menu.</p>
    </div>

    <div class="section">
        <h2>File Locations</h2>
        
        <h3>Source Files</h3>
        <ul>
            <li><strong>Source:</strong> <code>mymod/gamesrc/ui/ui_video.c</code></li>
            <li><strong>Compiled:</strong> <code>mymod/vm/uix86_64.so</code></li>
        </ul>

        <h3>Build System</h3>
        <p>The UI module is built using the Makefile in <code>mymod/gamesrc/</code>. After making changes to the source, rebuild the module:</p>
        <div class="code-block">
            <pre><code>cd mymod/gamesrc
make</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>How to See the Changes</h2>
        
        <h3>Step 1: Verify Game Settings</h3>
        <p>Open console (<code>~</code> key) and check:</p>
        <div class="code-block">
            <pre><code>/vm_ui</code></pre>
        </div>
        <p><strong>Must show <code>0</code></strong> (for native .so loading, not QVM)</p>
        
        <div class="code-block">
            <pre><code>/fs_game</code></pre>
        </div>
        <p><strong>Must show <code>mymod</code></p>

        <h3>Step 2: Restart Game</h3>
        <ol>
            <li><strong>Completely exit</strong> the game (not just minimize)</li>
            <li>Restart the game</li>
            <li>Load the <code>mymod</code> mod</li>
            <li>Go to: <strong>System Setup → Graphics</strong></li>
        </ol>

        <h3>Step 3: If Still Not Working</h3>
        <p>Try reloading UI:</p>
        <div class="code-block">
            <pre><code>/vid_restart</code></pre>
        </div>

        <h3>Step 4: Verify UI Module Loading</h3>
        <p>Check console for any errors about UI module loading.</p>
        
        <p>See <a href="ui/reload-ui">Reload UI Changes</a> for more detailed instructions.</p>
    </div>

    <div class="section">
        <h2>Troubleshooting</h2>
        
        <h3>Problem: Menu Items Not Showing</h3>
        <p><strong>Solution:</strong></p>
        <ul>
            <li>Make sure <code>vm_ui</code> is set to <code>0</code></li>
            <li>Completely restart the game</li>
            <li>Verify <code>mymod/vm/uix86_64.so</code> exists and is recent</li>
        </ul>

        <h3>Problem: Scrollbar Not Appearing</h3>
        <p><strong>Solution:</strong></p>
        <ul>
            <li>Scroll down with mouse wheel - scrollbar appears when content exceeds screen</li>
            <li>Use arrow keys to navigate - all items are accessible</li>
        </ul>

        <h3>Problem: Old Menu Still Showing</h3>
        <p><strong>Solution:</strong></p>
        <ul>
            <li>Check for <code>mymod/vm/ui.qvm</code> - if it exists, remove it (QVM takes precedence)</li>
            <li>Verify you're loading the <code>mymod</code> mod, not base game</li>
        </ul>
    </div>

    <div class="section">
        <h2>Technical Details</h2>
        
        <h3>Menu Statistics</h3>
        <ul>
            <li><strong>Total Menu Items:</strong> 51 (25 original + 26 new)</li>
            <li><strong>Menu System:</strong> Uses Quake 3 menu framework</li>
            <li><strong>Scrolling:</strong> Custom implementation with position tracking</li>
            <li><strong>Scrollbar:</strong> Drawn using <code>UI_FillRect</code> API</li>
            <li><strong>Build System:</strong> Makefile in <code>mymod/gamesrc/</code></li>
        </ul>

        <h3>Implementation Details</h3>
        <p>The scrolling system works by:</p>
        <ul>
            <li>Tracking the current scroll offset</li>
            <li>Adjusting item positions based on scroll position</li>
            <li>Drawing a scrollbar indicator showing current position</li>
            <li>Handling mouse wheel events for scrolling</li>
            <li>Maintaining keyboard navigation compatibility</li>
        </ul>
    </div>

    <div class="section">
        <h2>Verification</h2>
        
        <h3>Verify UI Module Contains Changes</h3>
        <p>To verify the UI module contains the changes, use the <code>strings</code> command:</p>
        <div class="code-block">
            <pre><code>strings mymod/vm/uix86_64.so | grep "Ray Tracing"
strings mymod/vm/uix86_64.so | grep "DLSS"
strings mymod/vm/uix86_64.so | grep "Mesh Shaders"</code></pre>
        </div>
        <p>All should return results if the module is built correctly.</p>

        <h3>Check Module Timestamp</h3>
        <p>Verify the module was recently built:</p>
        <div class="code-block">
            <pre><code>ls -lh mymod/vm/uix86_64.so</code></pre>
        </div>
        <p>The timestamp should match when you last built the module.</p>
    </div>

    <div class="section">
        <h2>Feature Descriptions</h2>
        
        <h3>Ray Tracing</h3>
        <p>Hardware-accelerated ray tracing for realistic lighting, reflections, and global illumination. Requires compatible GPU (NVIDIA RTX or AMD RX 6000+).</p>

        <h3>DLSS (Deep Learning Super Sampling)</h3>
        <p>AI-powered upscaling technology that improves performance while maintaining image quality. Requires NVIDIA RTX GPU.</p>

        <h3>Mesh Shaders</h3>
        <p>Modern GPU geometry processing pipeline for improved performance with complex geometry.</p>

        <h3>Virtual Texturing</h3>
        <p>Efficient texture streaming system that allows for larger texture datasets with reduced memory usage.</p>

        <h3>Advanced Materials</h3>
        <p>Enhanced material features including clearcoat, anisotropic materials, and subsurface scattering for more realistic rendering.</p>

        <h3>GPU Particles</h3>
        <p>GPU-accelerated particle system for improved performance and visual quality of particle effects.</p>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="ui/ui">UI System</a> - Complete UI system documentation</li>
            <li><a href="ui/reload-ui">Reload UI Changes</a> - How to reload UI changes</li>
            <li><a href="rendering/ray-tracing">Ray Tracing</a> - Ray tracing documentation</li>
            <li><a href="rendering/pbr">PBR Pipeline</a> - Physically-based rendering</li>
            <li><a href="development/modding">Modding Guide</a> - Mod development guide</li>
        </ul>
    </div>
</div>

