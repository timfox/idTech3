<?php
/**
 * ET Legacy Weapons Documentation
 */
$title = 'Weapon Systems - ET Legacy Documentation';
$breadcrumbs = [
    '/et-legacy' => 'ET Legacy',
    '/et-legacy/weapons' => 'Weapon Systems'
];
?>

<h1>Weapon Systems</h1>

<div class="section">
    <h2>Overview</h2>
    <p>ET Legacy features an extensive weapons system with faction-specific weapons, class restrictions, and skill-based progression. Understanding weapon mechanics, damage models, and tactical applications is crucial for effective gameplay.</p>
    
    <div class="feature-list">
        <h3>Weapon System Features</h3>
        <ul>
            <li><strong>Class-Based Access:</strong> Weapons restricted by player class</li>
            <li><strong>Faction Variants:</strong> Different weapons for Axis and Allied forces</li>
            <li><strong>Skill Progression:</strong> Weapon effectiveness improves with experience</li>
            <li><strong>Damage Models:</strong> Realistic ballistics and armor penetration</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Sidearms</h2>
    
    <h3>Axis Sidearms</h3>
    <div class="code-block">
        <pre><code>// Luger P08 statistics
Damage: 18 (body), 36 (head)
Magazine: 8 rounds
Reload Time: 1.5 seconds
Rate of Fire: 400 RPM
Effective Range: 25 meters
Available to: All Axis classes</code></pre>
    </div>
    
    <h3>Allied Sidearms</h3>
    <div class="code-block">
        <pre><code>// Colt .45 statistics  
Damage: 18 (body), 36 (head)
Magazine: 7 rounds
Reload Time: 1.5 seconds
Rate of Fire: 380 RPM
Effective Range: 25 meters
Available to: All Allied classes</code></pre>
    </div>
    
    <h3>Sidearm Tactics</h3>
    <ul>
        <li><strong>Last Resort:</strong> Switch when primary weapon runs dry</li>
        <li><strong>Close Quarters:</strong> Effective in tight spaces</li>
        <li><strong>Silent Movement:</strong> Quieter than primary weapons</li>
        <li><strong>Quick Draw:</strong> Faster weapon switching than reloading</li>
    </ul>
</div>

<div class="section">
    <h2>Submachine Guns</h2>
    
    <h3>MP40 (Axis)</h3>
    <div class="code-block">
        <pre><code>// MP40 Maschinenpistole statistics
Damage: 14 (body), 28 (head)
Magazine: 32 rounds
Reload Time: 2.1 seconds
Rate of Fire: 500 RPM
Effective Range: 30 meters
Spread: Medium
Available to: Soldier, Medic, Engineer, Field Ops</code></pre>
    </div>
    
    <h3>Thompson M1A1 (Allied)</h3>
    <div class="code-block">
        <pre><code>// Thompson submachine gun statistics
Damage: 14 (body), 28 (head)
Magazine: 30 rounds
Reload Time: 2.1 seconds  
Rate of Fire: 600 RPM
Effective Range: 30 meters
Spread: Medium
Available to: Soldier, Medic, Engineer, Field Ops</code></pre>
    </div>
    
    <h3>Sten Gun</h3>
    <div class="code-block">
        <pre><code>// Sten Gun statistics (Covert Ops)
Damage: 14 (body), 28 (head)
Magazine: 32 rounds
Reload Time: 1.8 seconds
Rate of Fire: 550 RPM
Effective Range: 25 meters
Spread: High
Available to: Covert Ops (both factions)
Special: Silenced operation</code></pre>
    </div>
</div>

<div class="section">
    <h2>Rifles</h2>
    
    <h3>Karabiner 43 (Axis)</h3>
    <div class="code-block">
        <pre><code>// K43 semi-automatic rifle statistics
Damage: 34 (body), 68 (head)
Magazine: 10 rounds
Reload Time: 2.4 seconds
Rate of Fire: 300 RPM (semi-auto)
Effective Range: 80 meters
Spread: Low
Available to: Engineer
Special: High accuracy, scope available</code></pre>
    </div>
    
    <h3>M1 Garand (Allied)</h3>
    <div class="code-block">
        <pre><code>// M1 Garand statistics
Damage: 34 (body), 68 (head)
Magazine: 8 rounds (en-bloc clip)
Reload Time: 2.4 seconds
Rate of Fire: 350 RPM (semi-auto)
Effective Range: 80 meters
Spread: Low
Available to: Engineer
Special: "Ping" sound on empty clip</code></pre>
    </div>
    
    <h3>Scoped Variants</h3>
    <ul>
        <li><strong>K43 with Scope:</strong> 4x magnification, enhanced long-range accuracy</li>
        <li><strong>Garand with Scope:</strong> 4x magnification, sniper configuration</li>
        <li><strong>FG42 with Scope:</strong> 4x magnification (Covert Ops only)</li>
    </ul>
