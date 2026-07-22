#!/usr/bin/env bash
# Print / refresh C++20 migration status (filesystem snapshot + inventory).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

count_ext() {
  local root="$1" ext="$2"
  find "$root" \( -path './third_party/*' -o -path './.git/*' -o -path './build*' -o -path './release/*' \) -prune -o -name "*.$ext" -print 2>/dev/null | wc -l
}

C_ENG=$(find engine runtime renderers modules tests -name '*.c' 2>/dev/null | wc -l)
CXX_ENG=$(find engine runtime renderers modules tests -name '*.cpp' 2>/dev/null | wc -l)
CONVERTED=$(grep -c $'\tcpp\t.*converted' docs/cpp20_inventory.tsv 2>/dev/null || echo 0)
BLOCKED=$(grep -c 'CPP_BLOCKED\|KEEP_C_EXTERNAL\|defer' docs/cpp20_inventory.tsv 2>/dev/null || echo 0)

echo "cpp20_status (filesystem):"
echo "  first_party C=.c count:  $C_ENG"
echo "  first_party C++=.cpp:    $CXX_ENG"
echo "  inventory converted:     $CONVERTED"
echo "  inventory blocked-ish:   $BLOCKED"
echo "  options: see CMake USE_CPP20 CPP20_EXCEPTIONS CPP20_RTTI CPP20_STRICT"
echo "  docs: docs/CPP20_MIGRATION.md"
if [[ -f engine/core/cpp20_migration_status.inc ]]; then
  echo "  embedded snapshot:"
  grep 'CPP20_STATUS_' engine/core/cpp20_migration_status.inc | sed 's/^/    /'
fi
