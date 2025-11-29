<?php
/**
 * Creating Animated Skyboxes Tutorial
 */
$title = 'Creating Animated Skyboxes Tutorial - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/animated-skybox' => 'Creating Animated Skyboxes'
];
?>

<h1>Creating Animated Skyboxes Tutorial</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This tutorial will guide you through creating animated skyboxes using the flipbook animation system in id Tech 3. Animated skyboxes allow you to create dynamic sky effects like moving clouds, day/night cycles, or atmospheric effects.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li>Understanding flipbook animation concept</li>
            <li>Preparing animated sky textures</li>
            <li>Naming conventions for sky textures</li>
            <li>Writing shader code for animated skies</li>
            <li>Configuring animation speed</li>
            <li>Creating per-side animations</li>
            <li>Troubleshooting common issues</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 engine with animated skybox support</li>
        <li>Image editing software (GIMP, Photoshop, etc.)</li>
        <li>Basic understanding of shader syntax</li>
        <li>Knowledge of skybox creation basics</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Understanding Flipbook Animation</h2>
    
    <h3>What is Flipbook Animation?</h3>
    <p>Flipbook animation works like a traditional flipbook - multiple frames are displayed in sequence to create animation. For skyboxes, each frame is a complete sky texture, and the engine cycles through them.</p>
    
    <h3>How It Works</h3>
    <ul>
        <li>Multiple sky textures are loaded as animation frames</li>
        <li>Frames are displayed in sequence</li>
        <li>Animation speed is configurable</li>
        <li>Each skybox side can have its own animation</li>
    </ul>
    
    <h3>Example Use Cases</h3>
    <ul>
        <li>Moving clouds</li>
        <li>Day/night cycle</li>
        <li>Weather effects</li>
        <li>Atmospheric changes</li>
        <li>Animated aurora effects</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Preparing Your Textures</h2>
    
    <h3>Step 1: Create Animation Frames</h3>
    <p>Create a sequence of sky textures representing your animation:</p>
    <ul>
        <li>Each frame should be a complete sky texture</li>
        <li>All frames must be the same size</li>
        <li>Recommended: 512x512 or 1024x1024 pixels per side</li>
        <li>Use TGA format for best compatibility</li>
    </ul>
    
    <h3>Step 2: Name Your Textures</h3>
    <p>Follow the naming convention: <code>texturename_frameN_side.tga</code></p>
    
    <h4>Example: 5-Frame Cloud Animation</h4>
    <div class="code-block">
        <pre><code>textures/skies/clouds_frame0_ft.tga  (front, frame 0)
textures/skies/clouds_frame1_ft.tga  (front, frame 1)
textures/skies/clouds_frame2_ft.tga  (front, frame 2)
textures/skies/clouds_frame3_ft.tga  (front, frame 3)
textures/skies/clouds_frame4_ft.tga  (front, frame 4)

textures/skies/clouds_frame0_bk.tga  (back, frame 0)
textures/skies/clouds_frame1_bk.tga  (back, frame 1)
... (repeat for all frames)

textures/skies/clouds_frame0_up.tga   (up/top, frame 0)
... (repeat for all frames)

textures/skies/clouds_frame0_dn.tga  (down/bottom, frame 0)
... (repeat for all frames)

textures/skies/clouds_frame0_lf.tga  (left, frame 0)
... (repeat for all frames)

