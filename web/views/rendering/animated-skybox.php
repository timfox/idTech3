<?php
/**
 * Animated Skybox Documentation
 */
$title = 'Animated Skybox - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/animated-skybox' => 'Animated Skybox'
];
?>

<h1>Animated Skybox (Flipbook)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The animated skybox feature allows skyboxes to cycle through multiple texture frames over time, creating dynamic sky effects. Each side of the skybox (6 sides for both outerbox and innerbox) can animate independently through numbered frames, similar to how <code>animMap</code> works for regular textures.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Per-Side Animation:</strong> Each side of the skybox animates independently</li>
            <li><strong>Configurable Speed:</strong> Animation speed specified in frames per second</li>
            <li><strong>Flexible Frame Counts:</strong> Different sides can have different frame counts</li>
            <li><strong>Automatic Frame Loading:</strong> Engine automatically loads frames starting from frame 0</li>
            <li><strong>Backward Compatible:</strong> Existing <code>skyParms</code> shaders continue to work unchanged</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Shader Syntax</h2>
    
    <h3>Basic Syntax</h3>
    <div class="code-block">
        <pre><code>skyParmsFlipbook <base> <cloudheight> <innerbase> <animSpeed></code></pre>
    </div>
    
    <h3>Parameters</h3>
    <ul>
        <li><strong><code>&lt;base&gt;</code>:</strong> Base path for outerbox textures (e.g., <code>env/sky</code>)</li>
        <li><strong><code>&lt;cloudheight&gt;</code>:</strong> Cloud height parameter (same as regular <code>skyParms</code>)</li>
        <li><strong><code>&lt;innerbase&gt;</code>:</strong> Base path for innerbox textures (e.g., <code>env/sky_inner</code>, or <code>-</code> to disable)</li>
        <li><strong><code>&lt;animSpeed&gt;</code>:</strong> Animation speed in frames per second (e.g., <code>8.0</code>)</li>
    </ul>
</div>

<div class="section">
    <h2>File Naming Convention</h2>
    <p>The engine automatically loads frames using the following naming pattern:</p>
    
    <div class="code-block">
        <pre><code>&lt;base&gt;_&lt;side&gt;_&lt;frame&gt;.tga</code></pre>
    </div>
    
    <p>Where:</p>
    <ul>
        <li><strong><code>&lt;base&gt;</code></strong> is the base path specified in the shader</li>
        <li><strong><code>&lt;side&gt;</code></strong> is one of: <code>rt</code> (right), <code>bk</code> (back), <code>lf</code> (left), <code>ft</code> (front), <code>up</code> (up), <code>dn</code> (down)</li>
        <li><strong><code>&lt;frame&gt;</code></strong> is the frame number starting from <code>0</code> (e.g., <code>0</code>, <code>1</code>, <code>2</code>, ...)</li>
    </ul>
    
    <h3>Example File Structure</h3>
    <p>For a shader using <code>skyParmsFlipbook env/sky 512 env/sky_inner 8.0</code>, the engine will look for:</p>
    
    <h4>Outerbox:</h4>
    <div class="code-block">
        <pre><code>env/sky_rt_0.tga, env/sky_rt_1.tga, env/sky_rt_2.tga, ...
env/sky_bk_0.tga, env/sky_bk_1.tga, env/sky_bk_2.tga, ...
env/sky_lf_0.tga, env/sky_lf_1.tga, env/sky_lf_2.tga, ...
env/sky_ft_0.tga, env/sky_ft_1.tga, env/sky_ft_2.tga, ...
env/sky_up_0.tga, env/sky_up_1.tga, env/sky_up_2.tga, ...
env/sky_dn_0.tga, env/sky_dn_1.tga, env/sky_dn_2.tga, ...</code></pre>
    </div>
    
    <h4>Innerbox:</h4>
    <div class="code-block">
        <pre><code>env/sky_inner_rt_0.tga, env/sky_inner_rt_1.tga, ...
