#!/bin/bash
#
# Automated Release Packaging Script
# Creates release packages for all platforms
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
RELEASE_DIR="$PROJECT_ROOT/release"
PACKAGE_DIR="$PROJECT_ROOT/release-packages"

# Get version from git tag or VERSION file
if [ -n "$1" ]; then
    VERSION="$1"
elif [ -f "$PROJECT_ROOT/VERSION" ]; then
    VERSION=$(cat "$PROJECT_ROOT/VERSION")
elif git describe --tags --exact-match HEAD 2>/dev/null; then
    VERSION=$(git describe --tags --exact-match HEAD | sed 's/^v//')
else
    VERSION="dev-$(git rev-parse --short HEAD)"
fi

echo "Packaging release version: $VERSION"
echo "Build directory: $BUILD_DIR"
echo "Release directory: $RELEASE_DIR"
echo "Package directory: $PACKAGE_DIR"

# Create package directory
mkdir -p "$PACKAGE_DIR"

# Function to package Linux release
package_linux() {
    local arch="$1"
    local package_name="idtech3-linux-${arch}-${VERSION}"
    local package_dir="$PACKAGE_DIR/$package_name"
    
    echo "Packaging Linux ${arch}..."
    mkdir -p "$package_dir"
    
    # Copy binaries
    cp "$BUILD_DIR/idtech3.x86_64" "$package_dir/idtech3" 2>/dev/null || true
    cp "$BUILD_DIR/idtech3.server.x86_64" "$package_dir/idtech3.server" 2>/dev/null || true
    cp "$BUILD_DIR"/*.so "$package_dir/" 2>/dev/null || true
    
    # Copy mods
    if [ -d "$RELEASE_DIR/mymod" ]; then
        cp -r "$RELEASE_DIR/mymod" "$package_dir/"
    fi
    if [ -d "$RELEASE_DIR/blacksun" ]; then
        cp -r "$RELEASE_DIR/blacksun" "$package_dir/"
    fi
    
    # Create README
    cat > "$package_dir/README.txt" << EOF
id Tech 3 Engine Release ${VERSION}
==================================

Platform: Linux ${arch}
Build Date: $(date)

Installation:
1. Extract this archive
2. Run ./idtech3 to start the game

For server:
  ./idtech3.server

See README.md for more information.
EOF
    
    # Create archive
    cd "$PACKAGE_DIR"
    tar czf "${package_name}.tar.gz" "$package_name"
    echo "Created: ${package_name}.tar.gz"
}

# Function to package Windows release
package_windows() {
    local arch="$1"
    local package_name="idtech3-windows-${arch}-${VERSION}"
    local package_dir="$PACKAGE_DIR/$package_name"
    
    echo "Packaging Windows ${arch}..."
    mkdir -p "$package_dir"
    
    # Copy binaries (adjust paths for Windows)
    if [ -d "$BUILD_DIR/Release" ]; then
        cp "$BUILD_DIR/Release/idtech3.exe" "$package_dir/" 2>/dev/null || true
        cp "$BUILD_DIR/Release/idtech3.server.exe" "$package_dir/" 2>/dev/null || true
        cp "$BUILD_DIR/Release"/*.dll "$package_dir/" 2>/dev/null || true
    fi
    
    # Copy mods
    if [ -d "$RELEASE_DIR/mymod" ]; then
        cp -r "$RELEASE_DIR/mymod" "$package_dir/"
    fi
    if [ -d "$RELEASE_DIR/blacksun" ]; then
        cp -r "$RELEASE_DIR/blacksun" "$package_dir/"
    fi
    
    # Create README
    cat > "$package_dir/README.txt" << EOF
id Tech 3 Engine Release ${VERSION}
==================================

Platform: Windows ${arch}
Build Date: $(date)

Installation:
1. Extract this archive
2. Run idtech3.exe to start the game

For server:
  idtech3.server.exe

See README.md for more information.
EOF
    
    # Create archive
    cd "$PACKAGE_DIR"
    zip -r "${package_name}.zip" "$package_name"
    echo "Created: ${package_name}.zip"
}

# Function to package macOS release
package_macos() {
    local arch="$1"
    local package_name="idtech3-macos-${arch}-${VERSION}"
    local package_dir="$PACKAGE_DIR/$package_name"
    
    echo "Packaging macOS ${arch}..."
    mkdir -p "$package_dir"
    
    # Copy binaries
    cp "$BUILD_DIR/idtech3" "$package_dir/" 2>/dev/null || true
    cp "$BUILD_DIR/idtech3.server" "$package_dir/" 2>/dev/null || true
    cp "$BUILD_DIR"/*.dylib "$package_dir/" 2>/dev/null || true
    
    # Copy mods
    if [ -d "$RELEASE_DIR/mymod" ]; then
        cp -r "$RELEASE_DIR/mymod" "$package_dir/"
    fi
    if [ -d "$RELEASE_DIR/blacksun" ]; then
        cp -r "$RELEASE_DIR/blacksun" "$package_dir/"
    fi
    
    # Create README
    cat > "$package_dir/README.txt" << EOF
id Tech 3 Engine Release ${VERSION}
==================================

Platform: macOS ${arch}
Build Date: $(date)

Installation:
1. Extract this archive
2. Run ./idtech3 to start the game

For server:
  ./idtech3.server

See README.md for more information.
EOF
    
    # Create archive
    cd "$PACKAGE_DIR"
    tar czf "${package_name}.tar.gz" "$package_name"
    echo "Created: ${package_name}.tar.gz"
}

# Detect platform and package accordingly
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    package_linux "x86_64"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    ARCH=$(uname -m)
    if [ "$ARCH" == "arm64" ]; then
        package_macos "arm64"
    else
        package_macos "x86_64"
    fi
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    package_windows "x64"
else
    echo "Unknown platform: $OSTYPE"
    exit 1
fi

echo ""
echo "Release packaging complete!"
echo "Packages created in: $PACKAGE_DIR"

