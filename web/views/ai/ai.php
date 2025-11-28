<?php
/**
 * AI Systems Documentation
 */
$title = 'AI Systems - id Tech 3 Documentation';
$breadcrumbs = [
    '/ai' => 'AI',
    '/ai/ai' => 'AI Systems'
];
?>

<h1>AI Systems</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Artificial Intelligence systems in id Tech 3, including bot AI, pathfinding, and advanced AI behaviors.</p>
</div>

<div class="section">
    <h2>Bot AI</h2>
    <ul>
        <li><strong>Navigation:</strong> Area Awareness System (AAS) for pathfinding</li>
        <li><strong>Combat:</strong> Weapon selection and targeting</li>
        <li><strong>Tactics:</strong> Team coordination and objective-based behavior</li>
        <li><strong>Difficulty:</strong> Skill-based AI scaling</li>
    </ul>
</div>

<div class="section">
    <h2>Advanced AI Features</h2>
    <ul>
        <li><strong>GOAP:</strong> Goal-Oriented Action Planning system</li>
        <li><strong>Fuzzy Logic:</strong> Decision-making algorithms</li>
        <li><strong>Learning:</strong> Adaptive behavior patterns</li>
        <li><strong>Personality:</strong> Individual bot characteristics</li>
    </ul>
</div>

<div class="section">
    <h2>Configuration</h2>
    <div class="code-block">
        <pre><code># AI settings
seta bot_enable "1"
seta bot_minplayers "8"
seta g_spSkill "2"       // Bot skill level (1-5)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Bot Commands</h2>
    <div class="code-block">
        <pre><code># Add bots
addbot crash 2 red
addbot sarge 3 blue

# Remove bots
kick crash
bot_minplayers 0</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="goap">GOAP Implementation Details</a></li>
<li><a href="gameplay/gameplay">Gameplay Systems</a></li>
<li><a href="development/scripting">AI Scripting</a></li>
    </ul>
</div> 