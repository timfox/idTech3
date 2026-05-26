#!/usr/bin/env bash
# Regression checks for the Vulkan ImGui inspector toggle path.
# These source guards cover UI/input behavior that is hard to exercise in headless CI.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

CMAKE_FILE="$PROJECT_ROOT/CMakeLists.txt"
CL_KEYS="$PROJECT_ROOT/src/client/cl_keys.c"
CL_MAIN="$PROJECT_ROOT/src/client/cl_main.c"
TR_INIT="$PROJECT_ROOT/src/renderers/vulkan/tr_init.c"
TR_CMDS="$PROJECT_ROOT/src/renderers/vulkan/tr_cmds.c"
VK_IMGUI="$PROJECT_ROOT/src/renderers/vulkan/inspector/vk_imgui.cpp"
VK_IMGUI_VULKAN="$PROJECT_ROOT/src/renderers/vulkan/inspector/vk_imgui_vulkan.cpp"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

require_file() {
	[ -f "$1" ] || fail "missing file: $1"
}

require_pattern() {
	local file="$1"
	local pattern="$2"
	local context="$3"

	PATTERN="$pattern" perl -0ne '
		my $pattern = $ENV{"PATTERN"};
		exit(!(/$pattern/s));
	' "$file" || fail "$context"
}

require_literal_count() {
	local file="$1"
	local literal="$2"
	local expected="$3"
	local context="$4"
	local count

	count="$(FILE="$file" LITERAL="$literal" perl -0ne '
		my $needle = $ENV{"LITERAL"};
		my $count = () = /\Q$needle\E/g;
		print $count;
	' "$file")"
	[ "$count" = "$expected" ] || fail "$context: expected $expected occurrences of '$literal', found $count"
}

require_literal_order() {
	local file="$1"
	local first="$2"
	local second="$3"
	local context="$4"

	FILE="$file" FIRST="$first" SECOND="$second" perl -0ne '
		my $first = index($_, $ENV{"FIRST"});
		my $second = index($_, $ENV{"SECOND"});
		exit(!($first >= 0 && $second >= 0 && $first < $second));
	' "$file" || fail "$context"
}

for file in "$CMAKE_FILE" "$CL_KEYS" "$CL_MAIN" "$TR_INIT" "$TR_CMDS" "$VK_IMGUI" "$VK_IMGUI_VULKAN"; do
	require_file "$file"
done

# The client-side toggle must only compile when both ImGui and the Vulkan renderer API are active.
require_pattern "$CMAKE_FILE" \
	'if\s*\(\s*USE_VULKAN\s*\).*?TARGET_COMPILE_DEFINITIONS\s*\(\s*client\s+PRIVATE\s+USE_VULKAN_API\s*\).*?if\s*\(\s*USE_IMGUI\s+AND\s+NOT\s+APPLE\s*\).*?target_compile_definitions\s*\(\s*client\s+PRIVATE\s+USE_IMGUI\s*\)' \
	"client must receive USE_VULKAN_API and USE_IMGUI definitions for the toggle path"

# F11 should enqueue the command once on initial keydown only, and never on dedicated servers.
require_pattern "$CL_KEYS" \
	'#if\s+defined\(USE_IMGUI\)\s*&&\s*defined\(USE_VULKAN_API\).*?if\s*\(\s*key\s*==\s*K_F11\s*&&\s*keys\s*\[\s*key\s*\]\.repeats\s*==\s*1\s*&&\s*\(\s*!\s*com_dedicated\s*\|\|\s*!\s*com_dedicated->integer\s*\)\s*\)\s*\{.*?Cbuf_ExecuteText\s*\(\s*EXEC_APPEND\s*,\s*"toggle_imgui\\n"\s*\)\s*;.*?return\s*;.*?#endif' \
	"F11 toggle must be repeat-filtered, non-dedicated, and compile-gated"

# The command must flip r_imgui through the cvar system and unregister during client shutdown.
require_pattern "$CL_MAIN" \
	'static\s+void\s+CL_ToggleImgui_f\s*\(\s*void\s*\).*?if\s*\(\s*com_dedicated\s*&&\s*com_dedicated->integer\s*\)\s*\{[^}]*return\s*;[^}]*\}.*?Cvar_Get\s*\(\s*"r_imgui"\s*,\s*"1"\s*,\s*CVAR_ARCHIVE_ND\s*\).*?on\s*=\s*cv->integer\s*\?\s*0\s*:\s*1\s*;.*?Cvar_SetValue\s*\(\s*"r_imgui"\s*,\s*\(float\)on\s*\)' \
	"toggle_imgui command must flip archived r_imgui and no-op on dedicated servers"
require_pattern "$CL_MAIN" \
	'#if\s+defined\(USE_IMGUI\)\s*&&\s*defined\(USE_VULKAN_API\)\s*Cmd_AddCommand\s*\(\s*"toggle_imgui"\s*,\s*CL_ToggleImgui_f\s*\)\s*;\s*#endif' \
	"toggle_imgui must be registered behind the same compile gate"
