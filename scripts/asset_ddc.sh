#!/usr/bin/env bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 "$SCRIPT_DIR/asset_ddc.py" "$@"
