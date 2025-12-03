<?php
$title = "Advanced Engine Systems";
?>

<h1>Advanced Engine Systems</h1>

<p>This document describes the engine-level systems focusing on authoring power, spatial systems, and systemic rendering.</p>

<h2>Overview</h2>

<p>The engine has been enhanced with six major systems:</p>

<ol>
    <li><strong>GPU-Driven Culling and Instancing</strong> - Move visibility and batching to GPU compute</li>
    <li><strong>Material System with Runtime Parameters</strong> - Materials as logic systems</li>
    <li><strong>Cell Streaming</strong> - Region-based world streaming for larger levels</li>
    <li><strong>Animation Event System</strong> - Gameplay hooks tied to animation frames</li>
    <li><strong>Encounter/Sequence Authoring</strong> - Lua-based encounter orchestration</li>
    <li><strong>Atmosphere/Mood System</strong> - Scriptable lighting and post-processing</li>
</ol>

<h2>1. GPU-Driven Culling and Instancing</h2>

<h3>Purpose</h3>
<p>Move frustum culling and instance management to GPU compute shaders for better performance with large numbers of objects.</p>

<h3>Files</h3>
<ul>
    <li><code>src/renderervk/vk_gpu_culling.h</code> / <code>.c</code></li>
    <li><code>src/renderervk/shaders/glsl/gpu_cull.comp</code></li>
    <li><code>src/renderervk/shaders/glsl/gpu_instance.comp</code></li>
</ul>

<h3>Features</h3>
<ul>
    <li>GPU-based frustum culling</li>
    <li>Distance-based culling</li>
    <li>Indirect draw command generation</li>
    <li>Instance compaction</li>
</ul>

<h3>CVars</h3>
<ul>
    <li><code>r_gpuCulling</code> - Enable GPU culling (default: 1)</li>
    <li><code>r_gpuInstancing</code> - Enable GPU instancing (default: 1)</li>
    <li><code>r_cullDistance</code> - Maximum culling distance</li>
</ul>

<h3>Usage</h3>
<pre><code>// Add instance for GPU culling
vk_gpu_culling_add_instance(modelMatrix, entityIndex, color);

// Execute indirect draws
vk_gpu_culling_execute_indirect();
</code></pre>

<h2>2. Material System with Runtime Parameters</h2>

<h3>Purpose</h3>
<p>Materials become systems that can react to gameplay state (wetness, damage, corruption, magic).</p>

<h3>Files</h3>
<ul>
    <li><code>src/renderervk/vk_material_system.h</code> / <code>.c</code></li>
    <li><code>src/renderervk/shaders/glsl/material_params.glsl</code></li>
</ul>

<h3>Features</h3>
<ul>
    <li>Runtime material parameter modification</li>
    <li>Wetness effects (reduced roughness)</li>
    <li>Damage effects (increased roughness, color tinting)</li>
    <li>Corruption effects (emissive glow, color shift)</li>
    <li>Magic glow effects</li>
    <li>Lua scripting interface</li>
</ul>

<h3>CVars</h3>
<ul>
    <li><code>r_materialSystem</code> - Enable material system (default: 1)</li>
    <li><code>r_materialWetness</code> - Global wetness override</li>
    <li><code>r_materialDamage</code> - Global damage override</li>
    <li><code>r_materialMagic</code> - Global magic override</li>
</ul>

<h3>Usage</h3>
<pre><code>// Set material wetness
vk_material_set_wetness(materialIndex, 0.5f);

// Set material damage
vk_material_set_damage(materialIndex, 0.3f);

// Set magic glow
vec3_t glowColor = {1.0f, 0.5f, 0.0f};
vk_material_set_magic_glow(materialIndex, 0.8f, glowColor);
</code></pre>

<h3>Shader Integration</h3>
<p>Include <code>material_params.glsl</code> in your fragment shader:</p>
<pre><code>#include "material_params.glsl"

vec3 emissive;
vec3 finalColor = applyMaterialParams(materialIndex, baseColor, roughness, metallic, emissive);
float finalRoughness = getMaterialRoughness(materialIndex, baseRoughness);
</code></pre>

<h2>3. Cell Streaming System</h2>

