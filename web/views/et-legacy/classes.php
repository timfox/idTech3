<?php
/**
 * ET Legacy Classes Documentation
 */
$title = 'Player Classes - ET Legacy Documentation';
$breadcrumbs = [
    '/et-legacy' => 'ET Legacy',
    '/et-legacy/classes' => 'Player Classes'
];
?>

<h1>Player Classes</h1>

<div class="section">
    <h2>Overview</h2>
    <p>ET Legacy features five distinct player classes, each with unique abilities, weapons, and roles in team-based gameplay. Understanding each class is crucial for effective team coordination and objective completion.</p>
    
    <div class="feature-list">
        <h3>Class System Features</h3>
        <ul>
            <li><strong>Unique Abilities:</strong> Each class has special skills and equipment</li>
            <li><strong>Skill Progression:</strong> Gain XP to unlock enhanced abilities</li>
            <li><strong>Team Balance:</strong> Each class fills specific battlefield roles</li>
            <li><strong>Weapon Specialization:</strong> Class-specific weapon access</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Soldier</h2>
    
    <h3>Role</h3>
    <p>The <strong>Soldier</strong> is the heavy weapons specialist, excelling at destroying objectives and providing explosive firepower support.</p>
    
    <h3>Weapons</h3>
    <ul>
        <li><strong>Primary:</strong> MP40 (Axis) / Thompson (Allied)</li>
        <li><strong>Heavy Weapons:</strong> MG42 (Axis) / Browning .30 cal (Allied)</li>
        <li><strong>Explosives:</strong> Panzerfaust, Bazooka, Flamethrower</li>
        <li><strong>Grenades:</strong> Hand grenades, Rifle grenades</li>
        <li><strong>Sidearm:</strong> Luger (Axis) / Colt .45 (Allied)</li>
    </ul>
    
    <h3>Special Abilities</h3>
    <div class="code-block">
        <pre><code>// Soldier class abilities
Heavy Weapons     // Access to MG42/Browning
Explosives        // Panzerfaust, Bazooka, Flamethrower
Ammunition        // Extra ammo capacity
Rifle Grenades    // Launch grenades from rifle</code></pre>
    </div>
    
    <h3>Skill Progression (Heavy Weapons)</h3>
    <ul>
        <li><strong>Level 1 (20 XP):</strong> Improved dexterity with heavy weapons</li>
        <li><strong>Level 2 (50 XP):</strong> Heavy weapons have less recoil</li>
        <li><strong>Level 3 (90 XP):</strong> Heavy weapons overheat 50% slower</li>
        <li><strong>Level 4 (140 XP):</strong> No damage from own explosives</li>
    </ul>
    
    <h3>Tactical Role</h3>
    <ul>
        <li><strong>Objective Destruction:</strong> Use explosives to destroy primary objectives</li>
        <li><strong>Area Denial:</strong> Heavy weapons control key chokepoints</li>
        <li><strong>Support Fire:</strong> Suppress enemy positions</li>
        <li><strong>Tank Support:</strong> Escort and protect tank objectives</li>
    </ul>
</div>

<div class="section">
    <h2>Medic</h2>
    
    <h3>Role</h3>
    <p>The <strong>Medic</strong> keeps the team alive through healing and revival abilities while maintaining solid combat effectiveness.</p>
    
    <h3>Weapons</h3>
    <ul>
        <li><strong>Primary:</strong> MP40 (Axis) / Thompson (Allied)</li>
        <li><strong>Medical:</strong> Syringe, Health packs</li>
        <li><strong>Sidearm:</strong> Luger (Axis) / Colt .45 (Allied)</li>
        <li><strong>Grenades:</strong> Hand grenades</li>
    </ul>
    
    <h3>Special Abilities</h3>
    <div class="code-block">
        <pre><code>// Medic class abilities
Revive            // Revive fallen teammates with syringe
Health Packs      // Drop health packs for team healing
Self Healing      // Regenerate health over time
Spawn Protection  // Brief invulnerability when spawning</code></pre>
    </div>
    
    <h3>Skill Progression (First Aid)</h3>
    <ul>
        <li><strong>Level 1 (20 XP):</strong> Medic gets extra health (112 HP)</li>
        <li><strong>Level 2 (50 XP):</strong> Faster revive and heal time</li>
        <li><strong>Level 3 (90 XP):</strong> Revive with full health, self-heal</li>
        <li><strong>Level 4 (140 XP):</strong> Adrenaline self-revive ability</li>
    </ul>
    
    <h3>Tactical Role</h3>
    <ul>
        <li><strong>Team Support:</strong> Keep teammates alive and healthy</li>
        <li><strong>Revival:</strong> Quickly revive fallen allies</li>
        <li><strong>Frontline Combat:</strong> Engage enemies while supporting team</li>
        <li><strong>Objective Support:</strong> Heal engineers during construction</li>
    </ul>
