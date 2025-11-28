<?php 
$title = "Map Making Guide - id Tech 3 Documentation";
$description = "Complete guide to creating maps for Quake III Arena using Q3Radiant and advanced level design techniques";
$breadcrumbs = [
    '/development' => 'Development',
    '/development/map-making' => 'Map Making'
];
?>

<div class="content-section">
    <h1>Quake III Map Making Guide</h1>
    
    <blockquote>
        <strong>Level Design:</strong> Creating compelling maps is an art that combines technical knowledge with creative vision. This guide covers everything from basic Q3Radiant usage to advanced level design principles.
    </blockquote>

    <h2>Getting Started with Q3Radiant</h2>
    
    <h3>Setting Up Your Environment</h3>
    <p><span class="keyword">Q3Radiant</span> is the official level editor for Quake III Arena. Modern alternatives include <span class="keyword">NetRadiant</span> and <span class="keyword">GtkRadiant</span>.</p>
    
    <h4>Essential Setup Steps</h4>
    <ol>
        <li>Install Q3Radiant or a modern equivalent</li>
        <li>Configure game paths to your Q3 installation</li>
        <li>Set up texture directories</li>
        <li>Configure compiler tools (q3map2)</li>
    </ol>

    <div class="example">
        <pre>// Basic Radiant configuration
// File: q3radiant.ini or preferences
GameType=quake3
BasePath=C:\Games\Quake3
GamePath=baseq3
TexturePath=baseq3/textures
MapPath=baseq3/maps</pre>
    </div>

    <h3>Understanding the Radiant Interface</h3>
    <table>
        <thead>
            <tr>
                <th>Window</th>
                <th>Purpose</th>
                <th>Key Features</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>3D View</strong></td>
                <td>3D perspective of your map</td>
                <td>Navigation, entity placement, visual feedback</td>
            </tr>
            <tr>
                <td><strong>2D Views</strong></td>
                <td>Top, front, side orthographic views</td>
                <td>Precise brush placement and alignment</td>
            </tr>
            <tr>
                <td><strong>Texture Browser</strong></td>
                <td>Browse and apply textures</td>
                <td>Texture selection, scaling, rotation</td>
            </tr>
            <tr>
                <td><strong>Entity List</strong></td>
                <td>All entities in the map</td>
                <td>Selection, properties, organization</td>
            </tr>
        </tbody>
    </table>

    <h2>Fundamental Concepts</h2>
    
    <h3>Brushes: The Building Blocks</h3>
    <p><strong>Brushes</strong> are the basic geometric primitives used to construct Q3 maps:</p>
    
    <ul>
        <li><span class="keyword">Solid Brushes</span> - Basic geometry (walls, floors, ceilings)</li>
        <li><span class="keyword">Detail Brushes</span> - Non-structural geometry for optimization</li>
        <li><span class="keyword">Clip Brushes</span> - Invisible collision surfaces</li>
        <li><span class="keyword">Hint Brushes</span> - Guide BSP compilation</li>
    </ul>

    <h3>Brush vs Detail Classification</h3>
    <div class="example">
        <pre>// Brush classification guidelines:
// 
// STRUCTURAL (default brushes):
// - Main walls that divide space
// - Load-bearing architecture
// - Major geometry that affects visibility

// DETAIL (Ctrl+M to convert):
// - Decorative elements
// - Small architectural details  
// - Non-load-bearing geometry
// - Anything that doesn't affect room layout</pre>
    </div>

    <h3>Textures and Shaders</h3>
    <p>Q3 uses a sophisticated <strong>shader system</strong> for materials:</p>
    
    <h4>Basic Texture Types</h4>
    <table>
        <thead>
            <tr>
                <th>Type</th>
                <th>Purpose</th>
                <th>Examples</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><strong>Diffuse</strong></td>
                <td>Base color/pattern</td>
                <td>wall_concrete, floor_metal</td>
            </tr>
            <tr>
                <td><strong>Special</strong></td>
                <td>Gameplay functions</td>
                <td>nodraw, clip, trigger</td>
            </tr>
            <tr>
                <td><strong>Sky</strong></td>
                <td>Skybox rendering</td>
                <td>sky_space, sky_hell</td>
            </tr>
            <tr>
                <td><strong>Liquids</strong></td>
                <td>Water, lava, slime</td>
                <td>water_clear, lava_red</td>
            </tr>
        </tbody>
    </table>

    <h2>Entity System</h2>
    
    <h3>Essential Entities</h3>
    <p>Entities add functionality beyond basic geometry:</p>
    
    <h4>Player Spawns</h4>
    <div class="example">
        <pre>// info_player_deathmatch