<h3>Purpose</h3>
<p>Stream world regions dynamically instead of loading entire maps, enabling larger contiguous levels.</p>

<h3>Files</h3>
<ul>
    <li><code>src/renderervk/vk_cell_streaming.h</code> / <code>.c</code></li>
</ul>

<h3>Features</h3>
<ul>
    <li>Cell-based world partitioning</li>
    <li>Priority-based loading queue</li>
    <li>Distance-based unloading</li>
    <li>Per-cell asset management</li>
</ul>

<h3>CVars</h3>
<ul>
    <li><code>r_cellStreaming</code> - Enable cell streaming (default: 1)</li>
    <li><code>r_cellLoadRadius</code> - Cells to load around player (default: 2)</li>
    <li><code>r_cellUnloadDistance</code> - Distance to unload cells (default: 4)</li>
</ul>

<h3>Usage</h3>
<pre><code>// Update streaming (call each frame)
vec3_t playerPos;
vk_cell_streaming_update(playerPos);

// Check if cell is loaded
if (vk_cell_streaming_is_cell_loaded(cellX, cellY, cellZ)) {
    // Render cell content
}
</code></pre>

<h2>4. Animation Event System</h2>

<h3>Purpose</h3>
<p>Provide gameplay hooks tied to specific animation frames (hit frames, parry windows, recovery).</p>

<h3>Files</h3>
<ul>
    <li><code>src/game/animation_events.h</code> / <code>.c</code></li>
</ul>

<h3>Features</h3>
<ul>
    <li>Hit frame events</li>
    <li>Parry window open/close</li>
    <li>Recovery start/end</li>
    <li>Custom events with string parameters</li>
    <li>Lua integration</li>
</ul>

<h3>Usage</h3>
<pre><code>// Register callback for hit frame
void OnHitFrame(int entityNum, anim_event_type_t eventType, const char *customData) {
    // Deal damage, play sound, etc.
}

G_RegisterAnimationEvent(entityNum, ANIM_EVENT_HIT_FRAME, OnHitFrame);

// Trigger event from animation system
G_TriggerAnimationEvent(entityNum, ANIM_EVENT_HIT_FRAME, NULL);
</code></pre>

<h2>5. Encounter/Sequence Authoring System</h2>

<h3>Purpose</h3>
<p>Lua-based system for authoring encounters, sequences, and world state management.</p>

<h3>Files</h3>
<ul>
    <li><code>src/game/encounter_system.h</code> / <code>.c</code></li>
</ul>

<h3>Features</h3>
<ul>
    <li>Encounter definition and management</li>
    <li>Sequence orchestration (multiple encounters)</li>
    <li>World state system (corruption, weather, etc.)</li>
    <li>Trigger-based activation</li>
    <li>Lua scripting interface</li>
</ul>

<h3>Usage</h3>
<pre><code>// Create encounter
encounter_t *encounter = G_Encounter_Create("boss_fight", "scripts/boss.lua");
G_Encounter_SetTrigger(encounter, ENCOUNTER_TRIGGER_PLAYER_PROXIMITY, position, 100.0f);

// Create sequence
sequence_t *sequence = G_Sequence_Create("level_1_sequence");
G_Sequence_AddEncounter(sequence, encounter1);
G_Sequence_AddEncounter(sequence, encounter2);
G_Sequence_Start(sequence);

// World state
G_WorldState_Set("corruption", 0.7f, 2.0f); // Transition over 2 seconds
float corruption = G_WorldState_Get("corruption");
</code></pre>

<h2>6. Atmosphere/Mood System</h2>

<h3>Purpose</h3>
<p>Scriptable lighting, fog, and post-processing effects for mood control.</p>

<h3>Files</h3>
<ul>
    <li><code>src/renderervk/vk_atmosphere.h</code> / <code>.c</code></li>
</ul>

<h3>Features</h3>
<ul>
    <li>Preset atmospheres (Brutal, Mysterious, Combat, Calm)</li>
    <li>Custom atmosphere parameters</li>
    <li>Smooth transitions between moods</li>
    <li>Exposure, contrast, saturation control</li>
    <li>Fog system with height falloff</li>
    <li>Bloom post-processing</li>
    <li>Color grading</li>
    <li>Time of day simulation</li>
