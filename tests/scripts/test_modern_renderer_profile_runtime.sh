#!/usr/bin/env bash
# Modern Vulkan profile confidence guard: source contract plus optional client smoke.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MODE="${1:-source}"

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

require_or_skip() {
	local msg="$1"
	if [[ "${IDTECH3_RENDERER_RUNTIME_REQUIRED:-0}" = "1" ]]; then
		fail "$msg"
	fi
	echo "SKIP: $msg"
	exit 0
}

check_grep() {
	local file="$1"
	local pattern="$2"
	local msg="$3"
	grep -q "$pattern" "$file" || fail "$msg"
	pass "$msg"
}

find_client() {
	local candidate
	for candidate in \
		"${IDTECH3_BIN:-}" \
		"${RELEASE_DIR:-$ROOT/release}/idtech3" \
		"${RELEASE_DIR:-$ROOT/release}/idtech3.x86_64" \
		"${BUILD_DIR:-$ROOT/build-vk-Release}/idtech3" \
		"${BUILD_DIR:-$ROOT/build-vk-Release}/idtech3.x86_64"; do
		if [[ -n "$candidate" && -x "$candidate" ]]; then
			printf '%s\n' "$candidate"
			return 0
		fi
	done
	return 1
}

source_checks() {
	cd "$ROOT"

	local cfg="config/modern_vulkan.cfg"
	local stable="config/modern_vulkan_stable.cfg"
	local quality="config/modern_vulkan_quality.cfg"
	local deferred="config/vulkan_overlay_deferred.cfg"
	local rtx="config/vulkan_overlay_rtx.cfg"
	local hybrid="config/vulkan_overlay_hybrid1.cfg"
	local tr_init="renderers/vulkan/tr_init.c"
	local tr_diag="renderers/vulkan/diagnostics/tr_init_diagnostics.inc"

	[[ -f "$cfg" ]] || fail "missing modern Vulkan profile"
	[[ -f "$stable" ]] || fail "missing modern_vulkan_stable.cfg"
	[[ -f "$quality" ]] || fail "missing modern_vulkan_quality.cfg"
	[[ -f "config/gfx_safe.cfg" ]] || fail "missing gfx_safe.cfg"
	[[ -f "$deferred" ]] || fail "missing deferred overlay"
	[[ -f "$rtx" ]] || fail "missing RTX overlay"
	[[ -f "$hybrid" ]] || fail "missing Hybrid1 overlay"
	[[ -f "$tr_diag" ]] || fail "missing renderer diagnostics include"

	check_grep "$cfg" 'exec modern_vulkan_stable.cfg' "modern_vulkan.cfg inherits Spine stable profile"
	check_grep "$stable" 'seta r_renderMode 3' "stable profile selects Unified Clustered mode 3"
	check_grep "$stable" 'seta r_forwardPlus 1' "stable profile enables Forward+"
	check_grep "$stable" 'seta r_forwardPlusZSlices 8' "stable profile enables clustered Z slices"
	check_grep "$stable" 'seta r_forwardPlusZSliceMode 1' "stable profile uses logarithmic Z slicing"
	check_grep "$stable" 'seta r_deferredGBuffer 1' "stable profile enables G-buffer sidecar"
	check_grep "$stable" 'seta r_deferredLighting 1' "stable profile enables clustered deferred opaque lighting"
	check_grep "$stable" 'seta r_pbr 1' "stable profile enables PBR"
	check_grep "$stable" 'seta r_hdr 2' "stable profile enables HDR32"
	check_grep "$stable" 'seta r_aaMode 2' "stable profile defaults SMAA 1x (r_aaMode 2)"
	check_grep "$stable" 'seta r_taa 0' "stable profile defaults Temporal Reconstruction off"
	check_grep "$stable" 'seta r_ext_smaa 1' "stable profile enables SMAA"
	check_grep "$stable" 'seta r_oit 1' "stable profile enables production WBOIT"
	check_grep "$stable" 'seta r_stochasticAlpha 0' "stable profile disables stochastic alpha"
	check_grep "$stable" 'seta r_visibilityBuffer 0' "stable profile disables visibility buffer"
	check_grep "$stable" 'seta r_ambientVisibilityMode 2' "stable profile uses GTAO"
	check_grep "$stable" 'seta r_ssao 0' "stable profile disables SSAO when GTAO active"
	check_grep "$stable" 'seta r_hybrid1 0' "stable profile disables Hybrid1"
	check_grep "$stable" 'seta r_openWorld 0' "stable profile disables open-world streaming"
	check_grep "$quality" 'seta r_oit 1' "quality profile enables WBOIT"
	check_grep "$quality" 'seta r_taa 0' "quality profile keeps TAA off"

	check_grep "$deferred" 'exec modern_vulkan.cfg' "deferred overlay inherits modern profile"
	check_grep "$deferred" 'seta r_renderMode 1' "deferred overlay selects deferred mode"
	check_grep "$rtx" 'exec modern_vulkan.cfg' "RTX overlay inherits modern profile"
	check_grep "$hybrid" 'exec modern_vulkan.cfg' "Hybrid1 overlay inherits modern profile"
	if grep -q 'seta r_renderMode 1' "$hybrid"; then
		fail "Hybrid1 overlay must not replace modern clustered base with mode 1"
	fi

	check_grep "scripts/compile_engine.sh" 'modern_vulkan.cfg' "release packaging includes modern profile"
	check_grep "scripts/compile_engine.sh" 'modern_vulkan_stable.cfg' "release packaging includes stable profile"
	check_grep "scripts/compile_engine.sh" 'gfx_safe.cfg' "release packaging includes gfx_safe"
	check_grep "scripts/compile_engine.sh" 'modern_clustered.cfg' "release packaging includes clustered profile"
	check_grep "scripts/compile_engine.sh" 'vulkan_overlay_deferred.cfg' "release packaging includes deferred overlay"
	check_grep "scripts/compile_engine.sh" 'vulkan_overlay_rtx.cfg' "release packaging includes RTX overlay"
	check_grep "scripts/compile_engine.sh" 'vulkan_overlay_hybrid1.cfg' "release packaging includes Hybrid1 overlay"

	check_grep "$tr_init" 'ri.Cmd_AddCommand( "renderer_status"' "renderer_status is registered"
	check_grep "$tr_init" 'ri.Cmd_AddCommand( "renderer_profile"' "renderer_profile is registered"
	check_grep "$tr_init" 'ri.Cmd_AddCommand( "renderer_health"' "renderer_health is registered"
	check_grep "$tr_init" 'ri.Cmd_AddCommand( "renderer_subsystems"' "renderer_subsystems is registered"
	check_grep "$tr_init" 'ri.Cmd_AddCommand( "renderer_compat"' "renderer_compat is registered"
	check_grep "$tr_init" 'ri.Cmd_AddCommand( "renderer_compatibility"' "renderer_compatibility alias is registered"
	check_grep "$tr_init" 'ri.Cmd_AddCommand( "renderer_modern_safe"' "renderer_modern_safe is registered"
	check_grep "$tr_diag" 'R_RendererPrintCompatibilityWarnings' "compatibility warnings share one implementation"
	check_grep "$tr_diag" 'modern profile expects r_forwardPlus 1' "compatibility warns on broken Forward+ profile"
	check_grep "$tr_diag" 'modern profile expects r_forwardPlusDepthCull 1' "compatibility warns on missing Forward+ depth cull"
	check_grep "$tr_diag" 'quality/rt/experimental profile expects r_deferredGBuffer 1' "compatibility warns on missing sidecar G-buffer"
	check_grep "$tr_diag" 'renderer_modern_safe + vid_restart' "compatibility warns on stale deferred lighting in modern mode"
	check_grep "$tr_diag" 'r_niv_useGBuffer 1 needs r_deferredGBuffer 1' "compatibility warns on NIV G-buffer mismatch"
	check_grep "$tr_diag" 'r_vfgi_useGBuffer 1 needs r_deferredGBuffer 1' "compatibility warns on VFGI G-buffer mismatch"
	check_grep "$tr_diag" 'r_nvc 1 expects r_forwardPlus 1' "compatibility warns on NVC without Forward+"
	check_grep "$tr_diag" 'r_ndgi_compute 1 is reserved' "compatibility warns on reserved NDGI compute path"
	check_grep "$tr_diag" 'warnings  : %d' "renderer diagnostics print warning count"
	check_grep "$tr_diag" 'lighting   : mode=%d forwardPlus=%d' "renderer_status reports advanced lighting state"
	check_grep "$tr_diag" 'gi/neural' "renderer_status reports neural GI state"
	check_grep "$tr_diag" 'Renderer Subsystems' "renderer_subsystems prints subsystem table"
	check_grep "$tr_diag" '======== Renderer Health ========' "renderer_health command is implemented"
	check_grep "$tr_diag" 'health    : %s' "renderer_health reports a top-level status"
	check_grep "$tr_diag" 'framegraph : fbo=%s' "renderer_subsystems reports framegraph state"
	check_grep "$tr_diag" 'temporal   : taa=%d' "renderer_subsystems reports temporal state"
	check_grep "$tr_diag" 'contract  : clean=%s recovery=%s' "renderer_status reports mode-contract cleanliness"
	check_grep "$tr_diag" 'contract   : clean=%s recovery=%s' "renderer_subsystems reports mode-contract cleanliness"
	check_grep "$tr_diag" 'passes    : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' "renderer_status reports pass ownership"
	check_grep "$tr_diag" 'present   : swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d' "renderer_status reports presentation ownership"
	check_grep "$tr_diag" 'restart   : count=%u last=%s' "renderer_status reports swapchain restart ownership"
	check_grep "$tr_diag" 'restartAt : recent=%s ms=%d' "renderer_status reports swapchain restart recency"
	check_grep "$tr_diag" 'sources   : postFog=%s scene=%s luminance=%s' "renderer_status reports scene-source ownership"
	check_grep "$tr_diag" 'cubemaps  : runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=' "renderer_status reports cubemap readiness"
	check_grep "$tr_diag" 'passes     : inRenderPass=%s active=%s uiOverlay=%s sourcesHealthy=%s' "renderer_subsystems reports pass ownership"
	check_grep "$tr_diag" 'present    : swapchainHealthy=%s extent=%ux%u images=%u fullscreen=%d srgb=%d' "renderer_subsystems reports presentation ownership"
	check_grep "$tr_diag" 'restart    : count=%u last=%s' "renderer_subsystems reports swapchain restart ownership"
	check_grep "$tr_diag" 'restartAt  : recent=%s ms=%d' "renderer_subsystems reports swapchain restart recency"
	check_grep "$tr_diag" 'sources    : postFog=%s scene=%s luminance=%s' "renderer_subsystems reports scene-source ownership"
	check_grep "$tr_diag" 'cubemaps   : runtime=%s total=%d ready=%d incomplete=%d hdrFallback=%s localReady=%s debug=' "renderer_subsystems reports cubemap readiness"
	check_grep "renderers/vulkan/vk_frame_end.c" 'vk_end_frame_validate_post_process_chain' "frame-end path validates post-process chain ownership"
	check_grep "renderers/vulkan/vk_scene_pass.c" 'vk_scene_pass_validate_begin' "scene-pass helpers validate begin assumptions"
	check_grep "renderers/vulkan/vk_scene_pass.c" 'vk_scene_pass_validate_resume' "scene-pass helpers validate resume assumptions"
	check_grep "renderers/vulkan/vk_deferred_gbuffer.c" 'vk_dgb_validate_compute_break' "deferred path validates compute-break assumptions"
	check_grep "renderers/vulkan/vk_visibility_buffer.c" 'vk_visbuf_validate_compute_break' "visibility path validates compute-break assumptions"
	check_grep "renderers/vulkan/vk_postfx_passes.c" 'vk_oit_validate_pass_break' "OIT path validates pass-break assumptions"
	check_grep "renderers/vulkan/vk_scene_pass.c" 'vk_scene_pass_resume_framebuffer' "scene-pass helpers self-heal safe resume targets"
	check_grep "renderers/vulkan/vk_post_fog.c" 'vk_choose_post_fog_fallback_source' "post-fog chain self-heals null source drift"
	check_grep "renderers/vulkan/vk_presentation.c" 'vk_reset_post_fog_frame_state();' "swapchain restore rebinds post-fog state after resize/fullscreen"
	check_grep "renderers/vulkan/vk_presentation.c" 'vk_reset_presentation_runtime_state' "swapchain restore clears stale presentation runtime state after resize/fullscreen"
	check_grep "renderers/vulkan/tr_shade.c" 'R_SelectReadyCubemapIndexForPBRAt' "PBR cubemap path can remap to the nearest ready probe"
	check_grep "renderers/vulkan/tr_shade.c" 'R_HasReadyCubemapForPBR' "PBR cubemap path detects whether any local probe is actually ready"
	check_grep "renderers/vulkan/tr_shade.c" 'no ready local cubemap found; using HDR skybox fallback until map probes are complete' "PBR cubemap path can fall back to HDR skybox when all local probes are incomplete"
	check_grep "renderers/vulkan/vk_frame_submit.c" 'vk.cmd->swapchain_image_index >= vk.swapchain_image_count' "frame begin validates acquired swapchain image index after fullscreen restart"
	check_grep "renderers/vulkan/vk_frame_end.c" 'vk_end_frame_record_emergency_present' "frame-end path can emergency-present color data when gamma/post chain is unavailable"
	check_grep "renderers/vulkan/vk_forward_plus.c" 'vk_forward_plus_ensure_runtime' "Forward+ path can rebuild runtime resources in-place"
	check_grep "renderers/vulkan/vk_deferred_gbuffer.c" 'vk_deferred_gbuffer_ensure_runtime' "deferred path can rebuild runtime resources in-place"
	check_grep "renderers/vulkan/vk_visibility_buffer.c" 'vk_visibility_buffer_ensure_runtime' "visibility-buffer path can rebuild runtime resources in-place"

	if [[ -d "${RELEASE_DIR:-$ROOT/release}/base" ]]; then
		local release_base="${RELEASE_DIR:-$ROOT/release}/base"
		if [[ -f "$release_base/modern_vulkan.cfg" &&
			-f "$release_base/vulkan_overlay_deferred.cfg" &&
			-f "$release_base/vulkan_overlay_rtx.cfg" &&
			-f "$release_base/vulkan_overlay_hybrid1.cfg" ]]; then
			pass "release/base contains modern renderer cfgs"
		elif [[ "${IDTECH3_REQUIRE_RELEASE_CFGS:-0}" = "1" ]]; then
			fail "release/base missing one or more modern renderer cfgs"
		else
			echo "SKIP: release/base exists but is missing modern renderer cfgs; source packaging contract checked instead"
		fi
	else
		echo "SKIP: release/base not present; source packaging contract checked instead"
	fi

	pass "modern renderer profile source contract"
}

