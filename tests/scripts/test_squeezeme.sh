#!/usr/bin/env bash
# SqueezeMe scaffolding checks
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
# shellcheck source=idtech3_test_paths.sh
source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init "$ROOT"

VK_SQUEEZEME="$(idtech3_file renderers/vulkan/extensions/splats/vk_squeezeme.c src/renderers/vulkan/extensions/splats/vk_squeezeme.c)"

grep -q 'R_SQZ_Init' "$VK_SQUEEZEME"
grep -q 'SQZ_EvalLinearCorrectives' "$VK_SQUEEZEME"
grep -q 'SQZ_UpsampleGCSNearest' "$VK_SQUEEZEME"
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
