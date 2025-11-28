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

    <button id="back-button" class="back-button">← Back to Index</button>
    <div id="loading" class="loading">Loading...</div>
    <div id="error-message" class="error-message"></div>

    <div id="main-container">
        <div id="sections-container" class="content">
            <div class="section">
                <h2>Getting Started</h2>
                                 <ul>
                     <li><a href="getting-started/installation">Installation</a></li>
                     <li><a href="getting-started/configuration">Configuration</a></li>
                     <li><a href="getting-started/quick-start">Quick Start Guide</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Development</h2>
                                 <ul>
                     <li><a href="development/map-making">Map Making</a></li>
                     <li><a href="development/modding">Modding</a></li>
                     <li><a href="development/scripting">Scripting</a></li>
                     <li><a href="development/debugging">Debugging</a></li>
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
                <h2>Rendering</h2>
                                 <ul>
                     <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
                     <li><a href="rendering/pbr">PBR</a></li>
                     <li><a href="rendering/global-illumination">Global Illumination</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Physics</h2>
                                 <ul>
                     <li><a href="physics/physics">Physics</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Networking</h2>
                                 <ul>
                     <li><a href="networking/networking">Networking</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Sound</h2>
                                 <ul>
                     <li><a href="sound/sound">Sound</a></li>
                 </ul>
            </div>
            
            <div class="section">
                <h2>Gameplay</h2>
                                 <ul>
                     <li><a href="gameplay/gameplay">Gameplay</a></li>
                 </ul>
            </div>
            
            <div class="section">
                <h2>AI</h2>
                                 <ul>
                     <li><a href="ai/ai">AI</a></li>
                     <li><a href="ai/pathfinding">Pathfinding</a></li>
                     <li><a href="ai/behavior-trees">Behavior Trees</a></li>
                     <li><a href="ai/goap">GOAP</a></li>
                 </ul>
            </div>
            
            <div class="section">
                <h2>Animation</h2>
                                 <ul>
                     <li><a href="animation/animation">Animation</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>UI</h2>
                                 <ul>
                     <li><a href="ui/ui">UI</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>External Libraries</h2>
                                 <ul>
                     <li><a href="external/libraries">Libraries Overview</a></li>
                     <li><a href="external/imgui-integration">ImGui Integration (C++)</a></li>
                     <li><a href="external/cimgui-quake3e">CimGui + Quake3e Walkthrough</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Core Engine</h2>
                                 <ul>
                     <li><a href="core/engine-subsystems">Engine Subsystems</a></li>
                     <li><a href="core/main-loop">Main Loop Analysis</a></li>
                     <li><a href="core/memory-management">Memory Management</a></li>
                     <li><a href="core/entity-system">Entity System</a></li>
                     <li><a href="core/console-system">Console System</a></li>
                     <li><a href="core/input-system">Input System</a></li>
                     <li><a href="core/filesystem">File System</a></li>
                     <li><a href="core/virtual-machine">Virtual Machine (QVM)</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Platform and Deployment</h2>
                                 <ul>
                     <li><a href="platform/cross-platform">Cross-Platform Development</a></li>
                     <li><a href="platform/threading-concurrency">Threading and Concurrency</a></li>
                     <li><a href="platform/mobile-console">Mobile and Console Ports</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Renderer Deep Dive</h2>
                                 <ul>
                     <li><a href="renderer/vulkan-implementation">Vulkan Renderer</a></li>
                     <li><a href="renderer/pbr-pipeline">PBR Pipeline</a></li>
                     <li><a href="renderer/resource-management">Resource Management</a></li>
                     <li><a href="renderer/renderdoc-debugging">RenderDoc Debugging</a></li>
                 </ul>
            </div>

            <div class="section">
                <h2>Modernization</h2>
                                 <ul>
                     <li><a href="modernization/modern-cpp">Modern C++ Features</a></li>
                     <li><a href="modernization/build-systems">Build Systems</a></li>
                     <li><a href="modernization/profiling-tools">Profiling Tools</a></li>
                     <li><a href="modernization/package-management">Package Management</a></li>
                     <li><a href="modernization/ci-cd">CI/CD Pipeline</a></li>
                 </ul>
            </div>
        </div>
        
        <div id="content-container"></div>
    </div>

    <script>
        // Check if we're loading a direct URL and need to fetch content
        function handleDirectUrl() {
            const path = window.location.pathname;
            console.log('Current path:', path);
            if (path !== '/' && path !== '') {
                // Remove leading slash and handle the path
                const cleanPath = path.substring(1);
                if (cleanPath) {
                    console.log('Direct URL detected:', cleanPath);
                    // Don't update history for initial load
                    fetchContentDirect(cleanPath);
                }
            }
        }

        // Function to fetch content without updating history (for direct URLs)
        async function fetchContentDirect(url) {
            console.log('Fetching content for direct URL:', url);
            setLoading(true);
            toggleView(true);
            
            try {
                const fetchUrl = `/app/core/content-handler.php?page=${encodeURIComponent(url)}`;
                console.log('Making request to:', fetchUrl);
                
                const response = await fetch(fetchUrl, {
                    headers: {
                        'Accept': 'application/json',
                        'X-Requested-With': 'XMLHttpRequest'
                    }
                });
                
                if (!response.ok) {
                    throw new Error('Failed to load content');
                }
                
                const contentType = response.headers.get('content-type');
                if (!contentType || !contentType.includes('application/json')) {
                    const textResponse = await response.text();
                    console.error('Invalid response:', textResponse);
                    throw new Error('Invalid response format');
                }
                
                const data = await response.json();
                console.log('Direct URL response data:', data);
                
                if (data.success) {
                    document.getElementById('content-container').innerHTML = data.content;
                    if (data.title) {
                        document.title = data.title;
                    }
                    // Don't update history for direct URL load
                } else {
                    console.error('Server returned error:', data);
                    throw new Error(data.error || 'Failed to load content');
                }
            } catch (error) {
                console.error('Direct URL fetch error:', error);
                showError(error.message);
                toggleView(false);
            } finally {
                setLoading(false);
            }
        }

        // Utility function to show error message
        function showError(message) {
            const errorDiv = document.getElementById('error-message');
            errorDiv.textContent = message;
            errorDiv.style.display = 'block';
            setTimeout(() => {
                errorDiv.style.display = 'none';
            }, 5000);
        }
        
        // Function to show loading state
        function setLoading(isLoading) {
            document.getElementById('loading').style.display = isLoading ? 'block' : 'none';
        }
        
        // Function to toggle view between sections and content
        function toggleView(showContent) {
            const sectionsContainer = document.getElementById('sections-container');
            const contentContainer = document.getElementById('content-container');
            const backButton = document.getElementById('back-button');
            
            sectionsContainer.style.display = showContent ? 'none' : 'grid';
            contentContainer.style.display = showContent ? 'block' : 'none';
            backButton.style.display = showContent ? 'block' : 'none';
        }
        
        // Function to fetch and display content
        async function fetchContent(url) {
            console.log('Fetching content for URL:', url);
            setLoading(true);
            toggleView(true);
            
            try {
                const fetchUrl = `/app/core/content-handler.php?page=${encodeURIComponent(url)}`;
                console.log('Making request to:', fetchUrl);
                
                const response = await fetch(fetchUrl, {
                    headers: {
                        'Accept': 'application/json',
                        'X-Requested-With': 'XMLHttpRequest'
                    }
                });
                
                console.log('Response status:', response.status);
                console.log('Response headers:', response.headers);
                
                if (!response.ok) {
                    console.error('Response not OK:', response.status, response.statusText);
                    throw new Error('Failed to load content');
                }
                
                const contentType = response.headers.get('content-type');
                console.log('Content type:', contentType);
                
                if (!contentType || !contentType.includes('application/json')) {
                    console.error('Invalid content type:', contentType);
                    const textResponse = await response.text();
                    console.error('Response text:', textResponse);
                    throw new Error('Invalid response format');
                }
                
                const data = await response.json();
                console.log('Response data:', data);
                
                if (data.success) {
                    document.getElementById('content-container').innerHTML = data.content;
                    if (data.title) {
                        document.title = data.title;
                    }
                    // Update browser history with clean URL
                    history.pushState({ content: data.content }, data.title || '', '/' + url);
                } else {
                    console.error('Server returned error:', data);
                    throw new Error(data.error || 'Failed to load content');
                }
            } catch (error) {
                console.error('Fetch error:', error);
                showError(error.message);
                toggleView(false);
            } finally {
                setLoading(false);
            }
        }

        // Add click handlers to all links
        document.querySelectorAll('.section a').forEach(link => {
            link.addEventListener('click', function(e) {
                e.preventDefault();
                fetchContent(this.getAttribute('href'));
            });
        });

        // Back button handler
        document.getElementById('back-button').addEventListener('click', () => {
            toggleView(false);
            document.title = 'id Tech 3 Engine Documentation';
            history.pushState(null, 'id Tech 3 Engine Documentation', '/');
        });

        // Search functionality
        document.getElementById('search').addEventListener('input', function(e) {
            const searchTerm = e.target.value.toLowerCase();
            const sections = document.querySelectorAll('.section');
            
            sections.forEach(section => {
                const links = section.querySelectorAll('a');
                let hasMatch = false;
                
                links.forEach(link => {
                    const text = link.textContent.toLowerCase();
                    const linkElement = link.parentElement;
                    
                    if (text.includes(searchTerm)) {
                        hasMatch = true;
                        linkElement.style.display = 'block';
                    } else {
                        linkElement.style.display = 'none';
                    }
                });
                
                section.style.display = hasMatch || searchTerm === '' ? 'block' : 'none';
            });
        });

        // Handle browser navigation
        window.addEventListener('popstate', (e) => {
            if (e.state && e.state.content) {
                document.getElementById('content-container').innerHTML = e.state.content;
                toggleView(true);
            } else {
                toggleView(false);
            }
        });

        // Initialize direct URL handling when page loads
        document.addEventListener('DOMContentLoaded', handleDirectUrl);
    </script>
</body>
</html>