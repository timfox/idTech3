<?php
/**
 * ET Legacy Objectives Documentation
 */
$title = 'Objective Gameplay - ET Legacy Documentation';
$breadcrumbs = [
    '/et-legacy' => 'ET Legacy',
    '/et-legacy/objectives' => 'Objective Gameplay'
];
?>

<h1>Objective Gameplay</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Objective-based gameplay is the heart of ET Legacy, where teams must complete specific goals rather than simply eliminating opponents. This asymmetric warfare requires coordination, strategy, and specialized class roles to succeed.</p>
    
    <div class="feature-list">
        <h3>Objective Features</h3>
        <ul>
            <li><strong>Asymmetric Gameplay:</strong> Different goals for attacking and defending teams</li>
            <li><strong>Time Pressure:</strong> Limited time to complete objectives</li>
            <li><strong>Class Requirements:</strong> Specific classes needed for different objectives</li>
            <li><strong>Strategic Depth:</strong> Multiple approaches and tactical choices</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Objective Types</h2>
    
    <h3>Destroy Objectives</h3>
    <p>Primary objectives that must be destroyed using explosives:</p>
    <div class="code-block">
        <pre><code>// Dynamite destruction
Class Required: Engineer
Explosives: Dynamite only
Plant Time: 2.5 seconds
Timer: 30 seconds
Defuse Time: 7.5 seconds (continuous)
Defense: Any class can defuse

// Satchel destruction (alternative)
Class Required: Covert Ops
Explosives: Satchel charge
Plant Time: 1.5 seconds
Detonation: Remote (manual)
Defense: Difficult to defuse</code></pre>
    </div>
    
    <h3>Construct Objectives</h3>
    <p>Build structures or repair damaged equipment:</p>
    <div class="code-block">
        <pre><code>// Construction mechanics
Class Required: Engineer
Tool Required: Pliers
Build Time: Variable (5-30 seconds)
Progress: Continuous interaction required
Destruction: Explosive damage only
Team Support: Multiple engineers = faster build</code></pre>
    </div>
    
    <h3>Escort Objectives</h3>
    <p>Move vehicles or equipment to designated areas:</p>
    <div class="code-block">
        <pre><code>// Tank escort example
Activation: Engineer constructs tank
Movement: Automatic when path is clear
Speed: Slow, vulnerable to explosives
Stopping: Enemy presence or obstacles
Repair: Engineer required for damage</code></pre>
    </div>
    
    <h3>Steal/Deliver Objectives</h3>
    <p>Retrieve items and deliver them to specific locations:</p>
    <div class="code-block">
        <pre><code>// Document theft example
Class Required: Any (usually Covert Ops)
Pickup Time: 2 seconds
Carrying: Player moves slower
Drop on Death: Yes (retrievable)
Delivery: Must reach specific location</code></pre>
    </div>
</div>

<div class="section">
    <h2>Map Progression System</h2>
    
    <h3>Sequential Objectives</h3>
    <p>Most maps feature multiple objectives that must be completed in order:</p>
    <div class="code-block">
        <pre><code>// Goldrush objective sequence
Objective 1: Construct tank (Allied Engineer)
Objective 2: Escort tank to first barrier
Objective 3: Destroy tank barrier (Allied Engineer) 
Objective 4: Escort tank to second barrier
Objective 5: Destroy second barrier (Allied Engineer)
Objective 6: Escort tank to gold room
Objective 7: Steal gold bars (Allied any class)
Objective 8: Deliver gold to truck (Allied)</code></pre>
    </div>
    
    <h3>Parallel Objectives</h3>
    <p>Some objectives can be completed simultaneously:</p>
    <div class="code-block">
        <pre><code>// Oasis parallel objectives
Primary: Destroy Old City Wall (Engineer)
Secondary: Capture Forward Spawn (any class)
Optional: Destroy Anti-Aircraft Gun (Engineer)

// Benefits of parallel completion
- Faster overall time
- Multiple attack routes
- Divided enemy attention</code></pre>
    </div>
</div>

<div class="section">
    <h2>Major Map Objectives</h2>
    
    <h3>Goldrush</h3>
    <div class="code-block">
        <pre><code>// Allied Objectives
1. Build tank at Allied spawn
2. Escort tank to first tank barrier
3. Destroy first tank barrier with dynamite
4. Escort tank to second tank barrier  
5. Destroy second tank barrier with dynamite
6. Escort tank to gold room entrance
7. Steal both gold crates from storage
8. Load gold into truck at Allied spawn

// Axis Defense Strategy
- Use landmines on tank route
- Set up machine gun nests at barriers
- Guard gold room with multiple classes
- Coordinate timed counterattacks</code></pre>
    </div>
    
    <h3>Oasis</h3>
    <div class="code-block">
        <pre><code>// Allied Objectives  