// Basic spawn point for deathmatch games
Properties:
- angle: Player facing direction (0-360)
- target: Optional target entity
- spawnflags: Special spawn conditions

// info_player_start  
// Single player spawn point
// Also used as fallback spawn in MP</pre>
    </div>

    <h4>Items and Pickups</h4>
    <table>
        <thead>
            <tr>
                <th>Entity</th>
                <th>Item Type</th>
                <th>Respawn Time</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><code>weapon_*</code></td>
                <td>Weapons</td>
                <td>5-30 seconds</td>
            </tr>
            <tr>
                <td><code>ammo_*</code></td>
                <td>Ammunition</td>
                <td>40 seconds</td>
            </tr>
            <tr>
                <td><code>item_armor_*</code></td>
                <td>Armor</td>
                <td>25-35 seconds</td>
            </tr>
            <tr>
                <td><code>item_health_*</code></td>
                <td>Health</td>
                <td>15-35 seconds</td>
            </tr>
        </tbody>
    </table>

    <h3>Advanced Entities</h3>
    
    <h4>Triggers and Movers</h4>
    <div class="example">
        <pre>// trigger_hurt
// Damages players who touch it
Properties:
- dmg: Damage amount per touch
- spawnflags: 8=SLOW (damage over time)

// func_door
// Moving door entity  
Properties:
- angle: Movement direction
- speed: Movement speed
- wait: Time before auto-close
- lip: Distance to leave showing

// func_plat
// Moving platform
Properties:
- height: Platform travel distance
- speed: Movement speed</pre>
    </div>

    <h2>Level Design Principles</h2>
    
    <h3>Flow and Navigation</h3>
    <p>Good Q3 maps have excellent <strong>flow</strong> - players can move through the space naturally:</p>
    
    <ul>
        <li><strong>Clear Sight Lines:</strong> Players should see where they can go</li>
        <li><strong>Multiple Routes:</strong> Avoid single chokepoints</li>
        <li><strong>Logical Layout:</strong> Intuitive navigation without confusion</li>
        <li><strong>Vertical Elements:</strong> Use height variation for strategy</li>
    </ul>

    <h3>Scale and Proportions</h3>
    <div class="example">
        <pre>// Standard Q3 measurements (units):
// 
// Player height: 56 units
// Player width: 30 units (bounding box)
// Standard door: 128 units high, 64 units wide
// Ceiling height: 128-192 units minimum
// Corridor width: 128+ units for comfort
// Jump height: ~45 units maximum
// Step height: 16 units maximum</pre>
    </div>

    <h3>Lighting Design</h3>
    <p>Effective lighting enhances gameplay and atmosphere:</p>
    
    <h4>Light Entity Properties</h4>
    <table>
        <thead>
            <tr>
                <th>Property</th>
                <th>Function</th>
                <th>Typical Values</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><code>light</code></td>
                <td>Brightness intensity</td>
                <td>100-400</td>
            </tr>
            <tr>
                <td><code>_color</code></td>
                <td>RGB color values</td>
                <td>1.0 1.0 1.0 (white)</td>
            </tr>
            <tr>
                <td><code>radius</code></td>
                <td>Light falloff distance</td>
                <td>200-800</td>
            </tr>
            <tr>
                <td><code>spawnflags</code></td>
                <td>Special light modes</td>
                <td>1=LINEAR, 2=NO_ANGLE</td>
            </tr>
        </tbody>
    </table>

    <h2>Compilation Process</h2>
    
    <h3>The Build Pipeline</h3>
    <p>Q3 maps go through multiple compilation stages:</p>
    
    <ol>
        <li><strong>BSP:</strong> Basic geometry and visibility</li>
        <li><strong>VIS:</strong> Visibility optimization</li>
        <li><strong>LIGHT:</strong> Lighting calculations</li>
    </ol>

    <h3>Compilation Commands</h3>
    <div class="example">
        <pre>// Basic compilation (fast)
