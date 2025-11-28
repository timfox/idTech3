<?php
/**
 * Gameplay Systems Documentation
 */
$title = 'Gameplay - id Tech 3 Documentation';
$breadcrumbs = [
    '/gameplay' => 'Gameplay',
    '/gameplay/gameplay' => 'Gameplay Systems'
];
?>

<h1>Gameplay Systems</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Core gameplay mechanics and systems in id Tech 3, including player interactions, game modes, and scripting.</p>
</div>

<div class="section">
    <h2>Game Modes</h2>
    <ul>
        <li><strong>Deathmatch:</strong> Free-for-all combat</li>
        <li><strong>Team Deathmatch:</strong> Team-based combat</li>
        <li><strong>Capture the Flag:</strong> Objective-based team gameplay</li>
        <li><strong>Tournament:</strong> 1v1 competitive matches</li>
    </ul>
</div>

<div class="section">
    <h2>Player Systems</h2>
    <ul>
        <li><strong>Health & Armor:</strong> Damage and protection systems</li>
        <li><strong>Weapons:</strong> Arsenal of projectile and hitscan weapons</li>
        <li><strong>Powerups:</strong> Temporary enhancements</li>
        <li><strong>Movement:</strong> Advanced movement mechanics</li>
    </ul>
</div>

<div class="section">
    <h2>Scripting</h2>
    <p>Gameplay behavior is controlled through:</p>
    <ul>
        <li><strong>QuakeC:</strong> Game logic scripting</li>
        <li><strong>Entity System:</strong> Map-based gameplay elements</li>
        <li><strong>Triggers:</strong> Event-based interactions</li>
    </ul>
</div>

<div class="section">
    <h2>Configuration</h2>
    <div class="code-block">
        <pre><code># Gameplay settings
seta g_gametype "0"     // Game mode
seta fraglimit "20"     // Score limit
seta timelimit "20"     // Time limit (minutes)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="development/scripting">Scripting Guide</a></li>
<li><a href="ai/ai">AI Systems</a></li>
<li><a href="physics/physics">Physics System</a></li>
    </ul>
</div> 