1. Capture forward bunker spawn
2. Destroy Old City Wall with dynamite
3. Destroy two anti-aircraft guns
4. Steal war documents from villa
5. Transmit documents at radio

// Axis Defense Strategy
- Hold forward bunker at all costs
- Mine the wall approach routes
- Position snipers overlooking objectives
- Quick response team for document carrier</code></pre>
    </div>
    
    <h3>Battery</h3>
    <div class="code-block">
        <pre><code>// Allied Objectives
1. Destroy main entrance gate
2. Destroy side entrance wall  
3. Destroy two 88mm gun controls
4. Escort allied tank to compound
5. Destroy gun breech with tank

// Axis Defense Strategy
- Fortify both entrances heavily
- Use artillery strikes on tank route
- Coordinate between gun positions
- Maintain communication networks</code></pre>
    </div>
</div>

<div class="section">
    <h2>Class Roles in Objectives</h2>
    
    <h3>Engineer Role</h3>
    <div class="code-block">
        <pre><code>// Critical Engineer tasks
Primary Explosives: Plant/defuse dynamite
Construction: Build objectives and defenses
Repair: Fix damaged equipment
Mine Warfare: Deploy and clear landmines
Defusing: Disarm enemy explosives

// Engineer priorities
1. Complete primary objectives first
2. Build defensive structures when possible
3. Clear enemy mines from routes
4. Support team with ammunition</code></pre>
    </div>
    
    <h3>Support Classes</h3>
    <div class="code-block">
        <pre><code>// Medic support for objectives
- Keep engineers alive during construction
- Provide spawn protection during advances  
- Quick revives in combat zones
- Health support for objective carriers

// Field Ops support
- Ammunition for engineers and soldiers
- Artillery strikes on defensive positions
- Smoke screens for objective approaches
- Communication and coordination

// Soldier support  
- Clear defensive positions with explosives
- Provide heavy fire support
- Destroy enemy equipment
- Suppress enemy positions during objectives</code></pre>
    </div>
    
    <h3>Covert Ops Objectives</h3>
    <div class="code-block">
        <pre><code>// Specialized Covert Ops tasks
Infiltration: Use disguises to bypass defenses
Alternative Explosives: Satchel charges for objectives
Assassination: Eliminate key enemy players
Intelligence: Steal documents and intelligence
Sabotage: Destroy equipment behind enemy lines

// Covert Ops strategies
- Disguise as enemy to plant explosives
- Use smoke grenades for team advances
- Assassinate enemy engineers
- Provide reconnaissance information</code></pre>
    </div>
</div>

<div class="section">
    <h2>Defensive Strategies</h2>
    
    <h3>Layered Defense</h3>
    <div class="code-block">
        <pre><code>// Defense in depth
Layer 1: Outer perimeter and spawn areas
Layer 2: Approach routes and choke points  
Layer 3: Objective areas and fallback positions
Layer 4: Final objectives and last stands

// Resource allocation
- Spread classes across all layers
- Concentrate engineers near objectives
- Position field ops for artillery coverage
- Mobile response teams for breaches</code></pre>
    </div>
    
    <h3>Area Denial</h3>
    <div class="code-block">
        <pre><code>// Defensive tools
Landmines: Block vehicle and infantry routes
MG Nests: Control key sight lines
Artillery: Suppress enemy advances
Grenades: Clear confined spaces
Crossfire: Overlapping fields of fire

// Timing considerations
- Repair damaged defenses quickly
- Rotate defensive positions
- Coordinate counterattacks
- Maintain communication</code></pre>
    </div>
</div>

<div class="section">
    <h2>Offensive Strategies</h2>
    
    <h3>Breakthrough Tactics</h3>
    <div class="code-block">
        <pre><code>// Coordinated assault phases
Phase 1: Reconnaissance and preparation
Phase 2: Suppression and smoke deployment
Phase 3: Engineer advance with escort
Phase 4: Objective completion and consolidation

// Timing and coordination
- Synchronize class abilities
- Use artillery and smoke together
- Protect engineers during plant time
- Maintain pressure on multiple fronts</code></pre>
    </div>
    
    <h3>Alternative Routes</h3>
    <div class="code-block">
        <pre><code>// Multi-pronged attacks
Primary Route: Main assault with most players
Secondary Route: Flanking force or distraction
Covert Route: Infiltration by disguised players
Fallback Route: Alternative if primary fails

// Benefits of multiple routes
- Divides enemy attention and resources
- Provides backup options for objectives
- Creates confusion and uncertainty
- Allows for tactical flexibility</code></pre>
    </div>
