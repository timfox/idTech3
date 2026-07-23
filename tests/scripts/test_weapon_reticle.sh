#!/usr/bin/env bash
# Phase 2.6 static gate wrapper → production certification suite.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
exec "$ROOT/tests/scripts/test_wboit_production_certification.sh"
