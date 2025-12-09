<?php
/**
 * Service Locator & Logger (C++) Tutorial
 */
$title = 'Service Locator & Logger - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/service-logger' => 'Service Locator & Logger'
];
?>

<h1>Service Locator & Logger (C++)</h1>

<div class="section">
    <h2>Overview</h2>
    <p>A lightweight C++ service locator provides an injectable logger interface, avoiding hardcoded globals. The default logger bridges to <code>Com_Printf</code>, but you can supply your own (file, network, buffered, or test stubs).</p>
</div>

<div class="section">
    <h2>Interfaces</h2>
    <ul>
        <li><code>ILogger</code> with <code>log(LogLevel, std::string_view)</code>.</li>
        <li><code>ServiceLocator::SetLogger(ILogger*)</code> and <code>ServiceLocator::Logger()</code> for access.</li>
        <li>Built-in <code>DefaultEngineLogger</code> (forwards to <code>Com_Printf</code>) and <code>NullLogger</code>.</li>
    </ul>
</div>

<div class="section">
    <h2>Usage</h2>
    <div class="code-block">
        <pre><code>// Supply a custom logger at startup
class FileLogger : public ILogger {
public:
    void log(LogLevel level, std::string_view msg) override {
        // write to file or buffer
    }
};

static FileLogger g_fileLogger;

void InitLogging() {
    Services::ServiceLocator::SetLogger(&g_fileLogger);
}

// Log anywhere in C++ code:
Services::ServiceLocator::Logger().log(Services::LogLevel::Info, "Initialized renderer");</code></pre>
    </div>
</div>

<div class="section">
    <h2>When to Swap Loggers</h2>
    <ul>
        <li><strong>Headless/tests:</strong> inject a buffer or null logger to silence output.</li>
        <li><strong>Tools:</strong> send logs to files or sockets for analysis.</li>
        <li><strong>Runtime:</strong> keep default logger to <code>Com_Printf</code> for in-game console visibility.</li>
    </ul>
</div>

<div class="section">
    <h2>Notes</h2>
    <ul>
        <li>Keep the locator minimal; avoid turning it into a general DI container.</li>
        <li>Use thread-safe loggers if logging from worker threads.</li>
        <li>Prefer concise messages; avoid heavy formatting in hot paths.</li>
    </ul>
</div>

