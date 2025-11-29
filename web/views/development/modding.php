<?php 
$title = "Quake III Modding Guide - id Tech 3 Documentation";
$description = "Complete guide to programming mods for Quake III Arena using the id Tech 3 engine";
$breadcrumbs = [
    '/development' => 'Development',
    '/development/modding' => 'Modding Guide'
];
?>

<div class="content-section">
    <h1>Quake III Arena Modding Guide</h1>
    
    <blockquote>
        <strong>Welcome to Modding:</strong> This comprehensive guide will teach you everything you need to know about creating modifications for Quake III Arena using the id Tech 3 engine.
    </blockquote>

    <div class="section">
        <h2>Historical Context</h2>
        <p>Modding has been a core part of id Tech 3 since its original release in 1999. The engine's Virtual Machine (QVM) architecture was specifically designed to enable extensive community modifications, leading to thousands of custom maps, mods, and total conversions over more than two decades.</p>
        <p>The open source release in 2005 further expanded modding possibilities, allowing deeper engine modifications and total conversions. See <a href="history">History of id Tech 3</a> to learn about the engine's modding legacy and community evolution.</p>
    </div>

    <h2>Chapter 1: Introduction to Programming Mods</h2>
    
    <h3>What Is a Mod?</h3>
    <p>A <strong>modification</strong> (mod) is a user-created addition or change to a video game that alters gameplay, graphics, or other aspects of the original game. In Quake III Arena, mods can range from simple weapon tweaks to complete gameplay overhauls.</p>
    
    <h3>Why Create a Mod Instead of Just Writing a Game?</h3>
    <ul>
        <li><strong>Established Engine:</strong> Leverage the proven id Tech 3 engine</li>
        <li><strong>Built-in Networking:</strong> Multiplayer support out of the box</li>
        <li><strong>Rendering System:</strong> Advanced graphics capabilities already implemented</li>
        <li><strong>Asset Pipeline:</strong> Tools and formats already established</li>
        <li><strong>Community:</strong> Existing player base and modding community</li>
    </ul>

    <h3>The Tools of the Trade</h3>
    
    <h4>Using C Programming</h4>
    <p>Quake III mods are primarily written in <span class="keyword">C</span>, providing:</p>
    <ul>
        <li><strong>Performance:</strong> Direct hardware access and optimized execution</li>
        <li><strong>Control:</strong> Fine-grained control over game mechanics</li>
        <li><strong>Compatibility:</strong> Works with the existing Q3 codebase</li>
        <li><strong>Portability:</strong> Cross-platform support</li>
    </ul>

    <h4>Development Environment</h4>
    <div class="example">
        <pre>// Required tools for Q3 modding:
// - Visual C++ or GCC compiler
// - Quake III Arena SDK
// - Text editor or IDE
// - Q3Map2 (for level compilation)
// - 3D modeling software (optional)</pre>
    </div>

    <h2>Chapter 2: C Programming in Quake III</h2>
    
    <h3>The History of Quake and Its Code</h3>
    <p>The evolution from DOS-based Quake to the modern Win32/Linux Quake III represents a significant advancement in game engine architecture.</p>

    <h3>Getting Set Up</h3>
    
    <h4>Installing Q3 and the Source</h4>
    <ol>
        <li>Install Quake III Arena</li>
        <li>Download the official Q3 source code</li>
        <li>Set up your development environment</li>
        <li>Configure build paths and dependencies</li>
    </ol>

    <h4>The Source Directory Structure</h4>
    <div class="example">
        <pre>code/
├── game/          # Server-side game logic
├── cgame/         # Client-side rendering and effects  
├── ui/            # User interface
├── q3_ui/         # Alternative UI system
├── botlib/        # Bot AI library
├── qcommon/       # Shared code
├── renderer/      # Rendering engine
├── server/        # Server code
├── client/        # Client code
└── tools/         # Development tools</pre>
    </div>

    <h3>Building the Source</h3>
    <p>The Quake III source uses a modular architecture with three main components:</p>
    <ul>
        <li><span class="keyword">game</span> - Server-side logic (weapons, physics, AI)</li>
        <li><span class="keyword">cgame</span> - Client-side presentation (effects, HUD)</li>
        <li><span class="keyword">ui</span> - User interface menus and controls</li>
    </ul>

    <h2>Chapter 3: Weapon System Fundamentals</h2>
    
    <h3>Understanding Weapon Types</h3>
    <p>Quake III weapons fall into several categories:</p>
    
    <table>
        <thead>
            <tr>
                <th>Weapon Type</th>
                <th>Examples</th>
                <th>Characteristics</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Hitscan</strong></td>
                <td>Machinegun, Shotgun, Railgun</td>
                <td>Instant hit detection</td>
            </tr>
            <tr>
                <td><strong>Projectile</strong></td>
                <td>Rocket Launcher, Plasma Gun</td>
                <td>Physics-based projectiles</td>
            </tr>
            <tr>
                <td><strong>Melee</strong></td>
                <td>Gauntlet</td>
                <td>Close-range contact</td>
            </tr>
            <tr>
                <td><strong>Hybrid</strong></td>
                <td>Lightning Gun</td>
                <td>Continuous beam weapons</td>
            </tr>
        </tbody>
    </table>

    <h3>A Simple Mod: The Homing Missile</h3>
    <p>Let's create a basic homing missile to understand Q3's entity system:</p>
    
    <div class="example">
        <pre>// g_missile.c - Homing missile implementation
