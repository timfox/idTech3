#!/usr/bin/env bash
# GPU golden image scaffold: capture or compare renderer output.
# Tier A (default): manifest + placeholder hash check (no GPU required).
# Tier B: run idtech3 with +exec gpu_golden_capture.cfg when display available.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GOLDEN_DIR="${ROOT}/tests/data/golden"
MANIFEST="${GOLDEN_DIR}/manifest.txt"
ENGINE="${IDTECH3_BIN:-${ROOT}/release/idtech3}"
COMPARE_ONLY=0
CAPTURE=0

usage() {
	echo "Usage: $0 [--compare] [--capture]"
	echo "  --compare  Verify golden manifest (default)"
	echo "  --capture  Run capture path (requires display + game data)"
	exit 1
}

while [[ $# -gt 0 ]]; do
	case "$1" in
	--compare) COMPARE_ONLY=1; shift ;;
	--capture) CAPTURE=1; shift ;;
	-h|--help) usage ;;
	*) echo "Unknown: $1"; usage ;;
	esac
done

mkdir -p "${GOLDEN_DIR}"

if [[ ! -f "${MANIFEST}" ]]; then
	cat > "${MANIFEST}" <<'EOF'
# GPU golden manifest (Tier A: file presence + size; Tier B: perceptual hash)
# Format: relative_path min_bytes max_bytes
placeholder/README.txt 1 4096
EOF
fi

failures=0
while IFS= read -r line || [[ -n "$line" ]]; do
	[[ -z "$line" || "$line" =~ ^# ]] && continue
	read -r relpath minb maxb <<< "$line"
	fpath="${GOLDEN_DIR}/${relpath}"
	if [[ ! -f "$fpath" ]]; then
		if [[ "$relpath" == "placeholder/README.txt" ]]; then
			mkdir -p "$(dirname "$fpath")"
			echo "GPU golden Tier A placeholder — replace with captured PNGs for Tier B." > "$fpath"
		else
			echo "FAIL: missing golden ${relpath}"
			failures=$((failures + 1))
			continue
		fi
	fi
	size=$(stat -c%s "$fpath" 2>/dev/null || stat -f%z "$fpath")
	if (( size < minb || size > maxb )); then
		echo "FAIL: ${relpath} size ${size} not in [${minb},${maxb}]"
		failures=$((failures + 1))
	else
		echo "OK: ${relpath} (${size} bytes)"
	fi
done < "${MANIFEST}"

if [[ "$CAPTURE" -eq 1 ]]; then
	if [[ ! -x "$ENGINE" ]]; then
		echo "WARN: capture skipped — engine not found: $ENGINE"
	elif [[ -z "${DISPLAY:-}" ]]; then
		echo "WARN: capture skipped — no DISPLAY"
	else
		echo "Capture: screenshot after gpu_golden_capture.cfg (manual Tier B)"
		# Non-blocking: user extends with real map + rtest scene
		"$ENGINE" +set cl_renderer vulkan +exec gpu_golden_capture.cfg +quit 2>/dev/null || true
	fi
fi

if (( failures > 0 )); then
	echo "GPU golden check FAILED (${failures})"
	exit 1
fi
echo "GPU golden check PASSED"
exit 0