</div>

<div class="section">
    <h2>Spawn System and Objectives</h2>
    
    <h3>Forward Spawns</h3>
    <div class="code-block">
        <pre><code>// Spawn control objectives
Capture Mechanism: Stand near flag for 10-20 seconds
Control Requirements: No enemies in capture zone
Spawn Benefits: Reduced travel time to objectives
Strategic Value: Critical for offensive momentum

// Spawn priority
1. Secure forward spawns first when possible
2. Defend captured spawns with multiple classes
3. Use spawn advantage for rapid objective completion
4. Deny enemy spawn opportunities</code></pre>
    </div>
    
    <h3>Mobile Spawn Points</h3>
    <div class="code-block">
        <pre><code>// Vehicle spawns
Construction: Engineers build mobile spawn
Vulnerability: Destroyable by explosives
Positioning: Strategic placement near objectives
Duration: Usually temporary or limited use

// Tactical considerations
- Protect mobile spawns with defenses
- Position for maximum objective coverage
- Coordinate construction with team advances
- Plan for spawn destruction contingencies</code></pre>
    </div>
</div>

<div class="section">
    <h2>Timing and Match Flow</h2>
    
    <h3>Time Management</h3>
    <div class="code-block">
        <pre><code>// Standard match timers
Campaign Mode: 20-30 minutes per map
Stopwatch Mode: Attack/defend time swaps
Tournament: Multiple rounds with time limits

// Time allocation strategy
Early Game: 30% - Secure spawns and routes
Mid Game: 50% - Complete primary objectives  
Late Game: 20% - Final push and completion

// Pressure considerations
- Increase aggression as time decreases
- Plan for overtime scenarios
- Save special abilities for critical moments
- Coordinate final pushes</code></pre>
    </div>
    
    <h3>Match Phases</h3>
    <div class="code-block">
        <pre><code>// Opening phase (0-5 minutes)
- Establish forward positions
- Begin construction projects
- Set up defensive perimeters
- Scout enemy positions

// Middle phase (5-15 minutes)  
- Execute primary strategies
- Complete early objectives
- Adapt to enemy tactics
- Maintain map control

// Closing phase (15+ minutes)
- Final objective pushes
- All-or-nothing strategies
- Use stored abilities
- Coordinate entire team</code></pre>
    </div>
</div>

<div class="section">
    <h2>Communication and Coordination</h2>
    
    <h3>Essential Callouts</h3>
    <div class="code-block">
        <pre><code># Team communication commands
say_team "Need engineer at [location]"
say_team "Objective armed/defused"
say_team "Enemy incoming [direction]"
say_team "Need medic/ammo at [location]"
say_team "Smoke/artillery ready"

# Quick voice commands
v-s-y: "Yes"
v-s-n: "No" 
v-c-h: "I need a medic"
v-c-a: "I need ammo"
v-f-f: "Fire in the hole!"</code></pre>
    </div>
    
    <h3>Information Sharing</h3>
    <ul>
        <li><strong>Enemy Positions:</strong> Report sniper and MG positions</li>
        <li><strong>Objective Status:</strong> Communicate progress and setbacks</li>
        <li><strong>Resource Needs:</strong> Request support and supplies</li>
        <li><strong>Tactical Changes:</strong> Adapt strategies based on situation</li>
    </ul>
</div>

<div class="section">
    <h2>Objective Configuration</h2>
    
    <h3>Server Settings</h3>
    <div class="code-block">
        <pre><code># Objective-related server variables
set g_gametype "6"               // Objective mode
set timelimit "20"               // Match time in minutes
set g_campaign "campaign1"       // Campaign selection
set g_campaignFile "legacy.campaign" // Campaign definition file

# Balance settings
set g_constructibleTime "30"    // Construction time modifier
set g_explosiveTime "30"        // Dynamite timer
set g_defuseTime "7.5"         // Defuse time required</code></pre>
    </div>
    
    <h3>Objective Customization</h3>
    <div class="code-block">
        <pre><code># Modify objective behavior
set g_fastres "0"                // Fast revive disabled
set g_weaponRespawn "30"         // Weapon respawn time
set g_medicRespawn "30"          // Medic respawn time
set g_spawnprot "1"              // Spawn protection enabled

# Experience and skills
set g_maxXP "0"                  // No XP cap
set g_skillSeed "0"              // Skill progression</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="et-legacy/classes">Player Classes</a></li>
        <li><a href="et-legacy/weapons">Weapon Systems</a></li>
        <li><a href="et-legacy/maps">Maps and Campaigns</a></li>
        <li><a href="et-legacy/teams">Team Mechanics</a></li>
    </ul>
</div> 