</div>

<div class="section">
    <h2>Heavy Weapons</h2>
    
    <h3>MG42 (Axis)</h3>
    <div class="code-block">
        <pre><code>// MG42 machine gun statistics
Damage: 18 (body), 36 (head)
Magazine: 250 rounds (belt-fed)
Reload Time: 4.5 seconds
Rate of Fire: 950 RPM
Effective Range: 60 meters
Spread: Variable (increases with sustained fire)
Available to: Soldier only
Special: Deployable bipod, overheating mechanic</code></pre>
    </div>
    
    <h3>Browning .30 Cal (Allied)</h3>
    <div class="code-block">
        <pre><code>// Browning .30 caliber statistics
Damage: 18 (body), 36 (head)
Magazine: 250 rounds (belt-fed)
Reload Time: 4.5 seconds
Rate of Fire: 850 RPM
Effective Range: 60 meters
Spread: Variable (increases with sustained fire)
Available to: Soldier only
Special: Deployable bipod, overheating mechanic</code></pre>
    </div>
    
    <h3>Heavy Weapon Mechanics</h3>
    <ul>
        <li><strong>Deployment:</strong> Must deploy bipod for accurate fire</li>
        <li><strong>Overheating:</strong> Extended fire causes weapon to overheat</li>
        <li><strong>Suppression:</strong> Effective for area denial and suppression</li>
        <li><strong>Mobility:</strong> Slower movement speed when equipped</li>
    </ul>
</div>

<div class="section">
    <h2>Explosive Weapons</h2>
    
    <h3>Panzerfaust (Axis)</h3>
    <div class="code-block">
        <pre><code>// Panzerfaust statistics
Damage: 300 (direct hit), 200-50 (splash)
Ammunition: 4 rockets maximum
Reload Time: 3.0 seconds
Velocity: Fast projectile
Splash Radius: 5 meters
Available to: Soldier only
Special: Anti-tank, high splash damage</code></pre>
    </div>
    
    <h3>Bazooka (Allied)</h3>
    <div class="code-block">
        <pre><code>// M1A1 Bazooka statistics
Damage: 300 (direct hit), 200-50 (splash)
Ammunition: 4 rockets maximum
Reload Time: 3.0 seconds
Velocity: Medium projectile
Splash Radius: 5 meters
Available to: Soldier only
Special: Anti-tank, arcing trajectory</code></pre>
    </div>
    
    <h3>Flamethrower</h3>
    <div class="code-block">
        <pre><code>// Flamethrower statistics
Damage: 12 per tick (damage over time)
Fuel: 200 units
Rate of Fire: Continuous stream
Effective Range: 8 meters
Available to: Soldier only
Special: Area effect, damage over time, fuel consumption</code></pre>
    </div>
</div>

<div class="section">
    <h2>Grenades and Explosives</h2>
    
    <h3>Hand Grenades</h3>
    <div class="code-block">
        <pre><code>// Hand grenade statistics
Damage: 300 (direct), 200-25 (splash)
Fuse Timer: 4 seconds
Splash Radius: 8 meters
Capacity: 4 grenades maximum
Available to: All classes except Medic
Special: Cookable fuse, bounce physics</code></pre>
    </div>
    
    <h3>Rifle Grenades</h3>
    <div class="code-block">
        <pre><code>// Rifle grenade statistics
Damage: 250 (direct), 150-25 (splash)
Range: 60 meters maximum
Ammunition: 4 grenades maximum
Available to: Soldier only
Special: Launched from primary weapon, arcing trajectory</code></pre>
    </div>
    
    <h3>Dynamite</h3>
    <div class="code-block">
        <pre><code>// Dynamite statistics
Damage: 1000+ (structure destruction)
Timer: 30 seconds
Defuse Time: 7.5 seconds
Available to: Engineer only
Special: Objective destruction, player-planted</code></pre>
    </div>
    
    <h3>Satchel Charges</h3>
    <div class="code-block">
        <pre><code>// Satchel charge statistics
Damage: 800 (direct), 400-50 (splash)
Detonation: Remote controlled
Capacity: 1 charge maximum
Available to: Covert Ops only
Special: Remote detonation, stealth deployment</code></pre>
    </div>
</div>

<div class="section">
    <h2>Weapon Modifications</h2>
    
    <h3>Skill-Based Improvements</h3>
    <div class="code-block">
        <pre><code>// Light Weapons skill progression
Level 1: Improved weapon handling
Level 2: Faster reload times
Level 3: Improved accuracy and less recoil
Level 4: Dual pistols (akimbo)

