<?php
/**
 * Animation System Documentation
 */
$title = 'Animation System - id Tech 3 Documentation';
$breadcrumbs = [
    '/animation' => 'Animation',
    '/animation/animation' => 'Animation System'
];
?>

<h1>Animation System</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 animation system supports multiple animation formats and techniques, from classic frame-based MD3 models to modern skeletal animation systems in Quake3e.</p>
    
    <div class="feature-list">
        <h3>Supported Animation Types</h3>
        <ul>
            <li><strong>MD3 Models:</strong> Frame-based vertex animation</li>
            <li><strong>Skeletal Animation:</strong> Bone-based animation (Quake3e)</li>
            <li><strong>Morph Targets:</strong> Vertex morphing for facial animation</li>
            <li><strong>Procedural Animation:</strong> Code-driven movement</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>MD3 Animation System</h2>
    
    <h3>Frame-Based Animation</h3>
    <p>Traditional id Tech 3 uses MD3 models with frame-based animation:</p>
    <div class="code-block">
        <pre><code>// MD3 animation sequence
typedef struct {
    int firstFrame;     // Starting frame number
    int numFrames;      // Number of frames in sequence
    int loopFrames;     // Frames to loop (0 = no loop)
    int frameLerp;      // Frame interpolation time
    int initialLerp;    // Initial lerp time
    int reversed;       // Play in reverse
    int flipflop;       // Ping-pong animation
} animType_t;</code></pre>
    </div>
    
    <h3>Animation Configuration</h3>
    <div class="code-block">
        <pre><code>// animation.cfg example
0    30   0   25      // BOTH_DEATH1
31   40   0   25      // BOTH_DEAD1  
41   50   -1  25      // TORSO_GESTURE (looping)
51   70   0   15      // LEGS_WALKCR</code></pre>
    </div>
</div>

<div class="section">
    <h2>Modern Skeletal Animation (Quake3e)</h2>
    
    <h3>Bone-Based System</h3>
    <p>Quake3e supports modern skeletal animation with IQM models:</p>
    <ul>
        <li><strong>IQM Format:</strong> Inter-Quake Model format support</li>
        <li><strong>Bone Weights:</strong> Smooth vertex deformation</li>
        <li><strong>Animation Blending:</strong> Smooth transitions between animations</li>
        <li><strong>GPU Acceleration:</strong> Hardware-accelerated skinning</li>
    </ul>
    
    <div class="code-block">
        <pre><code># Enable skeletal animation in Quake3e
seta cg_animBlend "1"          // Enable animation blending
seta r_iqmModels "1"           // Enable IQM model support
seta r_gpuSkinning "1"         // Use GPU for skeletal animation</code></pre>
    </div>
</div>

<div class="section">
    <h2>Animation Interpolation</h2>
    
    <h3>Frame Interpolation</h3>
    <p>Smooth animation between frames:</p>
    <div class="code-block">
        <pre><code># Animation smoothing settings
seta cg_animLerp "1"           // Enable frame interpolation
seta cg_animFPS "30"           // Target animation FPS
seta cg_debugAnim "0"          // Debug animation info</code></pre>
    </div>
    
    <h3>Blending Techniques</h3>
    <ul>
        <li><strong>Linear Interpolation:</strong> Simple frame blending</li>
        <li><strong>Spherical Interpolation:</strong> Smooth rotation blending</li>
        <li><strong>Cubic Interpolation:</strong> Enhanced smoothness</li>
    </ul>
</div>

<div class="section">
    <h2>Character Animation</h2>
    
    <h3>Player Animations</h3>
    <div class="code-block">
        <pre><code>// Player animation states
typedef enum {
    BOTH_DEATH1,
    BOTH_DEAD1,
    BOTH_DEATH2,
    BOTH_DEAD2,
    BOTH_DEATH3,
    BOTH_DEAD3,
    
    TORSO_GESTURE,
    TORSO_ATTACK,
    TORSO_ATTACK2,
    TORSO_DROP,
    TORSO_RAISE,
    TORSO_STAND,
    TORSO_STAND2,
    
    LEGS_WALKCR,
    LEGS_WALK,
    LEGS_RUN,
    LEGS_BACK,
    LEGS_SWIM,
    LEGS_JUMP,
    LEGS_LAND,
    LEGS_JUMPB,
    LEGS_LANDB,
    LEGS_IDLE,
    LEGS_IDLECR,
    LEGS_TURN
} animNumber_t;</code></pre>
    </div>
    
    <h3>Weapon Animations</h3>
    <ul>
        <li><strong>Ready:</strong> Weapon idle state</li>
        <li><strong>Firing:</strong> Attack animation</li>
        <li><strong>Reload:</strong> Ammunition replenishment</li>
        <li><strong>Raise/Lower:</strong> Weapon switching</li>
    </ul>
</div>

<div class="section">
    <h2>Custom Animation Creation</h2>
    
    <h3>MD3 Animation Pipeline</h3>
    <ol>
        <li>Create model in 3D software (Blender, Maya)</li>
        <li>Animate using keyframes</li>
        <li>Export as MD3 with multiple frames</li>
        <li>Configure animation.cfg file</li>
        <li>Test in-game</li>
    </ol>
    
    <h3>Tools for Animation</h3>
    <ul>
        <li><strong>Blender:</strong> MD3 export plugins available</li>
        <li><strong>Noesis:</strong> Model conversion tool</li>
        <li><strong>MilkShape 3D:</strong> Classic MD3 editor</li>
        <li><strong>Q3MAP2:</strong> Model processing</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Optimization</h2>
    
    <h3>Animation LOD</h3>
    <ul>
        <li><strong>Distance-based:</strong> Reduce animation quality with distance</li>
        <li><strong>Frame Skipping:</strong> Skip frames for distant models</li>
        <li><strong>Culling:</strong> Don't animate off-screen models</li>
    </ul>
    
    <div class="code-block">
        <pre><code># Performance settings
seta cg_animLOD "1"            // Enable animation LOD
seta cg_animDistance "1000"    // Max animation distance
seta r_lodBias "0"             // LOD bias adjustment</code></pre>
    </div>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common Issues</h3>
    <div class="troubleshooting">
        <h4>Animation not playing</h4>
        <ul>
            <li>Check animation.cfg syntax</li>
            <li>Verify frame numbers are correct</li>
            <li>Ensure MD3 has required frames</li>
        </ul>
        
        <h4>Jerky animation</h4>
        <ul>
            <li>Enable frame interpolation</li>
            <li>Check animation FPS settings</li>
            <li>Verify model frame rate</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="animation/md3-models">MD3 Model Format</a></li>
        <li><a href="animation/skeletal-animation">Skeletal Animation</a></li>
        <li><a href="animation/interpolation">Frame Interpolation</a></li>
        <li><a href="tools/asset-tools">Asset Creation Tools</a></li>
    </ul>
</div> 