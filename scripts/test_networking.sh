#!/bin/bash

# Networking and Multiplayer Test Script for idTech3
# Tests local multiplayer, dedicated server, and networking features

set -euo pipefail

echo "idTech3 Networking & Multiplayer Test"
echo "======================================"
echo

# Set paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE="$PROJECT_ROOT/release/idtech3.x86_64"
SERVER="$PROJECT_ROOT/release/idtech3.server.x86_64"

# Check if binaries exist
if [ ! -x "$ENGINE" ]; then
    echo "Error: Engine not found at $ENGINE"
    echo "Please build the engine first with: ./scripts/compile_engine.sh vulkan"
    exit 1
fi

if [ ! -x "$SERVER" ]; then
    echo "Warning: Dedicated server not found at $SERVER"
    echo "Building server..."
    ./scripts/compile_engine.sh vulkan
fi

echo "Testing Networking Features..."
echo

# Test 1: Start dedicated server (using client in dedicated mode)
echo "Test 1: Starting Dedicated Server"
echo "Command: ./idtech3.x86_64 +set dedicated 1 +set sv_pure 0 +set sv_maxclients 8 +map q3dm9"
"$ENGINE" +set dedicated 1 +set sv_pure 0 +set sv_maxclients 8 +map q3dm9 &
SERVER_PID=$!
sleep 5

# Check if server is running
if kill -0 $SERVER_PID 2>/dev/null; then
    echo "✓ Dedicated server started successfully (PID: $SERVER_PID)"
else
    echo "✗ Failed to start dedicated server"
    exit 1
fi

echo
echo "Test 2: Connect Client to Server"
echo "Command: +connect localhost +set name Player1"
timeout 10 "$ENGINE" \
    +connect localhost \
    +set name "Player1" \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    2>&1 | grep -E "(connect|CONNECT|client|CLIENT|server|SERVER)" | head -5

echo
echo "Test 3: Test Enhanced Networking Features"
echo "Command: +connect localhost +set cl_enhancedNetworking 1 +set name Player2"
timeout 10 "$ENGINE" \
    +connect localhost \
    +set cl_enhancedNetworking 1 \
    +set name "Player2" \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    2>&1 | grep -E "(enhanced|ENHANCED|network|NETWORK)" | head -5

echo
echo "Test 4: Test WebSocket Support (if available)"
echo "Command: +set cl_websocket 1 +connect localhost"
timeout 10 "$ENGINE" \
    +set cl_websocket 1 \
    +connect localhost \
    +set name "Player3" \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    2>&1 | grep -E "(websocket|WEBSOCKET|connect|CONNECT)" | head -5

echo
echo "Test 5: Test Server Browser"
echo "Command: +set cl_master ioquake3.org +globalservers"
timeout 10 "$ENGINE" \
    +set cl_master "ioquake3.org" \
    +globalservers \
    +set r_mode 6 \
    +set r_fullscreen 0 \
    +set r_vulkan 1 \
    +set com_introplayed 1 \
    2>&1 | grep -E "(master|MASTER|server|SERVER|globalservers|GLOBAL)" | head -10

# Clean up server
echo
echo "Cleaning up test server..."
kill $SERVER_PID 2>/dev/null || true
sleep 2

echo
echo "Networking Test Complete!"
echo
echo "Available Networking CVars:"
echo "  cl_enhancedNetworking - Enable HTTP/2 and enhanced networking (0/1)"
echo "  cl_websocket - Enable WebSocket support (0/1)"
echo "  sv_pure - Server pure mode (0/1)"
echo "  sv_maxclients - Maximum clients (1-64)"
echo "  cl_master - Master server address"
echo "  rate - Client network rate limit"
echo "  snaps - Server update rate"
echo "  cl_timeout - Connection timeout (seconds)"
echo
echo "Multiplayer Features:"
echo "- Dedicated server support"
echo "- Enhanced networking stack (HTTP/2)"
echo "- WebSocket support"
echo "- Server browser with master server"
echo "- LAN and internet multiplayer"
echo "- Advanced lag compensation"
echo "- DDoS protection"
echo "- Rate limiting and security features"
echo
echo "Performance Notes:"
echo "- Enhanced networking reduces latency"
echo "- WebSocket improves connection stability"
echo "- Server pure mode ensures file integrity"
echo "- Master servers help with server discovery"