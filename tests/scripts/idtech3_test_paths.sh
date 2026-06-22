# Shared path resolution for wiring tests (Phase 5c layout).
# Source from tests/scripts/*.sh:  source "$(dirname "$0")/idtech3_test_paths.sh"
idtech3_test_paths_init() {
	local root="${1:?root required}"
	IDTECH3_TEST_ROOT="$root"
	if [ -d "$root/runtime/client" ]; then
		IDTECH3_CLIENT_REL="runtime/client"
	elif [ -d "$root/src/client" ]; then
		IDTECH3_CLIENT_REL="src/client"
	else
		IDTECH3_CLIENT_REL="src/client"
	fi
	IDTECH3_CLIENT="$root/$IDTECH3_CLIENT_REL"
	if [ -d "$root/renderers" ]; then
		IDTECH3_RENDERERS="$root/renderers"
	else
		IDTECH3_RENDERERS="$root/src/renderers"
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