require_pattern "$CL_MAIN" \
	'#if\s+defined\(USE_IMGUI\)\s*&&\s*defined\(USE_VULKAN_API\)\s*Cmd_RemoveCommand\s*\(\s*"toggle_imgui"\s*\)\s*;\s*#endif' \
	"toggle_imgui must be removed during client shutdown"

# The renderer should have one authoritative cvar registration/log path.
require_literal_count "$TR_INIT" 'ri.Cvar_Get( "r_imgui", "1", CVAR_ARCHIVE_ND )' 1 \
	"r_imgui should only be registered once"
require_pattern "$TR_INIT" \
	'ri\.Cvar_SetDescription\s*\(\s*r_imgui\s*,[^;]*F11[^;]*\)\s*;\s*ri\.Cvar_CheckRange\s*\(\s*r_imgui\s*,\s*"0"\s*,\s*"1"\s*,\s*CV_INTEGER\s*\)\s*;\s*ri\.Printf\s*\(\s*PRINT_ALL\s*,\s*"\[VK\]\[imgui\] debug inspector r_imgui=%d \(F11 toggles when enabled\)\\n"\s*,\s*r_imgui->integer\s*\)' \
	"r_imgui must keep range validation, F11 documentation, and startup logging"

# Per-frame ImGui CPU/render work must be skipped when r_imgui is off.
require_pattern "$TR_CMDS" \
	'if\s*\(\s*stereoFrame\s*==\s*STEREO_CENTER\s*&&\s*r_imgui\s*&&\s*r_imgui->integer\s*\)\s*\{\s*VkImgui_Initialize\s*\(\s*\)\s*;\s*VkImgui_BeginFrame\s*\(\s*\)\s*;\s*\}' \
	"RE_BeginFrame must gate ImGui initialize/begin on center frame and r_imgui"
require_pattern "$TR_CMDS" \
	'if\s*\(\s*r_imgui\s*&&\s*r_imgui->integer\s*\)\s*\{\s*VkImgui_Draw\s*\(\s*\)\s*;\s*\}' \
	"RE_EndFrame must gate ImGui draw on r_imgui"

# Menu actions should continue to enqueue engine commands, not call shutdown or screenshot paths directly.
require_pattern "$VK_IMGUI" \
	'ImGui::MenuItem\s*\(\s*"Screenshot \(JPEG\)"\s*\).*?ri\.Cmd_ExecuteText\s*\(\s*EXEC_APPEND\s*,\s*"screenshotJPEG silent\\n"\s*\).*?ImGui::MenuItem\s*\(\s*"Toggle console"\s*\).*?ri\.Cmd_ExecuteText\s*\(\s*EXEC_APPEND\s*,\s*"toggleconsole\\n"\s*\).*?ImGui::MenuItem\s*\(\s*"Quit"\s*\).*?ri\.Cmd_ExecuteText\s*\(\s*EXEC_APPEND\s*,\s*"quit\\n"\s*\)' \
	"ImGui File menu must enqueue screenshot, console, and quit commands"
require_pattern "$VK_IMGUI" \
	'if\s*\(\s*r_imgui\s*\)\s*\{.*?bool\s+riOn\s*=\s*r_imgui->integer\s*!=\s*0\s*;.*?ImGui::Checkbox\s*\(\s*"Inspector overlay \(r_imgui\)"\s*,\s*&riOn\s*\).*?ri\.Cvar_SetValue\s*\(\s*"r_imgui"\s*,\s*riOn\s*\?\s*1\.0f\s*:\s*0\.0f\s*\)' \
	"Developer menu must toggle r_imgui through the renderer import cvar API"

# Overlay recording must bail out until backend/context/swapchain/draw data are valid and r_imgui is enabled.
require_literal_order "$VK_IMGUI_VULKAN" 'if ( !g_vkImguiBackendReady || !vkImguiState.active )' 'if ( !r_imgui || !r_imgui->integer )' \
	"overlay pass must check backend readiness before r_imgui"
require_literal_order "$VK_IMGUI_VULKAN" 'if ( !r_imgui || !r_imgui->integer )' 'if ( ImGui::GetCurrentContext() == nullptr )' \
	"overlay pass must skip before touching ImGui context when r_imgui is disabled"
require_literal_order "$VK_IMGUI_VULKAN" 'if ( vk.render_pass.overlay_compose == VK_NULL_HANDLE' 'ImGui_ImplVulkan_RenderDrawData( dd, vk.cmd->command_buffer );' \
	"overlay pass must validate framebuffer/extent before rendering draw data"
require_pattern "$VK_IMGUI_VULKAN" \
	'SDL_GetWindowSize\s*\(\s*SDL_window\s*,\s*&winW\s*,\s*&winH\s*\).*?ix\s*\*=\s*\(float\)glConfig\.vidWidth\s*/\s*\(float\)winW\s*;.*?iy\s*\*=\s*\(float\)glConfig\.vidHeight\s*/\s*\(float\)winH\s*;' \
	"SDL mouse coordinates must stay scaled to render resolution for HiDPI windows"

echo "PASS: ImGui toggle and overlay source guards"
