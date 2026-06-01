#!/usr/bin/env bash
# Ensure MSVC vcxproj lists qcommon sources that CMake always compiles (prevents LNK2019 on Windows CI).
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
QCOMMON="${ROOT}/src/qcommon"
DED_VCX="${ROOT}/src/platform/win32/msvc2017/quake3e-ded.vcxproj"
CLI_VCX="${ROOT}/src/platform/win32/msvc2017/quake3e.vcxproj"

fail() {
	echo "test_msvc_qcommon_parity: $*" >&2
	exit 1
}

[[ -f "${DED_VCX}" ]] || fail "missing ${DED_VCX}"
[[ -d "${QCOMMON}" ]] || fail "missing ${QCOMMON}"

# Optional or platform-specific; CMake may include them but MSVC omits by design.
MSVC_QCOMMON_ALLOWLIST=(
	lua_debug.c
	net_sdr.c
	puff.c
	vm_armv7l.c
	vm_powerpc.c
)

vcxproj_has() {
	local file="$1"
	local proj="$2"
	grep -Fq "qcommon\\${file}" "${proj}" || grep -Fq "qcommon/${file}" "${proj}"
}

is_allowed() {
	local file="$1"
	local x
	for x in "${MSVC_QCOMMON_ALLOWLIST[@]}"; do
		[[ "${file}" == "${x}" ]] && return 0
	done
	return 1
}

missing_ded=()
missing_cli=()

for path in "${QCOMMON}"/*.c; do
	[[ -f "${path}" ]] || continue
	base=$(basename "${path}")
	if is_allowed "${base}"; then
		continue
	fi
	if ! vcxproj_has "${base}" "${DED_VCX}"; then
		missing_ded+=("${base}")
	fi
	if ! vcxproj_has "${base}" "${CLI_VCX}"; then
		missing_cli+=("${base}")
	fi
done

if [[ ${#missing_ded[@]} -gt 0 ]]; then
	fail "quake3e-ded.vcxproj missing qcommon: ${missing_ded[*]}"
fi
if [[ ${#missing_cli[@]} -gt 0 ]]; then
	fail "quake3e.vcxproj missing qcommon: ${missing_cli[*]}"
fi

echo "test_msvc_qcommon_parity: MSVC qcommon source lists match CMake expectations"
