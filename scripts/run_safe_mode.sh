#!/usr/bin/env bash
# Safe mode launcher - disables experimental features for stability testing
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENGINE_BIN="${ENGINE_BIN:-$ROOT/release/idtech3.x86_64}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}🚨 Starting idtech3 in SAFE MODE${NC}"
echo "This disables experimental features for maximum stability"
echo ""

# Create safe mode flag
echo "Creating safe mode flag..."
mkdir -p "$ROOT/logs"
touch "$ROOT/logs/safe_mode.flag"
echo "Safe mode flag created at: $ROOT/logs/safe_mode.flag"
echo ""

# Launch with safe mode enabled
echo -e "${GREEN}Launching engine with safe mode...${NC}"
echo "Command: $ENGINE_BIN +set fs_game mymod"
echo ""

exec "$ENGINE_BIN" +set fs_game mymod "$@"