#!/usr/bin/env bash
# SqueezeMe scaffolding checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"

grep -q 'R_SQZ_Init' "$ROOT/src/renderers/vulkan/vk_squeezeme.c"
grep -q 'SQZ_EvalLinearCorrectives' "$ROOT/src/renderers/vulkan/vk_squeezeme.c"
grep -q 'SQZ_UpsampleGCSNearest' "$ROOT/src/renderers/vulkan/vk_squeezeme.c"
test -f "$ROOT/docs/SQUEEZEME.md"
test -f "$ROOT/scripts/squeezeme_distill.py"
test -f "$ROOT/scripts/sqz_pack_demo.py"

python3 "$ROOT/scripts/sqz_pack_demo.py" "$ROOT/build/sqz_demo_test.sqz"
python3 -c "
import struct, sys
p=sys.argv[1]
d=open(p,'rb').read()
assert d[:4]==b'SQZ1'
print('sqz pack ok', len(d))
" "$ROOT/build/sqz_demo_test.sqz"

echo "test_squeezeme.sh: ok"
