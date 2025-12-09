<?php
/**
 * Wolfenstein: Enemy Territory Overview
 */
$title = 'Wolfenstein: Enemy Territory Overview - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/wolf-et-overview' => 'Wolfenstein: Enemy Territory'
];
?>

<h1>Wolfenstein: Enemy Territory</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Wolfenstein: Enemy Territory (W:ET) is a free, class-based multiplayer FPS built on id Tech 3. Originally planned as a Return to Castle Wolfenstein expansion, it was released as a standalone title and became a cornerstone of competitive teamplay.</p>
</div>

<div class="section">
    <h2>Core Gameplay</h2>
    <ul>
        <li><strong>Class-based:</strong> Soldier, Engineer, Medic, Field Ops, Covert Ops, each with distinct roles.</li>
        <li><strong>Objective-focused:</strong> Maps revolve around attacking/defending multi-stage objectives (plant/demolish, escort, capture).</li>
        <li><strong>Teamplay mechanics:</strong> Revives, ammo packs, airstrikes/artillery, disguise/sabotage, and team spawns.</li>
        <li><strong>Movement:</strong> Strafe-jumping and trickjumps add depth to navigation and timing.</li>
    </ul>
</div>

<div class="section">
    <h2>Engine Notes</h2>
    <ul>
        <li>Based on id Tech 3 with significant networking and gameplay extensions for objective play.</li>
        <li>Uses a modified renderer and UI tailored to RTCW/W:ET aesthetics.</li>
        <li>Relies on BSP maps with shaders, lightmaps, and scripted objectives.</li>
    </ul>
</div>

<div class="section">
    <h2>Modding and Content</h2>
    <ul>
        <li><strong>Maps:</strong> BSP pipeline with <code>q3map2</code> for lighting/vis; objectives scripted via map scripts and entity setups.</li>
        <li><strong>Shaders/Materials:</strong> Standard id Tech 3 shader system for surfaces, effects, and lightstyles.</li>
        <li><strong>Paks:</strong> Content packaged in <code>.pk3</code> archives; maintain clean shaderlists for Radiant editing.</li>
    </ul>
</div>

<div class="section">
    <h2>Tools & Resources</h2>
    <ul>
        <li><strong>GtkRadiant:</strong> Level editor for brushwork, entities, and layout (see Radiant setup guides).</li>
        <li><strong>q3map2:</strong> Compile toolchain for BSP/vis/light.</li>
        <li><strong>ET: Legacy:</strong> Active open-source fork with modernized engine and QoL improvements.</li>
    </ul>
</div>

<div class="section">
    <h2>Why It Matters</h2>
    <ul>
        <li>Showcases id Tech 3’s flexibility for objective/team-based gameplay.</li>
        <li>Influenced subsequent class-based shooters and competitive objective modes.</li>
        <li>Remains a vibrant example of community-driven support and open-source maintenance.</li>
    </ul>
</div>