textures/skies/clouds_frame0_rt.tga  (right, frame 0)
... (repeat for all frames)</code></pre>
    </div>
    
    <h3>Step 3: Skybox Side Abbreviations</h3>
    <p>Use these abbreviations for each side:</p>
    <ul>
        <li><code>ft</code> - Front</li>
        <li><code>bk</code> - Back</li>
        <li><code>up</code> - Up/Top</li>
        <li><code>dn</code> - Down/Bottom</li>
        <li><code>lf</code> - Left</li>
        <li><code>rt</code> - Right</li>
    </ul>
    
    <h3>Step 4: Frame Numbering</h3>
    <p>Important naming rules:</p>
    <ul>
        <li>Frames must start at 0 (frame0, frame1, frame2...)</li>
        <li>Frames must be sequential (no gaps)</li>
        <li>All sides must have the same number of frames</li>
        <li>Maximum 64 frames per animation</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Writing the Shader</h2>
    
    <h3>Step 1: Basic Animated Skybox Shader</h3>
    <p>Create a shader file (e.g., <code>textures/skies/clouds.shader</code>):</p>
    <div class="code-block">
        <pre><code>textures/skies/clouds
{
    qer_editorimage textures/skies/clouds_frame0_ft.tga
    
    skyParmsFlipbook textures/skies/clouds 5 0.1
    
    {
        map textures/skies/clouds_frame0_ft.tga
        rgbGen identity
    }
}</code></pre>
    </div>
    
    <h3>Step 2: Understanding skyParmsFlipbook</h3>
    <p>The syntax is: <code>skyParmsFlipbook &lt;basename&gt; &lt;frames&gt; &lt;speed&gt;</code></p>
    <ul>
        <li><strong>basename:</strong> Base name of your textures (without frame/side)</li>
        <li><strong>frames:</strong> Number of animation frames (e.g., 5)</li>
        <li><strong>speed:</strong> Animation speed in seconds per frame (e.g., 0.1 = fast, 1.0 = slow)</li>
    </ul>
    
    <h3>Step 3: Complete Shader Example</h3>
    <div class="code-block">
        <pre><code>textures/skies/clouds
{
    qer_editorimage textures/skies/clouds_frame0_ft.tga
    
    // Animated skybox: 5 frames, 0.2 seconds per frame
    skyParmsFlipbook textures/skies/clouds 5 0.2
    
    {
        map textures/skies/clouds_frame0_ft.tga
        rgbGen identity
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Configuring Animation Speed</h2>
    
    <h3>Understanding Speed Parameter</h3>
    <p>The speed parameter controls how long each frame is displayed:</p>
    <ul>
        <li><strong>0.1 seconds:</strong> Very fast animation (10 FPS)</li>
        <li><strong>0.2 seconds:</strong> Fast animation (5 FPS)</li>
        <li><strong>0.5 seconds:</strong> Medium speed (2 FPS)</li>
        <li><strong>1.0 seconds:</strong> Slow animation (1 FPS)</li>
        <li><strong>2.0 seconds:</strong> Very slow animation (0.5 FPS)</li>
    </ul>
    
    <h3>Example: Day/Night Cycle</h3>
    <p>For a 24-hour day/night cycle with 24 frames:</p>
    <div class="code-block">
        <pre><code>textures/skies/daynight
{
    qer_editorimage textures/skies/daynight_frame0_ft.tga
    
    // 24 frames, 60 seconds per frame = 24 minutes total
    skyParmsFlipbook textures/skies/daynight 24 60.0
    
    {
        map textures/skies/daynight_frame0_ft.tga
        rgbGen identity
    }
}</code></pre>
    </div>
    
    <h3>Example: Fast Moving Clouds</h3>
    <div class="code-block">
        <pre><code>textures/skies/fastclouds
{
    qer_editorimage textures/skies/fastclouds_frame0_ft.tga
    
    // 8 frames, 0.15 seconds per frame = fast animation
    skyParmsFlipbook textures/skies/fastclouds 8 0.15
    
    {
        map textures/skies/fastclouds_frame0_ft.tga
        rgbGen identity
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Per-Side Animation</h2>
    
    <h3>Creating Different Animations Per Side</h3>
    <p>You can have different animations for different sides:</p>
    
    <h4>Example: Animated Clouds on Sides, Static Sky on Top</h4>
    <div class="code-block">
        <pre><code>textures/skies/mixed
{
    qer_editorimage textures/skies/mixed_frame0_ft.tga
    
    // Animated sides (ft, bk, lf, rt)
    skyParmsFlipbook textures/skies/mixed 5 0.2
    
    // Static top (up) - use regular skyParms
    skyParms textures/skies/mixed_static_up 512
    
    {
        map textures/skies/mixed_frame0_ft.tga
        rgbGen identity
    }
}</code></pre>
    </div>
    
    <h3>Note on Per-Side Animation</h3>
    <p>When using <code>skyParmsFlipbook</code>, all sides use the same animation. For different animations per side, you may need to use multiple skybox entities or custom shader code.</p>
</div>

<div class="section">
    <h2>Tutorial: Complete Example - Moving Clouds</h2>
    
    <h3>Step 1: Create Texture Files</h3>
    <p>Create 6 frames of cloud textures for each side:</p>
    <div class="code-block">
        <pre><code>textures/skies/clouds_frame0_ft.tga
textures/skies/clouds_frame1_ft.tga
textures/skies/clouds_frame2_ft.tga
textures/skies/clouds_frame3_ft.tga
textures/skies/clouds_frame4_ft.tga
textures/skies/clouds_frame5_ft.tga

# Repeat for bk, up, dn, lf, rt
# Total: 6 frames × 6 sides = 36 texture files</code></pre>
    </div>
    
    <h3>Step 2: Create Shader File</h3>
    <p>Create <code>textures/skies/clouds.shader</code>:</p>
    <div class="code-block">
        <pre><code>textures/skies/clouds
{
    qer_editorimage textures/skies/clouds_frame0_ft.tga
    
    // 6 frames, 0.3 seconds per frame = smooth cloud movement
    skyParmsFlipbook textures/skies/clouds 6 0.3
    
    {
        map textures/skies/clouds_frame0_ft.tga
        rgbGen identity
    }
}</code></pre>
    </div>
    
    <h3>Step 3: Use in Map</h3>
    <p>In your map's <code>worldspawn</code> entity:</p>
    <div class="code-block">
        <pre><code>{
"classname" "worldspawn"
"sky" "textures/skies/clouds"
...
}</code></pre>
    </div>
    
    <h3>Step 4: Test</h3>
    <p>Compile your map and test:</p>
    <div class="code-block">
        <pre><code># Compile map
./q3map2 -game mymod maps/mymap.map

# Run game
./idtech3.x86_64 +map mymap</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Tips and Best Practices</h2>
    
    <h3>Texture Optimization</h3>
    <ul>
        <li>Use compressed textures (DXT1/DXT5) to reduce memory usage</li>
        <li>Keep texture sizes reasonable (512x512 or 1024x1024)</li>
        <li>Limit number of frames to reduce memory footprint</li>
        <li>Consider using fewer frames with interpolation</li>
    </ul>
    
    <h3>Animation Smoothness</h3>
    <ul>
        <li>More frames = smoother animation but more memory</li>
        <li>Faster speed = more noticeable animation</li>
        <li>Test different speeds to find the right feel</li>
        <li>Consider the mood you want to create</li>
    </ul>
    
    <h3>Performance</h3>
    <ul>
        <li>Animated skyboxes use more memory than static ones</li>
        <li>Frame loading happens at map load time</li>
        <li>Animation itself has minimal performance impact</li>
        <li>Limit total frames to avoid excessive memory usage</li>
    </ul>
    
    <h3>Design Tips</h3>
    <ul>
        <li>Create seamless loops for continuous animation</li>
        <li>First and last frames should be similar for smooth looping</li>
        <li>Use subtle animations for atmosphere</li>
        <li>Avoid distracting animations that interfere with gameplay</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Troubleshooting</h2>
    
    <h3>Skybox Not Animating</h3>
    <p><strong>Problem:</strong> Skybox appears but doesn't animate.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify <code>skyParmsFlipbook</code> syntax is correct</li>
        <li>Check that all frame files exist</li>
        <li>Verify frame numbering starts at 0 and is sequential</li>
        <li>Check console for missing texture warnings</li>
        <li>Ensure all sides have the same number of frames</li>
    </ul>
    
    <h3>Missing Textures</h3>
    <p><strong>Problem:</strong> Console shows missing texture errors.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify all texture files exist</li>
        <li>Check file naming matches exactly (case-sensitive)</li>
        <li>Verify texture paths are correct</li>
        <li>Check that textures are in the correct directory</li>
        <li>Ensure textures are properly compiled/packaged</li>
    </ul>
    
    <h3>Animation Too Fast/Slow</h3>
    <p><strong>Problem:</strong> Animation speed doesn't feel right.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Adjust the speed parameter in <code>skyParmsFlipbook</code></li>
        <li>Lower value = faster animation</li>
        <li>Higher value = slower animation</li>
        <li>Test different values to find the right speed</li>
    </ul>
    
    <h3>Jarring Animation</h3>
    <p><strong>Problem:</strong> Animation looks choppy or jumps.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Add more frames for smoother animation</li>
        <li>Ensure frames transition smoothly</li>
        <li>Check that first and last frames match for seamless loop</li>
        <li>Reduce animation speed if too fast</li>
    </ul>
</div>

<div class="section">
    <h2>Limitations</h2>
    <ul>
        <li><strong>Maximum Frames:</strong> 64 frames per animation</li>
        <li><strong>Memory Usage:</strong> All frames loaded into memory</li>
        <li><strong>Same Animation:</strong> All sides use same animation (unless using custom shaders)</li>
        <li><strong>No Interpolation:</strong> Frames switch instantly (no blending)</li>
        <li><strong>Texture Size:</strong> Limited by renderer capabilities</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/animated-skybox">Animated Skybox Documentation</a> - Complete reference</li>
        <li><a href="rendering/shaders">Shader System</a> - Shader documentation</li>
        <li><a href="development/map-making">Map Making Guide</a> - Creating maps with skyboxes</li>
        <li><a href="tools/radiant">Q3Radiant</a> - Level editor</li>
    </ul>
</div>

