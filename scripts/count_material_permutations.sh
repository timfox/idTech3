#!/usr/bin/env bash
# Count shader/pipeline ifdef permutations (warn threshold).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
THRESH="${MAT_PERM_THRESHOLD:-512}"
count=0
while IFS= read -r f; do
  n=$(grep -c '#if' "$f" 2>/dev/null || true)
  count=$((count + n))
done < <(find "$ROOT/src/renderers/vulkan" -name '*.c' -o -name '*.frag' -o -name '*.vert' 2>/dev/null)
echo "Material permutation ifdef count: $count (threshold $THRESH)"
if (( count > THRESH )); then
  echo "WARN: exceeds budget — see docs/MATERIAL_PERMUTATIONS.md"
  exit 1
fi
exit 0
