<?php
header('Content-Type: text/html; charset=utf-8');
// Set error reporting
error_reporting(E_ALL);
ini_set('display_errors', 1);

// Set up error logging
ini_set('log_errors', 1);
ini_set('error_log', __DIR__ . '/error.log');

?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <meta name="description" content="Documentation for the id Tech 3 Engine">
    <meta name="theme-color" content="#223e5b">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <title>id Tech 3 Engine Documentation</title>
    <link rel="stylesheet" href="/public/css/styles.css">
</head>
<body>
    <div class="background"></div>
    
    <div class="header">
        <h1 style="margin-bottom: -0.25em;">id Tech 3 Engine Documentation</h1>
    </div>

    <div class="search-box">
        <input type="text" id="search" placeholder="Search">
    </div>

    <button id="back-button" class="back-button" style="display: none;">← Back to Index</button>
    <div id="loading" class="loading" style="display: none;">Loading...</div>
    <div id="error-message" class="error-message" style="display: none;"></div>

    <div id="main-container">
        <div id="sections-container" class="content">
            <div class="section" style="background: rgba(0, 247, 255, 0.1); border: 2px solid #00f7ff; padding: 15px; margin-bottom: 20px; border-radius: 5px;">
                <h2 style="color: #00f7ff; margin-top: 0;">What's New</h2>
                <p>Check out the latest enhancements and features:</p>
                <ul>
                    <li><a href="whats-new">What's New</a> - Recent enhancements overview</li>
                    <li><a href="engine/advanced-systems">Advanced Engine Systems</a> - GPU culling, material system, cell streaming, and more</li>
                    <li><a href="rendering/gibs">Global Illumination (GIBS)</a> - Real-time indirect lighting with surfels</li>
                    <li><a href="rendering/directx12">DirectX 12 Renderer</a> - New Windows renderer with DXR</li>
                    <li><a href="core/structured-logging">Structured Logging</a> - Modern logging system</li>
                    <li><a href="core/memory-safety">Memory Safety Tools</a> - ASan, UBSan, memory tracking</li>
                    <li><a href="core/filesystem-v2">Virtual Filesystem v2.0</a> - Modern mount management, priority-based search, enhanced security</li>
                    <li><a href="networking/websocket">WebSocket Support</a> - Real-time bidirectional communication</li>
                    <li><a href="imgui">ImGui Debug Overlays</a> - In-engine debugging tools</li>
                </ul>
            </div>

            <div class="section">
                <h2>Overview</h2>
                <ul>
                    <li><a href="idtech3">id Tech 3 Engine Features</a></li>
                    <li><a href="features/complete-features">Complete Features List</a></li>
                    <li><a href="engine/architecture">Engine Architecture</a></li>
                    <li><a href="engine/advanced-systems">Advanced Engine Systems</a></li>
                    <li><a href="history">History of id Tech 3</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Getting Started</h2>
                <ul>
                    <li><a href="getting-started/installation">Installation</a></li>
                    <li><a href="getting-started/build-instructions">Build Instructions</a></li>
                    <li><a href="getting-started/configuration">Configuration</a></li>
                    <li><a href="getting-started/quick-start">Quick Start Guide</a></li>
                    <li><a href="configurable-naming">Configurable Naming Tutorial</a> - Brand your game/mod</li>
                </ul>
            </div>

            <div class="section">
                <h2>Tutorials</h2>
                <ul>
                    <li><a href="tutorials/c23-overview">C23 Programming Language Tutorial</a></li>
                    <li><a href="tutorials/structured-logging">Structured Logging Tutorial</a></li>
                    <li><a href="tutorials/memory-profiling">Memory Profiling Tutorial</a></li>
                    <li><a href="tutorials/imgui-overlays">ImGui Debug Overlays Tutorial</a></li>
                    <li><a href="tutorials/websocket">WebSocket Integration Tutorial</a></li>
                    <li><a href="tutorials/animated-skybox">Creating Animated Skyboxes</a></li>
                    <li><a href="tutorials/enhanced-networking">Enhanced Networking Setup</a></li>
                    <li><a href="tutorials/directx12-setup">DirectX 12 Setup Tutorial</a></li>
                    <li><a href="tutorials/custom-backgrounds">Custom Menu &amp; Console Backgrounds</a></li>
                    <li><a href="pbr_tutorial">PBR Shader Tutorial</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Development</h2>
                <ul>
                    <li><a href="development/map-making">Map Making</a></li>
                    <li><a href="development/modding">Modding</a></li>
                    <li><a href="development/scripting">Scripting</a></li>
                    <li><a href="development/debugging">Debugging</a></li>
                    <li><a href="custom_cursor">Custom Cursor Guide</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Gameplay</h2>
                <ul>
                    <li><a href="gameplay/gameplay">Gameplay Systems</a></li>
                    <li><a href="gameplay/dialog-system">Dialog System</a></li>
                    <li><a href="gameplay/inventory-system">Inventory System</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Tools</h2>
                <ul>
                    <li><a href="tools/radiant">Q3Radiant</a></li>
                    <li><a href="tools/compiler">Map Compiler</a></li>
                    <li><a href="tools/asset-tools">Asset Tools</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Core Systems</h2>
                <ul>
                    <li><a href="core/complete-core">Complete Core Systems</a></li>
                    <li><a href="core/filesystem">Filesystem</a></li>
                    <li><a href="core/filesystem-v2">Virtual Filesystem v2.0</a></li>
                    <li><a href="core/memory-management">Memory Management</a></li>
                    <li><a href="core/input-system">Input System</a></li>
                    <li><a href="core/console-system">Console System</a></li>
                    <li><a href="core/structured-logging">Structured Logging</a></li>
                    <li><a href="core/memory-safety">Memory Safety</a></li>
                    <li><a href="core/refactoring-summary">Refactoring Summary</a></li>
                    <li><a href="core/engine-subsystems">Engine Subsystems</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Rendering</h2>
                <ul>
                    <li><a href="rendering/complete-renderer">Complete Renderer Guide</a></li>
                    <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
                    <li><a href="rendering/directx12">DirectX 12 Renderer</a></li>
                    <li><a href="rendering/metal">Metal Renderer</a></li>
                    <li><a href="rendering/pbr">PBR Pipeline</a></li>
                    <li><a href="rendering/pbr-materials">PBR Material Creation</a></li>
                    <li><a href="rendering/ray-tracing">Ray Tracing</a></li>
                    <li><a href="rendering/gibs">Global Illumination (GIBS)</a></li>
                    <li><a href="rendering/video-codecs">Video Codec Support</a></li>
                    <li><a href="rendering/shaders">Shaders</a></li>
                    <li><a href="rendering/animated-skybox">Animated Skybox</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Networking</h2>
                <ul>
                    <li><a href="networking/complete-networking">Complete Networking Guide</a></li>
                    <li><a href="networking/networking">Networking</a></li>
                    <li><a href="networking/enhanced-networking">Enhanced Networking</a></li>
                    <li><a href="networking/websocket">WebSocket Support</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>UI</h2>
                <ul>
                    <li><a href="ui/ui">UI System</a></li>
                    <li><a href="ui/graphics-options">Graphics Options UI Changes</a></li>
                    <li><a href="ui/reload-ui">Reload UI Changes</a></li>
                    <li><a href="imgui">ImGui Integration</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Modernization</h2>
                <ul>
                    <li><a href="modernization/modern-cpp">Modern C++</a></li>
                    <li><a href="modernization/cpp23-migration">C++23 Migration</a></li>
                    <li><a href="modernization/build-systems">Build Systems</a></li>
                    <li><a href="modernization/ci-cd">CI/CD Pipeline</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Performance</h2>
                <ul>
                    <li><a href="performance/optimization">Optimization and Stability</a></li>
                </ul>
            </div>

            <div class="section">
                <h2>Platform</h2>
                <ul>
                    <li><a href="platform/cross-platform">Cross Platform</a></li>
                    <li><a href="platform/ios-macos">iOS and macOS Support</a></li>
                    <li><a href="platform/mobile-console">Mobile/Console Platform</a></li>
                    <li><a href="platform/threading-concurrency">Threading & Concurrency</a></li>
                </ul>
            </div>
        </div>
        
        <div id="content-container"></div>
    </div>

    <script>
        // State management
        let currentPage = null;
        let isLoading = false;

        // Get DOM elements
        const sectionsContainer = document.getElementById('sections-container');
        const contentContainer = document.getElementById('content-container');
        const backButton = document.getElementById('back-button');
        const loadingIndicator = document.getElementById('loading');
        const errorMessage = document.getElementById('error-message');

        // Show/hide sections and content
        function showContent() {
            if (sectionsContainer) sectionsContainer.style.display = 'none';
            if (contentContainer) {
                contentContainer.style.display = 'block';
                contentContainer.style.visibility = 'visible';
                contentContainer.style.opacity = '1';
            }
            if (backButton) backButton.style.display = 'block';
        }

        function showSections() {
            if (sectionsContainer) sectionsContainer.style.display = 'grid';
            if (contentContainer) contentContainer.style.display = 'none';
            if (backButton) backButton.style.display = 'none';
        }

        function setLoadingState(loading) {
            isLoading = loading;
            if (loadingIndicator) {
                loadingIndicator.style.display = loading ? 'block' : 'none';
            }
        }

        function showError(msg) {
            if (errorMessage) {
                errorMessage.textContent = msg;
                errorMessage.style.display = 'block';
                setTimeout(() => {
                    errorMessage.style.display = 'none';
                }, 5000);
            }
        }

        // Main content loading function
        async function loadPage(url) {
            // Prevent duplicate loads
            if (isLoading || currentPage === url) {
                return;
            }

            currentPage = url;
            setLoadingState(true);
            showError(''); // Clear any previous errors

            try {
                const response = await fetch(`/app/core/content-handler.php?page=${encodeURIComponent(url)}`, {
                    headers: {
                        'Accept': 'application/json',
                        'X-Requested-With': 'XMLHttpRequest'
                    }
                });

                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                }

                const contentType = response.headers.get('content-type');
                if (!contentType || !contentType.includes('application/json')) {
                    throw new Error('Invalid response format');
                }

                const data = await response.json();

                if (!data.success) {
                    throw new Error(data.error || 'Failed to load content');
                }

                if (!data.content) {
                    throw new Error('No content received');
                }

                // Set content
                if (contentContainer) {
                    contentContainer.innerHTML = data.content;
                    
                    // Show content view
                    showContent();
                    
                    // Update title
                    if (data.title) {
                        document.title = data.title;
                    }
                    
                    // Update URL without reload
                    history.pushState({ page: url, content: data.content }, data.title || '', '/' + url);
                    
                    // Scroll to top
                    window.scrollTo({ top: 0, behavior: 'smooth' });
                } else {
                    throw new Error('Content container not found');
                }

            } catch (error) {
                console.error('Error loading page:', error);
                showError(`Failed to load page: ${error.message}`);
                showSections();
                currentPage = null;
            } finally {
                setLoadingState(false);
            }
        }

        // Handle link clicks (event delegation)
        document.addEventListener('click', (e) => {
            const link = e.target.closest('a');
            if (!link || !link.hasAttribute('href')) return;

            const href = link.getAttribute('href');
            
            // Skip external links, anchors, and special protocols
            if (href.startsWith('http') || href.startsWith('mailto:') || href.startsWith('#')) {
                return;
            }

            e.preventDefault();
            loadPage(href);
        });

        // Back button handler
        if (backButton) {
            backButton.addEventListener('click', () => {
                showSections();
                document.title = 'id Tech 3 Engine Documentation';
                history.pushState(null, '', '/');
                currentPage = null;
            });
        }

        // Handle browser back/forward
        window.addEventListener('popstate', (e) => {
            if (e.state && e.state.content && contentContainer) {
                contentContainer.innerHTML = e.state.content;
                showContent();
                if (e.state.title) {
                    document.title = e.state.title;
                }
                currentPage = e.state.page;
            } else {
                showSections();
                document.title = 'id Tech 3 Engine Documentation';
                currentPage = null;
            }
        });

        // Handle direct URL loads (when page is refreshed or opened directly)
        function handleDirectUrl() {
            const path = window.location.pathname;
            if (path && path !== '/' && path !== '') {
                const cleanPath = path.substring(1); // Remove leading slash
                if (cleanPath) {
                    loadPage(cleanPath);
                }
            }
        }

        // Search functionality
        const searchInput = document.getElementById('search');
        if (searchInput) {
            searchInput.addEventListener('input', (e) => {
                const term = e.target.value.toLowerCase();
                const sections = document.querySelectorAll('#sections-container .section');
                
                sections.forEach(section => {
                    const links = section.querySelectorAll('a');
                    let hasMatch = false;
                    
                    links.forEach(link => {
                        const text = link.textContent.toLowerCase();
                        const parent = link.parentElement;
                        if (text.includes(term)) {
                            hasMatch = true;
                            if (parent) parent.style.display = 'block';
                        } else {
                            if (parent) parent.style.display = 'none';
                        }
                    });
                    
                    section.style.display = (hasMatch || term === '') ? 'block' : 'none';
                });
            });
        }

        // Initialize: handle direct URL if present
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', handleDirectUrl);
        } else {
            handleDirectUrl();
        }
    </script>
</body>
</html>