q3map2 -bsp mymap.map
q3map2 -vis -fast mymap.bsp  
q3map2 -light -fast mymap.bsp

// Full compilation (slow but highest quality)
q3map2 -bsp -meta mymap.map
q3map2 -vis mymap.bsp
q3map2 -light -bounce 8 -samples 3 mymap.bsp

// Development compilation (very fast)
q3map2 -bsp -meta mymap.map
q3map2 -vis -fast mymap.bsp
q3map2 -light -fast -bounce 2 mymap.bsp</pre>
    </div>

    <h2>Optimization Techniques</h2>
    
    <h3>Performance Considerations</h3>
    <p>Optimize your maps for smooth gameplay:</p>
    
    <ul>
        <li><strong>Use Detail Brushes:</strong> Convert non-structural geometry</li>
        <li><strong>Hint Brushes:</strong> Guide visibility calculations</li>
        <li><strong>Caulk Hidden Surfaces:</strong> Use caulk texture on unseen faces</li>
        <li><strong>Reasonable Brush Count:</strong> Keep total brushes manageable</li>
    </ul>

    <h3>Special Textures for Optimization</h3>
    <table>
        <thead>
            <tr>
                <th>Texture</th>
                <th>Purpose</th>
                <th>Usage</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td><code>caulk</code></td>
                <td>Hidden surfaces</td>
                <td>Faces players never see</td>
            </tr>
            <tr>
                <td><code>nodraw</code></td>
                <td>No rendering</td>
                <td>Completely invisible surfaces</td>
            </tr>
            <tr>
                <td><code>clip</code></td>
                <td>Invisible collision</td>
                <td>Player movement barriers</td>
            </tr>
            <tr>
                <td><code>hint</code></td>
                <td>Visibility hints</td>
                <td>Guide BSP splitting</td>
            </tr>
        </tbody>
    </table>

    <h2>Advanced Techniques</h2>
    
    <h3>Curved Surfaces</h3>
    <p>Create smooth curves using <strong>patches</strong>:</p>
    
    <div class="example">
        <pre>// Creating patches:
// 1. Select faces you want to curve
// 2. Patch → Cap Selection → Bevel
// 3. Adjust control points in patch mode
// 4. Use Patch → Insert/Delete Column/Row for detail

// Patch optimization:
// - Keep patch resolution reasonable (5x5 max usually)
// - Use LOD (Level of Detail) for distant patches
// - Consider using func_group for organization</pre>
    </div>

    <h3>Complex Geometry</h3>
    <h4>Using func_group</h4>
    <div class="example">
        <pre>// Organize related brushes with func_group:
// 1. Select multiple brushes
// 2. Right-click → func_group
// 3. Benefits:
//    - Easier selection and manipulation
//    - Better organization
//    - Can be converted to other func_* types</pre>
    </div>

    <h2>Testing and Iteration</h2>
    
    <h3>Playtesting Workflow</h3>
    <ol>
        <li><strong>Quick Compile:</strong> Test basic layout with fast settings</li>
        <li><strong>Bot Testing:</strong> Add bots to test flow and balance</li>
        <li><strong>Multiplayer Testing:</strong> Real players provide best feedback</li>
        <li><strong>Performance Testing:</strong> Check frame rates and load times</li>
    </ol>

    <h3>Common Issues and Solutions</h3>
    <table>
        <thead>
            <tr>
                <th>Problem</th>
                <th>Symptoms</th>
                <th>Solution</th>
            </tr>
        </thead>
        <tbody>
            <tr>
                <td>Leak</td>
                <td>Map won't compile</td>
                <td>Find and seal holes in geometry</td>
            </tr>
            <tr>
                <td>Low FPS</td>
                <td>Stuttery gameplay</td>
                <td>Use more detail brushes, optimize vis</td>
            </tr>
            <tr>
                <td>Dark areas</td>
                <td>Poor visibility</td>
                <td>Add more lights, increase brightness</td>
            </tr>
            <tr>
                <td>Stuck players</td>
                <td>Movement issues</td>
                <td>Check collision, fix brush alignment</td>
            </tr>
        </tbody>
    </table>

    <h2>Game Mode Considerations</h2>
    
    <h3>Deathmatch Maps</h3>
    <ul>
        <li><strong>Size:</strong> 4-8 players, compact but not cramped</li>
        <li><strong>Weapon Balance:</strong> Strategic weapon placement</li>
        <li><strong>Power Items:</strong> Central, contested areas</li>
        <li><strong>Spawns:</strong> Multiple safe spawn points</li>
    </ul>

    <h3>Team Deathmatch</h3>
    <ul>
        <li><strong>Symmetry:</strong> Balanced for both teams</li>
        <li><strong>Control Points:</strong> Key areas worth fighting for</li>
        <li><strong>Team Coordination:</strong> Space for group tactics</li>
    </ul>

    <h3>Capture the Flag</h3>
    <ul>
        <li><strong>Base Design:</strong> Defensible but not impenetrable</li>
        <li><strong>Flag Routes:</strong> Multiple paths between bases</li>
        <li><strong>Middle Ground:</strong> Neutral area with key items</li>
    </ul>

    <h2>Asset Creation</h2>
    
    <h3>Custom Textures</h3>
    <div class="example">
        <pre>// Texture specifications:
