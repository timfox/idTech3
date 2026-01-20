#!/bin/bash
# Setup Java 17+ environment for Android build
# Usage: ./setup_java.sh

set -e

echo "Setting up Java 17+ environment for Android build..."
echo "Current Java version:"
java -version
echo ""

# Check for Java 17+ installations
JAVA17_PATHS=(
    "/usr/lib/jvm/java-17-openjdk-amd64"
    "/usr/lib/jvm/java-17-openjdk"
    "/usr/lib/jvm/jdk-17"
    "/opt/jdk-17"
    "$HOME/.jdks/jdk-17"
    "$(which java | sed 's:/bin/java::')"
)

JAVA17_FOUND=""
for path in "${JAVA17_PATHS[@]}"; do
    if [ -d "$path" ] && [ -f "$path/bin/java" ]; then
        JAVA17_FOUND="$path"
        break
    fi
done

if [ -n "$JAVA17_FOUND" ]; then
    echo "Found Java 17 at: $JAVA17_FOUND"
    export JAVA_HOME="$JAVA17_FOUND"
    export PATH="$JAVA_HOME/bin:$PATH"
    echo "Set JAVA_HOME to: $JAVA_HOME"
    echo ""
    echo "New Java version:"
    java -version
    echo ""
    echo "Environment setup complete!"
    echo "You can now run: ./build_apk.sh Release"
else
    echo "ERROR: Java 17+ not found!"
    echo ""
    echo "Please install Java 17+:"
    echo "  Ubuntu/Debian: sudo apt install openjdk-17-jdk"
    echo "  Fedora/CentOS: sudo dnf install java-17-openjdk"
    echo "  Arch Linux:    sudo pacman -S jdk17-openjdk"
    echo "  Manual:        Download from https://adoptium.net/"
    echo ""
    echo "Or set JAVA_HOME manually to your JDK 17+ installation"
    exit 1
fi