void G_HomingMissile(gentity_t *ent) {
    gentity_t *target;
    vec3_t dir;
    
    // Find the nearest enemy
    target = G_FindNearestEnemy(ent);
    if (!target) {
        return;
    }
    
    // Calculate direction to target
    VectorSubtract(target->r.currentOrigin, ent->r.currentOrigin, dir);
    VectorNormalize(dir);
    
    // Adjust missile velocity
    VectorScale(dir, 900, ent->s.pos.trDelta);
    
    // Set next think time
    ent->nextthink = level.time + 100;
    ent->think = G_HomingMissile;
}</pre>
    </div>

    <h3>Entities: Building Blocks in Q3</h3>
    <p>Everything in Quake III is an <span class="keyword">entity</span>:</p>
    <ul>
        <li><strong>Players:</strong> Human and bot characters</li>
        <li><strong>Weapons:</strong> Projectiles and effects</li>
        <li><strong>Items:</strong> Pickups and powerups</li>
        <li><strong>World:</strong> Static geometry and triggers</li>
    </ul>

    <h2>Chapter 4: Player Manipulation</h2>
    
    <h3>The Quake III Player Structure</h3>
    <p>Players in Q3 are complex entities with multiple data structures:</p>
    
    <div class="example">
        <pre>// Key player structures:
typedef struct gclient_s {
    playerState_t   ps;        // Player state
    clientPersistant_t pers;   // Persistent data
    clientSession_t sess;      // Session info
    usercmd_t       cmd;       // Current input
    // ... more fields
} gclient_t;</pre>
    </div>

    <h3>Implementing Locational Damage</h3>
    <p>Add realistic damage based on hit location:</p>
    
    <div class="example">
        <pre>// Damage multipliers by body part
#define DAMAGE_HEAD     3.0f
#define DAMAGE_TORSO    1.0f
#define DAMAGE_LEGS     0.8f
#define DAMAGE_ARMS     0.7f

int G_LocationalDamage(gentity_t *target, vec3_t point, int damage) {
    int location = G_GetHitLocation(target, point);
    float multiplier = 1.0f;
    
    switch(location) {
        case LOCATION_HEAD:
            multiplier = DAMAGE_HEAD;
            break;
        case LOCATION_TORSO:
            multiplier = DAMAGE_TORSO;
            break;
        case LOCATION_LEGS:
            multiplier = DAMAGE_LEGS;
            break;
    }
    
    return (int)(damage * multiplier);
}</pre>
    </div>

    <h2>Chapter 5: Networking and Communication</h2>
    
    <h3>The Client/Server Relationship</h3>
    <p>Quake III uses a <strong>client-server architecture</strong> where:</p>
    <ul>
        <li><strong>Server:</strong> Authoritative game state, physics simulation</li>
        <li><strong>Client:</strong> Input handling, rendering, prediction</li>
        <li><strong>Synchronization:</strong> Regular state updates between components</li>
    </ul>

    <h3>The Quake Virtual Machine (QVM)</h3>
    <p>Q3 mods can be distributed as:</p>
    <ul>
        <li><span class="keyword">.dll/.so files</span> - Native compiled code</li>
        <li><span class="keyword">.qvm files</span> - Platform-independent bytecode</li>
    </ul>

    <blockquote>
        <strong>Security Note:</strong> QVM files provide sandboxed execution, making them safer for distribution than native DLLs.
    </blockquote>

    <h2>Advanced Topics</h2>
    
    <h3>Custom Game Modes</h3>
    <p>Creating new game types like "Defend the Flag" involves:</p>
    <ul>
        <li>Defining new game rules and win conditions</li>
        <li>Creating custom items and objectives</li>
        <li>Implementing scoring systems</li>
        <li>Adding UI elements and feedback</li>
    </ul>

    <h3>Asset Creation Pipeline</h3>
    <ul>
        <li><strong>Models:</strong> 3D Studio Max, Maya, Blender</li>
        <li><strong>Textures:</strong> Photoshop, GIMP</li>
        <li><strong>Sounds:</strong> Audacity, professional audio tools</li>
        <li><strong>Maps:</strong> Q3Radiant level editor</li>
    </ul>

    <h2>Getting Started Checklist</h2>
    
    <ol>
        <li>✅ Install Quake III Arena and tools</li>
        <li>✅ Download and compile Q3 source code</li>
        <li>✅ Set up development environment</li>
        <li>✅ Study existing weapon implementations</li>
        <li>✅ Create your first simple modification</li>
        <li>✅ Test in both single and multiplayer</li>
        <li>✅ Package and distribute your mod</li>
    </ol>

    <blockquote>
        <strong>Pro Tip:</strong> Start small with simple modifications before attempting complex total conversions. Master the basics of weapon modification before moving on to new game modes.
    </blockquote>
</div> 