// - Power of 2 dimensions (128x128, 256x256, etc.)
// - TGA or JPG format
// - Place in /textures/mapname/ folder
// - Create shader files for special effects

// Basic shader example:
textures/mymap/water
{
    qer_editorimage textures/mymap/water.tga
    surfaceparm water
    surfaceparm trans
    cull disable
    {
        map textures/mymap/water.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        tcMod scroll 0.1 0.1
    }
}</pre>
    </div>

    <h2>Map Packaging and Distribution</h2>
    
    <h3>Creating a PK3 File</h3>
    <div class="example">
        <pre>// PK3 structure (ZIP file with .pk3 extension):
mymap.pk3
├── maps/
│   ├── mymap.bsp
│   └── mymap.arena
├── textures/
│   └── mymap/
│       ├── texture1.tga
│       └── texture2.tga
├── scripts/
│   └── mymap.shader
└── levelshots/
    └── mymap.tga</pre>
    </div>

    <h3>Arena File</h3>
    <div class="example">
        <pre>// maps/mymap.arena
{
map         "mymap"
longname    "My Awesome Map"
type        "ffa tourney"
bots        "anarki doom"
}</pre>
    </div>

    <blockquote>
        <strong>Pro Tip:</strong> Start with simple, clean geometry and focus on gameplay flow before adding complex details. A map that plays well is more important than one that looks pretty but has poor flow.
    </blockquote>

    <h2>Resources and Community</h2>
    
    <h3>Essential Tools</h3>
    <ul>
        <li><strong>NetRadiant:</strong> Modern cross-platform editor</li>
        <li><strong>Q3Map2:</strong> Advanced map compiler</li>
        <li><strong>Pakscape:</strong> PK3 file management</li>
        <li><strong>Image Editors:</strong> GIMP, Photoshop for textures</li>
    </ul>

    <h3>Learning Resources</h3>
    <ul>
        <li><strong>Quake3World:</strong> Active mapping community</li>
        <li><strong>..::LvL:</strong> High-quality map reviews</li>
        <li><strong>YouTube Tutorials:</strong> Video mapping guides</li>
        <li><strong>Discord Communities:</strong> Real-time help and feedback</li>
    </ul>

    <h2>Quick Reference Checklist</h2>
    
    <ul>
        <li>✅ Map has proper player spawns (8+ for FFA)</li>
        <li>✅ No leaks in geometry (compile successfully)</li>
        <li>✅ Adequate lighting throughout</li>
        <li>✅ Weapon and item placement balanced</li>
        <li>✅ Multiple routes between areas</li>
        <li>✅ Proper scale and proportions</li>
        <li>✅ Detail brushes used for optimization</li>
        <li>✅ Hidden surfaces use caulk texture</li>
        <li>✅ Playtested with bots and players</li>
        <li>✅ Packaged properly as PK3</li>
    </ul>
</div> 