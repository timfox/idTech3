# WebUI CMake configuration
# Handles platform-specific WebView integration

# Check if WebUI is enabled
if(NOT DEFINED USE_WEBUI)
    option(USE_WEBUI "Enable WebView integration" ON)
endif()

if(USE_WEBUI)
    message(STATUS "WebUI: Enabled")
    
    # Platform-specific configuration
    if(WIN32)
        # Windows: WebView2
        message(STATUS "WebUI: Windows platform - WebView2")
        
        # Check for WebView2 SDK
        find_path(WEBVIEW2_INCLUDE_DIR
            NAMES WebView2.h
            PATHS
                "${CMAKE_PROGRAM_FILES}/Microsoft/Edge WebView2"
                "C:/Program Files/Microsoft/Edge WebView2"
                "C:/Program Files (x86)/Microsoft/Edge WebView2"
            PATH_SUFFIXES
                include
        )
        
        if(WEBVIEW2_INCLUDE_DIR)
            message(STATUS "WebUI: WebView2 SDK found at ${WEBVIEW2_INCLUDE_DIR}")
            
            # Add webui source files
            target_sources(engine PRIVATE
                engine/webui/webui_main.c
                engine/webui/webui_win32.c
            )
            
            # Add include directories
            target_include_directories(engine PRIVATE
                engine/webui
                ${WEBVIEW2_INCLUDE_DIR}
            )
            
            # Link WebView2
            target_link_libraries(engine PRIVATE
                WebView2
            )
            
            # Define WebView2 flag
            add_definitions(-DWEBUI_USE_WEBVIEW2)
        else()
            message(WARNING "WebUI: WebView2 SDK not found, WebUI disabled")
            set(USE_WEBUI OFF CACHE BOOL "Enable WebView integration" FORCE)
        endif()
        
    elseif(UNIX AND NOT APPLE)
        # Linux: WebKitGTK
        message(STATUS "WebUI: Linux platform - WebKitGTK")
        
        # Find WebKitGTK using pkg-config
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(WEBKIT2GTK
            REQUIRED
            IMPORTED_TARGET
            webkit2gtk-4.1
        )
        
        if(WEBKIT2GTK_FOUND)
            message(STATUS "WebUI: WebKitGTK found")
            
            # Add webui source files
            target_sources(engine PRIVATE
                engine/webui/webui_main.c
                engine/webui/webui_linux.c
            )
            
            # Add include directories
            target_include_directories(engine PRIVATE
                engine/webui
                ${WEBKIT2GTK_INCLUDE_DIRS}
            )
            
            # Link WebKitGTK
            target_link_libraries(engine PRIVATE
                PkgConfig::WEBKIT2GTK
            )
            
            # Define WebKitGTK flag
            add_definitions(-DWEBUI_USE_WEBKITGTK)
        else()
            message(WARNING "WebUI: WebKitGTK not found, WebUI disabled")
            set(USE_WEBUI OFF CACHE BOOL "Enable WebView integration" FORCE)
        endif()
        
    elseif(APPLE)
        # macOS: WKWebView
        message(STATUS "WebUI: macOS platform - WKWebView")
        
        # Find WebKit framework
        find_library(WEBKIT WebKit)
        
        if(WEBKIT)
            message(STATUS "WebUI: WebKit framework found at ${WEBKIT}")
            
            # Add webui source files
            target_sources(engine PRIVATE
                engine/webui/webui_main.c
                engine/webui/webui_macos.c
            )
            
            # Add include directories
            target_include_directories(engine PRIVATE
                engine/webui
            )
            
            # Link WebKit
            target_link_libraries(engine PRIVATE
                ${WEBKIT}
            )
            
            # Define WKWebView flag
            add_definitions(-DWEBUI_USE_WKWEBVIEW)
        else()
            message(WARNING "WebUI: WebKit framework not found, WebUI disabled")
            set(USE_WEBUI OFF CACHE BOOL "Enable WebView integration" FORCE)
        endif()
        
    else()
        message(WARNING "WebUI: Unsupported platform, WebUI disabled")
        set(USE_WEBUI OFF CACHE BOOL "Enable WebView integration" FORCE)
    endif()
    
else()
    message(STATUS "WebUI: Disabled")
endif()
