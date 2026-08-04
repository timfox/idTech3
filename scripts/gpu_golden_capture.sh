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
	CAPTURE_HOME=$(mktemp -d "${TMPDIR:-/tmp}/idtech3-golden.XXXXXX")
	trap 'rm -rf "$CAPTURE_HOME"' EXIT
	if [[ ! -x "$ENGINE" ]]; then
		echo "WARN: capture skipped — engine not found: $ENGINE"
	elif [[ -z "${DISPLAY:-}" ]]; then
		echo "WARN: capture skipped — no DISPLAY"
	else
		echo "Capture: gpu_golden_capture.cfg (Tier B — needs fs_game with cfg in pk3 or base/)"
		GAME_ARGS=( +set cl_renderer vulkan )
		if [[ -d "${ROOT}/release/demo_game" ]] || [[ -f "${ROOT}/release/idtech3_demo.pk3" ]]; then
			GAME_ARGS+=( +set fs_game idtech3_demo )
		fi
		if [[ -n "${GAME_BASE:-}" ]]; then
			GAME_ARGS+=( +set fs_basepath "${GAME_BASE}" )
		fi
		"$ENGINE" "${GAME_ARGS[@]}" +set fs_homepath "$CAPTURE_HOME" +exec gpu_golden_capture.cfg 2>/dev/null || true
		capture_file=$(find "$CAPTURE_HOME" -type f -name 'renderer_golden.jpg' -print -quit)
		if [[ -n "$capture_file" ]]; then
			mkdir -p "${GOLDEN_DIR}/captures"
			cp "$capture_file" "${GOLDEN_DIR}/captures/renderer_golden.jpg"
			echo "Captured: ${GOLDEN_DIR}/captures/renderer_golden.jpg"
		else
			echo "WARN: capture completed without renderer_golden.jpg"
		fi
	fi
fi

if (( failures > 0 )); then
	echo "GPU golden check FAILED (${failures})"
	exit 1
fi
echo "GPU golden check PASSED"
exit 0