</ul>

<h3>CVars</h3>
<ul>
    <li><code>r_atmosphere</code> - Enable atmosphere system (default: 1)</li>
    <li><code>r_atmospherePreset</code> - Current preset (0=Brutal, 1=Mysterious, 2=Combat, 3=Calm)</li>
    <li><code>r_fogDensity</code> - Fog density override</li>
    <li><code>r_bloomIntensity</code> - Bloom intensity override</li>
</ul>

<h3>Usage</h3>
<pre><code>// Set preset with transition
vk_atmosphere_set_preset(ATMOSPHERE_MYSTERIOUS, 1.0f); // 1 second transition

// Custom atmosphere
atmosphere_params_t custom;
custom.exposure = 1.2f;
custom.fogDensity = 0.1f;
custom.bloomIntensity = 0.6f;
vk_atmosphere_set_custom(&custom, 0.5f);

// Individual parameters
vk_atmosphere_set_exposure(1.1f, 0.3f);
vk_atmosphere_set_fog(0.05f, 100.0f, 2000.0f, fogColor, 1.0f);
</code></pre>

<h2>Integration Points</h2>

<p>All systems are integrated into the main renderer lifecycle:</p>

<h3>Initialization (<code>vk_initialize</code>)</h3>
<pre><code>vk_gpu_culling_init();
vk_material_system_init();
vk_cell_streaming_init();
vk_atmosphere_init();
</code></pre>

<h3>Shutdown (<code>vk_shutdown</code>)</h3>
<pre><code>vk_atmosphere_shutdown();
vk_cell_streaming_shutdown();
vk_material_system_shutdown();
vk_gpu_culling_shutdown();
</code></pre>

<h3>Per-Frame Update (<code>vk_begin_frame</code>)</h3>
<pre><code>vk_gpu_culling_update();
vk_material_system_update();
vk_atmosphere_update();
</code></pre>

<h2>Lua Integration</h2>

<p>All systems expose Lua functions for scripting:</p>

<ul>
    <li>Material system: <code>MaterialSetWetness()</code>, <code>MaterialSetDamage()</code>, etc.</li>
    <li>Animation events: <code>OnHitFrame()</code>, <code>OnParryWindow()</code>, etc.</li>
    <li>Encounters: <code>EncounterDefine()</code>, <code>SequenceDefine()</code>, etc.</li>
    <li>World state: <code>WorldStateSet()</code>, <code>WorldStateGet()</code></li>
    <li>Atmosphere: <code>AtmosphereSetPreset()</code>, <code>AtmosphereSetFog()</code>, etc.</li>
</ul>

<h2>Next Steps</h2>

<ol>
    <li><strong>Complete Pipeline Creation</strong>: Implement compute pipeline creation for GPU culling</li>
    <li><strong>Shader Compilation</strong>: Compile GLSL compute shaders to SPIR-V</li>
    <li><strong>PBR Integration</strong>: Integrate material parameters into PBR fragment shader</li>
    <li><strong>Lua Bindings</strong>: Complete Lua function registration</li>
    <li><strong>Spatial Acceleration</strong>: Add octree/grid for efficient surfel lookup (GIBS)</li>
    <li><strong>Performance Profiling</strong>: Profile and optimize GPU culling performance</li>
</ol>

<h2>Design Philosophy</h2>

<p>These systems follow the principle: <strong>"Don't chase fidelity first—steal the architecture."</strong></p>

<p>The focus is on:</p>
<ul>
    <li><strong>Authoring Power</strong>: Systems that make content creation easier</li>
    <li><strong>Spatial Awareness</strong>: Systems that understand world space and proximity</li>
    <li><strong>Systemic Design</strong>: Systems that react to gameplay state</li>
    <li><strong>Scriptability</strong>: Everything exposed to Lua for rapid iteration</li>
</ul>

<p>This architecture enables:</p>
<ul>
    <li>Larger, more contiguous levels</li>
    <li>More dynamic, reactive environments</li>
    <li>Better performance through GPU-driven rendering</li>
    <li>Faster content iteration through scripting</li>
</ul>