</div>

<div class="section">
    <h2>Engineer</h2>
    
    <h3>Role</h3>
    <p>The <strong>Engineer</strong> specializes in construction, demolition, and maintaining team equipment and defenses.</p>
    
    <h3>Weapons</h3>
    <ul>
        <li><strong>Primary:</strong> MP40 (Axis) / Thompson (Allied)</li>
        <li><strong>Rifles:</strong> K43 (Axis) / Garand (Allied)</li>
        <li><strong>Equipment:</strong> Pliers, Dynamite, Landmines</li>
        <li><strong>Sidearm:</strong> Luger (Axis) / Colt .45 (Allied)</li>
        <li><strong>Grenades:</strong> Hand grenades</li>
    </ul>
    
    <h3>Special Abilities</h3>
    <div class="code-block">
        <pre><code>// Engineer class abilities
Construction      // Build bridges, walls, and objectives
Demolition        // Plant dynamite to destroy objectives
Defusing         // Disarm enemy explosives
Mine Detection   // Spot and disarm landmines
Repair           // Fix MG nests and other equipment</code></pre>
    </div>
    
    <h3>Skill Progression (Engineering)</h3>
    <ul>
        <li><strong>Level 1 (20 XP):</strong> Improved dexterity with engineer tools</li>
        <li><strong>Level 2 (50 XP):</strong> Land mines are undetectable, faster construction</li>
        <li><strong>Level 3 (90 XP):</strong> Faster defusing, can detect enemy mines</li>
        <li><strong>Level 4 (140 XP):</strong> Improved flak jacket, can upgrade MG nests</li>
    </ul>
    
    <h3>Tactical Role</h3>
    <ul>
        <li><strong>Objective Completion:</strong> Build/destroy primary objectives</li>
        <li><strong>Base Defense:</strong> Construct defensive structures</li>
        <li><strong>Mine Warfare:</strong> Deploy landmines strategically</li>
        <li><strong>Equipment Maintenance:</strong> Repair and upgrade team assets</li>
    </ul>
</div>

<div class="section">
    <h2>Field Ops</h2>
    
    <h3>Role</h3>
    <p>The <strong>Field Ops</strong> provides ammunition resupply and artillery support, serving as the team's logistical backbone.</p>
    
    <h3>Weapons</h3>
    <ul>
        <li><strong>Primary:</strong> MP40 (Axis) / Thompson (Allied)</li>
        <li><strong>Equipment:</strong> Ammo packs, Binoculars</li>
        <li><strong>Artillery:</strong> Artillery strikes, Smoke screens</li>
        <li><strong>Sidearm:</strong> Luger (Axis) / Colt .45 (Allied)</li>
        <li><strong>Grenades:</strong> Hand grenades</li>
    </ul>
    
    <h3>Special Abilities</h3>
    <div class="code-block">
        <pre><code>// Field Ops class abilities
Ammo Supply       // Drop ammo packs for teammates
Artillery Strike  // Call in artillery bombardment
Air Strike        // Request air support (some maps)
Smoke Screen      // Deploy concealing smoke
Binoculars        // Spot enemies and call strikes</code></pre>
    </div>
    
    <h3>Skill Progression (Signals)</h3>
    <ul>
        <li><strong>Level 1 (20 XP):</strong> Improved dexterity with signals equipment</li>
        <li><strong>Level 2 (50 XP):</strong> Can give ammo to teammates with full magazines</li>
        <li><strong>Level 3 (90 XP):</strong> Artillery charges 25% faster</li>
        <li><strong>Level 4 (140 XP):</strong> Two artillery charges maximum</li>
    </ul>
    
    <h3>Tactical Role</h3>
    <ul>
        <li><strong>Supply Support:</strong> Keep team supplied with ammunition</li>
        <li><strong>Artillery Coordination:</strong> Call strikes on enemy positions</li>
        <li><strong>Area Denial:</strong> Use artillery to control territory</li>
        <li><strong>Reconnaissance:</strong> Spot enemies with binoculars</li>
    </ul>
</div>