// Heavy Weapons skill progression  
Level 1: Improved weapon handling
Level 2: Reduced recoil and improved accuracy
Level 3: Reduced heat buildup and faster cooling
Level 4: No self-damage from explosives</code></pre>
    </div>
    
    <h3>Weapon Accessories</h3>
    <ul>
        <li><strong>Scopes:</strong> 4x magnification for rifles</li>
        <li><strong>Bipods:</strong> Deployable stability for machine guns</li>
        <li><strong>Bayonets:</strong> Melee attachment for rifles</li>
        <li><strong>Silencers:</strong> Sound suppression (Sten gun)</li>
    </ul>
</div>

<div class="section">
    <h2>Ammunition System</h2>
    
    <h3>Ammunition Types</h3>
    <div class="code-block">
        <pre><code>// Ammunition capacities by weapon class
Pistol Ammunition: 32 rounds maximum
SMG Ammunition: 120 rounds maximum  
Rifle Ammunition: 40 rounds maximum
MG Ammunition: 500 rounds maximum
Rocket Ammunition: 4 rockets maximum
Grenade Ammunition: 4 grenades maximum</code></pre>
    </div>
    
    <h3>Ammunition Sources</h3>
    <ul>
        <li><strong>Spawn:</strong> Start with partial ammunition load</li>
        <li><strong>Ammo Cabinets:</strong> Fixed resupply points on maps</li>
        <li><strong>Field Ops:</strong> Dropped ammo packs from Field Ops players</li>
        <li><strong>Enemy Weapons:</strong> Pick up weapons from eliminated enemies</li>
    </ul>
</div>

<div class="section">
    <h2>Weapon Tactics and Strategy</h2>
    
    <h3>Range Engagement</h3>
    <ul>
        <li><strong>Close Range (0-15m):</strong> SMGs, shotguns, pistols</li>
        <li><strong>Medium Range (15-40m):</strong> Assault rifles, SMGs</li>
        <li><strong>Long Range (40m+):</strong> Scoped rifles, machine guns</li>
        <li><strong>Explosive Range:</strong> Situational based on target type</li>
    </ul>
    
    <h3>Class-Specific Strategies</h3>
    <div class="code-block">
        <pre><code>// Weapon selection by role
Assault: SMG + grenades for close combat
Support: MG for area denial and suppression
Precision: Scoped rifle for long-range elimination
Demolition: Explosives for objective destruction
Stealth: Silenced weapons for covert operations</code></pre>
    </div>
    
    <h3>Team Coordination</h3>
    <ul>
        <li><strong>Suppression:</strong> Machine gunners provide covering fire</li>
        <li><strong>Overwatch:</strong> Snipers cover advancing teammates</li>
        <li><strong>Breaching:</strong> Explosives clear defensive positions</li>
        <li><strong>Support:</strong> Field Ops provide ammunition resupply</li>
    </ul>
</div>

<div class="section">
    <h2>Weapon Balancing</h2>
    
    <h3>Damage Model</h3>
    <div class="code-block">
        <pre><code>// Damage multipliers by body part
Head: 2.0x damage
Torso: 1.0x damage  
Arms: 0.8x damage
Legs: 0.6x damage

// Armor considerations
Helmet: Reduces headshot damage by 25%
Flak Jacket: Reduces explosive damage by 50%</code></pre>
    </div>
    
    <h3>Network Compensation</h3>
    <ul>
        <li><strong>Hit Registration:</strong> Client-side hit detection with server validation</li>
        <li><strong>Lag Compensation:</strong> Backward reconciliation for fair hit detection</li>
        <li><strong>Prediction:</strong> Local weapon effects with server correction</li>
        <li><strong>Anti-Cheat:</strong> Server-side validation of weapon behavior</li>
    </ul>
</div>

<div class="section">
    <h2>Weapon Configuration</h2>
    
    <h3>Weapon Settings</h3>
    <div class="code-block">
        <pre><code># Weapon-related console variables
seta cg_drawGun "1"              // Draw weapon model
seta cg_gunFrame "0"             // Weapon animation frame
seta cg_weaponCycleDelay "150"   // Weapon switching delay
seta cg_autoReload "1"           // Automatic reload when empty
seta cg_weapons "7"              // Weapon list display</code></pre>
    </div>
    
    <h3>Visual Customization</h3>
    <div class="code-block">
        <pre><code># Weapon visual settings
seta cg_muzzleFlash "1"          // Enable muzzle flash effects
seta cg_tracers "1"              // Show bullet tracers
seta cg_shells "1"               // Ejected shell casings
seta cg_weaponBob "1"            // Weapon bobbing animation</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="et-legacy/classes">Player Classes</a></li>
        <li><a href="et-legacy/objectives">Objective Gameplay</a></li>
        <li><a href="et-legacy/teams">Team Mechanics</a></li>
        <li><a href="et-legacy/maps">Maps and Strategies</a></li>
    </ul>
</div> 