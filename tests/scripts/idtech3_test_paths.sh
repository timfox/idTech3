# Shared path resolution for wiring tests (Phase 5c/5e layout).
# Source from tests/scripts/*.sh:  source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init() {
	local root="${1:?root required}"
	IDTECH3_TEST_ROOT="$root"
	if [ -d "$root/runtime/client" ] && [ ! -L "$root/runtime/client" ]; then
		IDTECH3_CLIENT_REL="runtime/client"
	elif [ -d "$root/src/client" ]; then
		IDTECH3_CLIENT_REL="src/client"
	else
		IDTECH3_CLIENT_REL="src/client"
	fi
	IDTECH3_CLIENT="$root/$IDTECH3_CLIENT_REL"

	if [ -d "$root/engine/core" ] && [ ! -L "$root/engine/core" ]; then
		IDTECH3_QCOMMON_REL="engine/core"
	else
		IDTECH3_QCOMMON_REL="src/qcommon"
	fi
	IDTECH3_QCOMMON="$root/$IDTECH3_QCOMMON_REL"

	if [ -d "$root/runtime/server" ] && [ ! -L "$root/runtime/server" ]; then
		IDTECH3_SERVER_REL="runtime/server"
	else
		IDTECH3_SERVER_REL="src/server"
	fi
	IDTECH3_SERVER="$root/$IDTECH3_SERVER_REL"

	if [ -d "$root/modules/world" ] && [ ! -L "$root/modules/world" ]; then
		IDTECH3_WORLD_REL="modules/world"
	else
		IDTECH3_WORLD_REL="src/world"
	fi
	IDTECH3_WORLD="$root/$IDTECH3_WORLD_REL"

	if [ -d "$root/renderers" ] && [ ! -L "$root/renderers" ]; then
		IDTECH3_RENDERERS="$root/renderers"
		IDTECH3_RENDERERS_REL="renderers"
	else
		IDTECH3_RENDERERS="$root/src/renderers"
		IDTECH3_RENDERERS_REL="src/renderers"
	fi
}

# Prefer canonical path; fall back to src/* shim.
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
