#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

test -x scripts/check_first_party_warnings.sh || fail "missing executable warning scanner"
grep -q 'idtech3_first_party_warnings_as_errors' CMakeLists.txt || fail "missing first-party Werror helper"
grep -q 'target_compile_options(${target_name} PRIVATE -Werror)' CMakeLists.txt || fail "missing GCC/Clang -Werror"
grep -q 'target_compile_options(${target_name} PRIVATE /WX)' CMakeLists.txt || fail "missing MSVC /WX"
grep -q 'idtech3_first_party_warnings_as_errors(qcommon)' CMakeLists.txt || fail "qcommon must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(qcommon_ded)' CMakeLists.txt || fail "qcommon_ded must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(botlib)' CMakeLists.txt || fail "botlib must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(client)' CMakeLists.txt || fail "client must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(q3ui)' CMakeLists.txt || fail "q3ui must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(phys_module)' CMakeLists.txt || fail "phys_module must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(${RENDERER_PREFIX}_vulkan' CMakeLists.txt || fail "Vulkan renderer must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(${CNAME}${BINEXT})' CMakeLists.txt || fail "client executable must be Werror-gated"
grep -q 'idtech3_first_party_warnings_as_errors(${DNAME}${BINEXT})' CMakeLists.txt || fail "dedicated server executable must be Werror-gated"
grep -q 'scripts/check_first_party_warnings.sh' .github/workflows/build.yml || fail "workflow must run warning scanner"
grep -q 'tee build-vk-${{ matrix.btype }}.log' .github/workflows/build.yml || fail "workflow must capture build log"

sample="$(mktemp)"
scan_out="$(mktemp)"
scan_err="$(mktemp)"
trap 'rm -f "$sample" "$scan_out" "$scan_err"' EXIT
cat > "$sample" <<'LOG'
/tmp/build/CMakeFiles/foo.c:1:1: warning: generated
/repo/third_party/lib/foo.c:2:2: warning: vendored
/repo/runtime/client/core/cl_main.c:3:3: warning: first-party
LOG
if scripts/check_first_party_warnings.sh "$sample" >"$scan_out" 2>"$scan_err"; then
	fail "warning scanner did not reject first-party sample warning"
fi

cat > "$sample" <<'LOG'
/repo/third_party/lib/foo.c:2:2: warning: vendored
/repo/build-vk-Release/CMakeFiles/foo.c:3:3: warning: generated
LOG
scripts/check_first_party_warnings.sh "$sample" >"$scan_out" 2>"$scan_err" || fail "warning scanner rejected ignored warnings"

pass "first-party warning-free CI policy is wired"
