<?php
/**
 * QVM Improvements
 */
$title = 'QVM Improvements - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/qvm-improvements' => 'QVM Improvements'
];
?>

<h1>QVM (Quake Virtual Machine) Improvements</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The Quake Virtual Machine (QVM) has been significantly reworked to improve performance, memory management, and reliability. These improvements make mod development easier and mod execution more efficient.</p>
</div>

<div class="section">
    <h2>Performance Improvements</h2>
    
    <h3>Optimized Interpreter</h3>
    <p>The QVM interpreter has been optimized for better performance:</p>
    <ul>
        <li>Faster instruction execution</li>
        <li>Reduced overhead</li>
        <li>Better CPU cache utilization</li>
        <li>Optimized hot paths</li>
    </ul>
    
    <h3>Memory Access Optimization</h3>
    <p>Memory access patterns have been improved:</p>
    <ul>
        <li>Better memory locality</li>
        <li>Reduced cache misses</li>
        <li>Optimized data structures</li>
    </ul>
</div>

<div class="section">
    <h2>Memory Management Improvements</h2>
    
    <h3>Data Segment Reuse</h3>
    <p>The QVM now reuses rounded-up data segment space for extra program stack when possible:</p>
    <ul>
        <li>Better memory utilization</li>
        <li>Larger available stack space</li>
        <li>Reduced memory waste</li>
    </ul>
    
    <h3>Improved Allocation</h3>
    <p>Better memory allocation strategies:</p>
    <ul>
        <li>More efficient memory pools</li>
        <li>Reduced fragmentation</li>
        <li>Better cleanup</li>
    </ul>
</div>

<div class="section">
    <h2>IEEE 754 Compliance</h2>
    
    <h3>Floating Point Precision</h3>
    <p>Unsafe floating-point optimizations have been disabled for QVM modules to ensure IEEE 754 compliance:</p>
    <ul>
        <li>Proper NaN handling</li>
        <li>Correct floating-point arithmetic</li>
        <li>Predictable behavior</li>
        <li>Better cross-platform consistency</li>
    </ul>
    
    <h3>Why This Matters</h3>
    <p>IEEE 754 compliance ensures:</p>
    <ul>
        <li>Consistent floating-point behavior</li>
        <li>Proper NaN propagation</li>
        <li>Correct infinity handling</li>
        <li>Predictable results across platforms</li>
    </ul>
</div>

<div class="section">
    <h2>Reliability Improvements</h2>
    
    <h3>Better Error Handling</h3>
    <p>Improved error detection and reporting:</p>
    <ul>
        <li>More descriptive error messages</li>
        <li>Better stack trace information</li>
        <li>Improved debugging capabilities</li>
    </ul>
    
    <h3>Stability</h3>
    <p>Enhanced stability features:</p>
    <ul>
        <li>Better bounds checking</li>
        <li>Improved memory protection</li>
        <li>More robust execution</li>
    </ul>
</div>

<div class="section">
    <h2>Developer Benefits</h2>
    
    <h3>Easier Mod Development</h3>
    <ul>
        <li>Better error messages help debug issues</li>
        <li>Improved performance means more complex mods possible</li>
        <li>Better memory management reduces crashes</li>
    </ul>
    
    <h3>Better Performance</h3>
    <ul>
        <li>Faster mod execution</li>
        <li>More efficient memory usage</li>
        <li>Better scalability</li>
    </ul>
</div>

<div class="section">
    <h2>Backward Compatibility</h2>
    <p>All QVM improvements are backward compatible:</p>
    <ul>
        <li>Existing mods continue to work</li>
        <li>No changes required to mod code</li>
        <li>Automatic benefit from improvements</li>
    </ul>
</div>

<div class="section">
    <h2>Technical Details</h2>
    
    <h3>Build System Changes</h3>
    <p>The Makefile has been updated to disable unsafe FP optimizations for QVM modules:</p>
    <div class="code-block">
        <pre><code># Disable unsafe FP optimizations for QVM
# Ensures IEEE 754 compliance when handling NaNs
CFLAGS += -fno-unsafe-math-optimizations</code></pre>
    </div>
    
    <h3>Memory Layout</h3>
    <p>Improved memory layout for QVM modules:</p>
    <ul>
        <li>Better alignment</li>
        <li>More efficient packing</li>
        <li>Optimized data structures</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/virtual-machine">Virtual Machine Documentation</a> - Complete QVM reference</li>
        <li><a href="development/modding">Modding Guide</a> - Creating QVM mods</li>
        <li><a href="development/scripting">Scripting</a> - QVM scripting</li>
    </ul>
</div>

