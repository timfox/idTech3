<?php
/**
 * Configuration guide view
 */
$title = 'Configuration - id Tech 3 Documentation';
$breadcrumbs = [
    '/getting-started' => 'Getting Started',
    '/getting-started/configuration' => 'Configuration'
];
?>

<h1>Configuration Guide</h1>

<div class="section">
    <h2>Engine Configuration</h2>
    
    <h3>Main Configuration File</h3>
    <div class="code-block">
        <pre><code><?php include __DIR__ . '/../partials/config-example.php'; ?></code></pre>
    </div>

    <h3>Command Line Parameters</h3>
    <?php include __DIR__ . '/../partials/command-line-params.php'; ?>
</div>

<div class="section">
    <h2>Graphics Settings</h2>
    <?php include __DIR__ . '/../partials/graphics-settings-table.php'; ?>
</div> 