#!/bin/bash
# Reproducible Build Setup Script
# Ensures consistent build environment across different systems

set -euo pipefail

echo "Setting up reproducible build environment..."

# Set consistent timestamps
export SOURCE_DATE_EPOCH=$(git log -1 --format=%ct 2>/dev/null || date +%s)
echo "SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH"

# Set consistent locale
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export TZ=UTC

# Create reproducible build info
cat > build_info.h << EOF
#ifndef BUILD_INFO_H
#define BUILD_INFO_H

#define BUILD_GIT_COMMIT "$(git rev-parse HEAD 2>/dev/null || echo 'unknown')"
#define BUILD_TIMESTAMP "$(date -u +"%Y-%m-%dT%H:%M:%SZ" -d "@$SOURCE_DATE_EPOCH")"
#define BUILD_COMPILER "$(cc --version | head -1 2>/dev/null || echo 'unknown')"
#define BUILD_PLATFORM "$(uname -s)-$(uname -m)"

#endif /* BUILD_INFO_H */
EOF

echo "Build info header generated"
echo "Git commit: $(git rev-parse HEAD 2>/dev/null || echo 'unknown')"
echo "Build timestamp: $(date -u +"%Y-%m-%dT%H:%M:%SZ" -d "@$SOURCE_DATE_EPOCH")"