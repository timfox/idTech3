<?php
/**
 * Quick Start Guide view
 */
$title = 'Quick Start Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/getting-started' => 'Getting Started',
    '/getting-started/quick-start' => 'Quick Start Guide'
];
?>

<h1>Quick Start Guide</h1>

<div class="section">
    <h2>Getting Up and Running</h2>
    <p>This guide will help you get id Tech 3 built and running quickly.</p>
    
    <div class="step">
        <h3>Step 1: Clone the Repository</h3>
        <div class="code-block">
            <pre><code>git clone https://github.com/yourusername/idtech3
cd idtech3</code></pre>
        </div>
    </div>

    <div class="step">
        <h3>Step 2: Build the Engine</h3>
        <?php include __DIR__ . '/../partials/build-instructions.php'; ?>
    </div>

    <div class="step">
        <h3>Step 3: Configure Settings</h3>
        <p>Copy the example configuration:</p>
        <div class="code-block">
            <pre><code>cp config/example.cfg config/quake3.cfg</code></pre>
        </div>
        
        <p>Edit <code>config/quake3.cfg</code> to set your preferred renderer:</p>
        <?php include __DIR__ . '/../partials/config-example.php'; ?>
    </div>

    <div class="step">
        <h3>Step 4: Run the Engine</h3>
        <div class="code-block">
            <pre><code>./quake3e +set fs_basepath . +set r_renderer vulkan</code></pre>
        </div>
    </div>
</div>

<div class="section">
    <h2>Next Steps</h2>
    <ul>
        <li><a href="getting-started/installation">Detailed Installation Guide</a></li>
<li><a href="getting-started/configuration">Configuration Options</a></li>
<li><a href="development/map-making">Creating Your First Map</a></li>
<li><a href="rendering/vulkan">Vulkan Renderer Features</a></li>
    </ul>
</div> 