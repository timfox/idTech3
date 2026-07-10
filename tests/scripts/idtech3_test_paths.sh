# Shared path resolution for wiring tests (Phase 5c/5e layout).
# Source from tests/scripts/*.sh:  source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init() {
	local root="${1:?root required}"
	IDTECH3_TEST_ROOT="$root"

	_idtech3_pick_root() {
		local canon_rel="$1"
		local shim_rel="$2"
		if [ -d "$root/$canon_rel" ] && [ ! -L "$root/$canon_rel" ]; then
			echo "$canon_rel"
		else
			echo "$shim_rel"
		fi
	}

	IDTECH3_CLIENT_REL="$(_idtech3_pick_root runtime/client src/client)"
	IDTECH3_CLIENT="$root/$IDTECH3_CLIENT_REL"

	IDTECH3_QCOMMON_REL="$(_idtech3_pick_root engine/core src/qcommon)"
	IDTECH3_QCOMMON="$root/$IDTECH3_QCOMMON_REL"

	IDTECH3_SERVER_REL="$(_idtech3_pick_root runtime/server src/server)"
	IDTECH3_SERVER="$root/$IDTECH3_SERVER_REL"

	IDTECH3_GAME_REL="$(_idtech3_pick_root runtime/game src/game)"
	IDTECH3_GAME="$root/$IDTECH3_GAME_REL"

	IDTECH3_WORLD_REL="$(_idtech3_pick_root modules/world src/world)"
	IDTECH3_WORLD="$root/$IDTECH3_WORLD_REL"

	IDTECH3_NAV_REL="$(_idtech3_pick_root modules/navigation src/navigation)"
	IDTECH3_NAV="$root/$IDTECH3_NAV_REL"

	IDTECH3_AUDIO_REL="$(_idtech3_pick_root modules/audio src/audio)"
	IDTECH3_AUDIO="$root/$IDTECH3_AUDIO_REL"

	IDTECH3_BOTLIB_REL="$(_idtech3_pick_root modules/botlib src/botlib)"
	IDTECH3_BOTLIB="$root/$IDTECH3_BOTLIB_REL"

	IDTECH3_EXTENSIONS_REL="$(_idtech3_pick_root extensions src/extensions)"
	IDTECH3_EXTENSIONS="$root/$IDTECH3_EXTENSIONS_REL"

	if [ -d "$root/renderers" ] && [ ! -L "$root/renderers" ]; then
		IDTECH3_RENDERERS_REL="renderers"
	else
		IDTECH3_RENDERERS_REL="src/renderers"
	fi
	IDTECH3_RENDERERS="$root/$IDTECH3_RENDERERS_REL"
}

# Prefer canonical path; fall back to src/* shim. Args are repo-relative.
idtech3_resolve_file() {
	local canon="$1"
	local shim="$2"
	if [ -f "${IDTECH3_TEST_ROOT}/${canon}" ]; then
		echo "${IDTECH3_TEST_ROOT}/${canon}"
	elif [ -f "${IDTECH3_TEST_ROOT}/${shim}" ]; then
		echo "${IDTECH3_TEST_ROOT}/${shim}"
	else
		echo "${IDTECH3_TEST_ROOT}/${canon}"
	fi
}

# Alias used by migrated wiring tests: idtech3_file canon shim
idtech3_file() {
	idtech3_resolve_file "$@"
}

# Require that at least one of canon/shim exists; print the resolved path.
idtech3_require_file() {
	local canon="$1"
	local shim="$2"
	local label="${3:-$canon}"
	local path
	path="$(idtech3_resolve_file "$canon" "$shim")"
	if [ ! -f "$path" ]; then
		echo "FAIL: missing $label (tried $canon and $shim)" >&2
		return 1
	fi
	echo "$path"
}

idtech3_submodule_path() {
	local name="$1"
	case "$name" in
		freeusd)
			if grep -qF 'path = third_party/FreeUSD' "$IDTECH3_TEST_ROOT/.gitmodules" 2>/dev/null; then
				echo "third_party/FreeUSD"
			else
				echo "src/external/FreeUSD"
			fi
			;;
		backend)
			if grep -qF 'path = third_party/idtech3backend' "$IDTECH3_TEST_ROOT/.gitmodules" 2>/dev/null; then
				echo "third_party/idtech3backend"
			else
				echo "src/external/idtech3backend"
			fi
			;;
		svo)
			if grep -qF 'path = third_party/src/SparseVoxelOctree' "$IDTECH3_TEST_ROOT/.gitmodules" 2>/dev/null; then
				echo "third_party/src/SparseVoxelOctree"
			else
				echo "src/external/src/SparseVoxelOctree"
			fi
			;;
		*) return 1 ;;
	esac
}
