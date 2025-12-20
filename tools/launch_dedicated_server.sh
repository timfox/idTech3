#!/bin/bash

# Enhanced Dedicated Server Launcher
# Professional server management script

# Configuration
SERVER_NAME="Enhanced idTech3 Server"
GAME_MOD="mymod"
SERVER_CONFIG="config/server_dedicated.cfg"
PORT="27960"
MAX_PLAYERS="16"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[SERVER]${NC} $1"
}

# Check if server executable exists
check_executable() {
    if [ ! -f "idtech3.x86_64" ]; then
        print_error "Server executable 'idtech3.x86_64' not found!"
        print_error "Please build the engine first with './tools/compile_engine.sh'"
        exit 1
    fi
}

# Create necessary directories
create_directories() {
    print_status "Creating server directories..."
    mkdir -p logs
    mkdir -p backups
    mkdir -p stats
}

# Backup server logs and configs
backup_server_data() {
    if [ -d "logs" ]; then
        local timestamp=$(date +%Y%m%d_%H%M%S)
        local backup_file="backups/server_backup_${timestamp}.tar.gz"

        print_status "Creating server backup: ${backup_file}"
        tar -czf "${backup_file}" logs/ *.log 2>/dev/null || true

        # Keep only last 10 backups
        ls -t backups/server_backup_*.tar.gz 2>/dev/null | tail -n +11 | xargs rm -f 2>/dev/null || true
    fi
}

# Start the dedicated server
start_server() {
    print_info "Starting ${SERVER_NAME}"
    print_info "Port: ${PORT}"
    print_info "Max Players: ${MAX_PLAYERS}"
    print_info "Game Mod: ${GAME_MOD}"
    echo

    # Set environment variables for better performance
    export __GL_SHADER_DISK_CACHE=1
    export __GL_SHADER_DISK_CACHE_SIZE=1073741824  # 1GB cache

    # Launch server with enhanced configuration
    exec ./idtech3.x86_64 \
        +set dedicated 2 \
        +set net_port "${PORT}" \
        +set sv_maxclients "${MAX_PLAYERS}" \
        +set fs_game "${GAME_MOD}" \
        +exec "${SERVER_CONFIG}" \
        +set sv_pure 1 \
        +set sv_cheats 0 \
        +set g_logfile 2 \
        +set g_logfileSync 1 \
        +set logfilename "logs/server_$(date +%Y%m%d_%H%M%S).log" \
        "$@"
}

# Display server information
show_server_info() {
    echo
    print_info "=== SERVER INFORMATION ==="
    echo "Server Name: ${SERVER_NAME}"
    echo "Port: ${PORT}"
    echo "Max Players: ${MAX_PLAYERS}"
    echo "Game Mod: ${GAME_MOD}"
    echo "Config: ${SERVER_CONFIG}"
    echo
    echo "Connect using: /connect your-server-ip:${PORT}"
    echo
    print_info "=== ENHANCED FEATURES ==="
    echo "✓ PBR Rendering for all players"
    echo "✓ Advanced weapon systems"
    echo "✓ Dynamic environmental effects"
    echo "✓ Performance monitoring"
    echo "✓ Anti-cheat measures"
    echo "✓ Lua scripting enhancements"
    echo "✓ Automatic statistics logging"
    echo
}

# Main execution
main() {
    echo
    print_info "Enhanced idTech3 Dedicated Server Launcher"
    print_info "=========================================="
    echo

    # Check requirements
    check_executable

    # Setup
    create_directories
    backup_server_data

    # Show info
    show_server_info

    # Start server
    print_status "Launching dedicated server..."
    echo
    start_server "$@"
}

# Handle command line arguments
case "$1" in
    --help|-h)
        echo "Usage: $0 [options]"
        echo
        echo "Enhanced idTech3 Dedicated Server Launcher"
        echo
        echo "Options:"
        echo "  --help, -h          Show this help message"
        echo "  --info, -i          Show server information only"
        echo "  --backup            Create backup only"
        echo
        echo "Environment Variables:"
        echo "  SERVER_NAME         Server display name"
        echo "  GAME_MOD           Game mod to use (default: mymod)"
        echo "  SERVER_CONFIG      Server config file"
        echo "  PORT               Server port (default: 27960)"
        echo "  MAX_PLAYERS        Maximum players (default: 16)"
        echo
        exit 0
        ;;
    --info|-i)
        show_server_info
        exit 0
        ;;
    --backup)
        backup_server_data
        print_status "Backup completed"
        exit 0
        ;;
    *)
        main "$@"
        ;;
esac