<div class="section">
    <h2>Covert Ops</h2>
    
    <h3>Role</h3>
    <p>The <strong>Covert Ops</strong> specializes in stealth, reconnaissance, and infiltration behind enemy lines.</p>
    
    <h3>Weapons</h3>
    <ul>
        <li><strong>Primary:</strong> Sten (Axis) / Sten (Allied)</li>
        <li><strong>Sniper:</strong> FG42 (Axis) / Garand w/ scope (Allied)</li>
        <li><strong>Equipment:</strong> Smoke grenades, Satchel charges</li>
        <li><strong>Sidearm:</strong> Luger (Axis) / Colt .45 (Allied)</li>
        <li><strong>Special:</strong> Uniform theft, Disguise</li>
    </ul>
    
    <h3>Special Abilities</h3>
    <div class="code-block">
        <pre><code>// Covert Ops class abilities
Disguise          // Steal enemy uniforms
Smoke Grenades    // Deploy concealing smoke
Satchel Charges   // Remote explosives
Assassination     // Instant knife kills from behind
Reconnaissance    // Enhanced awareness abilities</code></pre>
    </div>
    
    <h3>Skill Progression (Covert Operations)</h3>
    <ul>
        <li><strong>Level 1 (20 XP):</strong> Improved dexterity with covert ops tools</li>
        <li><strong>Level 2 (50 XP):</strong> Can steal enemy uniforms, extra ammunition</li>
        <li><strong>Level 3 (90 XP):</strong> Disguise is not penetrated by Binoculars</li>
        <li><strong>Level 4 (140 XP):</strong> Can kill enemies with one knife hit</li>
    </ul>
    
    <h3>Tactical Role</h3>
    <ul>
        <li><strong>Infiltration:</strong> Penetrate enemy defenses using disguise</li>
        <li><strong>Assassination:</strong> Eliminate key enemy players</li>
        <li><strong>Sabotage:</strong> Plant satchel charges on objectives</li>
        <li><strong>Reconnaissance:</strong> Gather intelligence on enemy positions</li>
    </ul>
</div>

<div class="section">
    <h2>Class Selection Strategy</h2>
    
    <h3>Team Composition</h3>
    <p>Effective teams balance classes based on objectives and map requirements:</p>
    <div class="code-block">
        <pre><code>// Typical team composition (6v6)
1 Engineer     // Essential for objectives
1-2 Medics     // Team survival
1 Field Ops    // Ammunition and artillery
1-2 Soldiers   // Heavy firepower
1 Covert Ops   // Infiltration and support</code></pre>
    </div>
    
    <h3>Map-Specific Considerations</h3>
    <ul>
        <li><strong>Goldrush:</strong> Extra engineers for tank barriers/construction</li>
        <li><strong>Oasis:</strong> More soldiers for open combat</li>
        <li><strong>Battery:</strong> Covert ops for infiltration routes</li>
        <li><strong>Radar:</strong> Field ops for artillery support</li>
    </ul>
    
    <h3>Skill Building Tips</h3>
    <ul>
        <li><strong>Focus:</strong> Concentrate on one skill tree per round</li>
        <li><strong>Teamwork:</strong> Skill actions that help teammates</li>
        <li><strong>Objectives:</strong> Prioritize objective-related skills</li>
        <li><strong>Survival:</strong> Dead players can't gain XP</li>
    </ul>
</div>

<div class="section">
    <h2>Advanced Class Tactics</h2>
    
    <h3>Multi-Class Strategies</h3>
    <div class="code-block">
        <pre><code># Class switching commands
/class s    // Switch to Soldier
/class m    // Switch to Medic  
/class e    // Switch to Engineer
/class f    // Switch to Field Ops
/class c    // Switch to Covert Ops</code></pre>
    </div>
    
    <h3>Class Combinations</h3>
    <ul>
        <li><strong>Medic + Engineer:</strong> Healing support during construction</li>
        <li><strong>Field Ops + Soldier:</strong> Ammo supply for heavy weapons</li>
        <li><strong>Covert Ops + Engineer:</strong> Disguised objective completion</li>
        <li><strong>Medic + Covert Ops:</strong> Infiltration with survival support</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="et-legacy/weapons">Weapon Systems</a></li>
        <li><a href="et-legacy/objectives">Objective Gameplay</a></li>
        <li><a href="et-legacy/teams">Team Mechanics</a></li>
        <li><a href="et-legacy/maps">Maps and Strategies</a></li>
    </ul>
</div> 