runtime_checks() {
	cd "$ROOT"

	command -v timeout >/dev/null 2>&1 || require_or_skip "timeout not available"
	[[ -n "${DISPLAY:-}" ]] || require_or_skip "DISPLAY not set for client runtime smoke"

	local client
	if ! client="$(find_client)"; then
		require_or_skip "idtech3 client not built (set IDTECH3_BIN, BUILD_DIR, or RELEASE_DIR)"
	fi

	local pack="$ROOT/docs/renderer_validation/devdata/rtest_base"
	[[ -f "$pack/default.cfg" ]] || fail "missing minimal pack default.cfg"
	[[ -f "$pack/maps/rtest_parity.bsp" ]] || fail "missing minimal pack map"
	[[ -f "$pack/vm/qagame.qvm" ]] || fail "missing minimal pack qagame"

	local temp
	temp="$(mktemp -d)"
	trap 'rm -rf "$temp"' EXIT
	mkdir -p "$temp/rtest_base"
	cp -a "$pack/." "$temp/rtest_base/"
	cp -f "$ROOT/config/modern_vulkan.cfg" "$temp/rtest_base/"
	cp -f "$ROOT/config/vulkan_overlay_deferred.cfg" "$temp/rtest_base/"
	cp -f "$ROOT/config/vulkan_overlay_rtx.cfg" "$temp/rtest_base/"
	cp -f "$ROOT/config/vulkan_overlay_hybrid1.cfg" "$temp/rtest_base/"

	local log="$temp/modern_renderer_runtime.log"
	local timeout_sec="${IDTECH3_RENDERER_RUNTIME_TIMEOUT:-45}"
	set +e
	timeout "$timeout_sec" "$client" \
		+set cl_renderer vulkan \
		+set fs_basepath "$temp" \
		+set fs_game rtest_base \
		+set vm_game 2 \
		+set sv_pure 0 \
		+set r_fullscreen 0 \
		+set net_enabled 0 \
		+set bot_enable 0 \
		+set com_hunkMegs 128 \
		+exec modern_vulkan.cfg \
		+vid_restart \
		+map rtest_parity \
		+renderer_profile \
		+renderer_status \
		+renderer_subsystems \
		+renderer_compatibility \
		+quit >"$log" 2>&1
	local rc=$?
	set -e

	if [[ "$rc" -ne 0 && "$rc" -ne 124 ]]; then
		if grep -Eiq 'Vulkan.*(not|fail|error)|SDL.*(fail|error)|GLimp_Init|Sys_Error' "$log"; then
			require_or_skip "client could not initialize Vulkan/display"
		fi
		echo "client runtime log:" >&2
		tail -80 "$log" >&2
		fail "modern renderer runtime smoke failed with rc=$rc"
	fi

	check_grep "$log" 'Renderer Profile' "runtime printed renderer_profile"
	check_grep "$log" 'active    : modern vulkan' "runtime selected modern Vulkan profile"
	check_grep "$log" 'Renderer Status' "runtime printed renderer_status"
	check_grep "$log" 'forward+  : enabled=1' "runtime reports Forward+ enabled"
	check_grep "$log" 'gbuffer   : cvar=1 fill=1' "runtime reports G-buffer sidecar enabled"
	check_grep "$log" 'temporal  : taa=1 motionVectors=1' "runtime reports TAA motion-vector path"
	check_grep "$log" 'lighting  : ssao=' "runtime reports lighting diagnostics"
	check_grep "$log" 'gi/neural : ndgi=' "runtime reports GI diagnostics"
	check_grep "$log" 'Renderer Subsystems' "runtime printed renderer_subsystems"
	check_grep "$log" 'framegraph : fbo=' "runtime reports framegraph subsystem"
	check_grep "$log" 'temporal   : taa=1' "runtime reports temporal subsystem"
	check_grep "$log" 'Renderer Compatibility' "runtime printed renderer_compatibility"
	check_grep "$log" 'warnings: 0' "runtime reports no modern-profile compatibility warnings"

	pass "modern renderer profile client runtime smoke"
}

case "$MODE" in
	source)
		source_checks
		;;
	runtime)
		runtime_checks
		;;
	all)
		source_checks
		runtime_checks
		;;
	*)
		echo "usage: $0 [source|runtime|all]" >&2
		exit 2
		;;
esac