(same pattern for all 6 sides)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Shader Examples</h2>
    
    <h3>Example 1: Simple Animated Skybox</h3>
    <div class="code-block">
        <pre><code>textures/skies/animated_sky
{
    skyParmsFlipbook env/sky 512 env/sky_inner 8.0
    // Animation speed: 8 frames per second
}</code></pre>
    </div>
    <p>This will animate through all available frames for each side at 8 frames per second.</p>
    
    <h3>Example 2: Fast Animation</h3>
    <div class="code-block">
        <pre><code>textures/skies/fast_animated_sky
{
    skyParmsFlipbook env/storm 512 - 15.0
    // Fast animation: 15 frames per second
    // No innerbox (using "-")
}</code></pre>
    </div>
    
    <h3>Example 3: Slow, Smooth Animation</h3>
    <div class="code-block">
        <pre><code>textures/skies/slow_animated_sky
{
    skyParmsFlipbook env/clouds 512 env/clouds_inner 2.0
    // Slow animation: 2 frames per second for smooth transitions
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Technical Details</h2>
    
    <h3>Frame Loading</h3>
    <ul>
        <li>The engine automatically loads frames starting from frame <code>0</code> and continues until a frame file is not found</li>
        <li>Each side can have a different number of frames (flexible per-side animation)</li>
        <li>Maximum frames per side: <strong>24</strong> (defined by <code>MAX_SKY_ANIMATIONS</code>)</li>
        <li>If no frames are found for a side, the default image is used</li>
    </ul>
    
    <h3>Animation Timing</h3>
    <ul>
        <li>Animation speed is specified in frames per second</li>
        <li>Frame selection uses modulo arithmetic to wrap animation cycles smoothly</li>
        <li>Animation is synchronized with shader time (<code>tess.shaderTime</code>)</li>
        <li>The calculation matches the logic used by <code>animMap</code> for regular textures</li>
    </ul>
    
    <h3>Per-Side Animation</h3>
    <p>Each side of the skybox animates independently:</p>
    <ul>
        <li>Different sides can have different frame counts</li>
        <li>All sides use the same animation speed</li>
        <li>Frame selection is calculated per-side based on shader time</li>
    </ul>
    
    <h3>Backward Compatibility</h3>
    <ul>
        <li>Existing <code>skyParms</code> shaders continue to work unchanged</li>
        <li>Non-animated skyboxes use <code>outerbox[i][0]</code> automatically</li>
        <li>The <code>isAnimated</code> flag defaults to <code>qfalse</code> for regular skyboxes</li>
    </ul>
</div>

<div class="section">
    <h2>Limitations</h2>
    <ol>
        <li><strong>Frame Count:</strong> Maximum 24 frames per side</li>
        <li><strong>File Format:</strong> Currently supports <code>.tga</code> format only (same as regular skyboxes)</li>
        <li><strong>Naming:</strong> Frame numbers must start from <code>0</code> and be sequential (no gaps)</li>
        <li><strong>Animation Speed:</strong> Must be greater than <code>0.0</code> (defaults to <code>8.0</code> if invalid)</li>
    </ol>
</div>

<div class="section">
    <h2>Tips</h2>
    <ol>
        <li><strong>Consistent Frame Counts:</strong> While different sides can have different frame counts, using the same count for all sides creates smoother, more predictable animations</li>
        
        <li><strong>Animation Speed:</strong>
            <ul>
                <li>Lower speeds (2-5 fps) create smooth, slow transitions</li>
                <li>Medium speeds (8-12 fps) work well for most effects</li>
                <li>Higher speeds (15+ fps) create fast, dynamic effects</li>
            </ul>
        </li>
        
        <li><strong>File Organization:</strong> Keep all frames for a skybox in the same directory for easier management</li>
        
        <li><strong>Testing:</strong> Use <code>r_showsky 1</code> to see skybox rendering clearly during development</li>
        
        <li><strong>Performance:</strong> Animated skyboxes have minimal performance impact - frame selection is O(1) per side</li>
    </ol>
</div>

<div class="section">
    <h2>Comparison with Regular Skyboxes</h2>
    <table class="settings-table">
        <tr>
            <th>Feature</th>
            <th>Regular <code>skyParms</code></th>
            <th>Animated <code>skyParmsFlipbook</code></th>
        </tr>
        <tr>
            <td>Frames per side</td>
            <td>1</td>
            <td>Up to 24</td>
        </tr>
        <tr>
            <td>Animation</td>
            <td>Static</td>
            <td>Animated</td>
        </tr>
        <tr>
            <td>File naming</td>
            <td><code>&lt;base&gt;_&lt;side&gt;.tga</code></td>
            <td><code>&lt;base&gt;_&lt;side&gt;_&lt;frame&gt;.tga</code></td>
        </tr>
        <tr>
            <td>Speed control</td>
            <td>N/A</td>
            <td>Configurable fps</td>
        </tr>
        <tr>
            <td>Per-side frames</td>
            <td>Same for all</td>
            <td>Can differ</td>
        </tr>
    </table>
</div>

<div class="section">
    <h2>Implementation Notes</h2>
    <ul>
        <li>Supported in all three renderers: OpenGL, Vulkan, and renderer2</li>
        <li>Uses the same animation timing system as <code>animMap</code> for consistency</li>
        <li>Frame selection happens at render time, ensuring smooth animation</li>
        <li>Memory overhead is minimal - only stores pointers to loaded images</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/shaders">Vulkan Shaders</a></li>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="development/map-making">Map Making</a></li>
    </ul>
</div>

