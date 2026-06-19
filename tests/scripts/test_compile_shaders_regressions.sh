#!/usr/bin/env bash
# Regression coverage for scripts/compile_shaders.sh.
# Uses a mocked shader tree and glslangValidator so script wiring can be
# exercised without Vulkan SDK binaries or real shader compilation.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCRIPT_UNDER_TEST="${1:-$PROJECT_ROOT/scripts/compile_shaders.sh}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_file_exists() {
	local path="$1"
	[ -f "$path" ] || fail "missing file: $path"
}

assert_contains_file() {
	local path="$1"
	local needle="$2"
	local context="$3"
	assert_file_exists "$path"
	if ! grep -Fq -- "$needle" "$path"; then
		fail "$context: expected '$needle' in $path"
	fi
}

assert_not_contains_file() {
	local path="$1"
	local needle="$2"
	local context="$3"
	assert_file_exists "$path"
	if grep -Fq -- "$needle" "$path"; then
		fail "$context: unexpected '$needle' in $path"
	fi
}

write_file() {
	local path="$1"
	local text="$2"
	mkdir -p "$(dirname "$path")"
	printf '%s\n' "$text" > "$path"
}

make_mock_glslang() {
	local bin_dir="$1"
	local log_file="$2"
	mkdir -p "$bin_dir"
	cat > "$bin_dir/glslangValidator" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "--version" ]]; then
	echo "Mock glslangValidator 1.0"
	exit 0
fi

stage=""
output=""
source=""
while [[ $# -gt 0 ]]; do
	case "$1" in
		-S)
			stage="${2:-}"
			shift 2
			;;
		-o)
			output="${2:-}"
			shift 2
			;;
		-*)
			shift
			;;
		*)
			source="$1"
			shift
			;;
	esac
done

[[ -n "$stage" ]] || { echo "missing -S stage" >&2; exit 2; }
[[ -n "$output" ]] || { echo "missing -o output" >&2; exit 2; }
[[ -n "$source" ]] || { echo "missing source" >&2; exit 2; }

mkdir -p "$(dirname "$output")"
printf 'SPV:%s:%s\n' "$stage" "$(basename "$source")" > "$output"
printf '%s|%s|%s\n' "$stage" "$source" "$output" >> "${MOCK_GLSLANG_LOG:?}"
EOF
	chmod +x "$bin_dir/glslangValidator"
	printf '%s\n' "$log_file"
}

make_shader_fixture() {
	local case_dir="$1"
	local shader_dir="$case_dir/src/renderers/vulkan/shaders"
	local tools_dir="$shader_dir/tools"
	local glsl_dir="$shader_dir/glsl"
	local spirv_dir="$shader_dir/spirv"

	mkdir -p "$case_dir/scripts" "$tools_dir" "$glsl_dir" "$spirv_dir"
	cp "$SCRIPT_UNDER_TEST" "$case_dir/scripts/compile_shaders.sh"
	chmod +x "$case_dir/scripts/compile_shaders.sh"
	write_file "$case_dir/CMakeLists.txt" "cmake_minimum_required(VERSION 3.24)"

	for helper in bin2hex bindshader; do
		write_file "$tools_dir/${helper}.c" "int main(void) { return 0; }"
		write_file "$tools_dir/$helper" "#!/usr/bin/env bash"
		chmod +x "$tools_dir/$helper"
	done

	for shader in \
		gen_frag.tmpl \
		gen_vert.tmpl \
		light_frag.tmpl \
		light_vert.tmpl \
		sanity.frag \
		sanity.geom \
		sanity.vert \
		volumetric/volumetric_fog.vert \
		volumetric/volumetric_fog.frag \
		volumetric/volumetric_fog.comp \
		volumetric/fluid_advect.comp \
		volumetric/fluid_divergence.comp \
		volumetric/fluid_pressure.comp \
		volumetric/fluid_gradient.comp \
		volumetric/depth_resolve_msaa.comp \
		postfx/luminance.comp \
		vegetation_wind.comp \
		terrain/cbt_terrain.comp \
		forward_plus_tile_cull.comp \
		terrain/terrain.vert \
		terrain/terrain.frag \
		rtx_demo.rgen \
		rtx_demo.rmiss \
		rtx_demo.rchit
	do
		write_file "$glsl_dir/$shader" "// fixture shader: $shader"
	done

	write_file "$spirv_dir/shader_data.c" "old shader data"
	write_file "$spirv_dir/shader_binding.c" "old shader binding"
}

run_compile_shaders() {
	local case_dir="$1"
	local output_file="$2"
	shift 2
	local mock_bin="$case_dir/mock-bin"
	local mock_log="$case_dir/glslang.log"
	make_mock_glslang "$mock_bin" "$mock_log" >/dev/null

	env -i \
		PATH="$mock_bin:/usr/bin:/bin" \
		HOME="$TMP_ROOT/home" \
		GLSLANG_VALIDATOR="$mock_bin/glslangValidator" \
		MOCK_GLSLANG_LOG="$mock_log" \
		bash "$case_dir/scripts/compile_shaders.sh" "$@" > "$output_file" 2>&1
}

first_matching_file() {
	local pattern="$1"
	local match
	for match in $pattern; do
		if [[ -f "$match" ]]; then
			printf '%s\n' "$match"
			return 0
		fi
	done
	return 1
}

if [ ! -f "$SCRIPT_UNDER_TEST" ]; then
	fail "compile_shaders script not found: $SCRIPT_UNDER_TEST"
fi

TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT
mkdir -p "$TMP_ROOT/home"

# Argument validation must fail before tool discovery or shader-tree side effects.
case_args="$TMP_ROOT/case-missing-generated-dir"
make_shader_fixture "$case_args"
if run_compile_shaders "$case_args" "$case_args/output.log" --generated-dir; then
	fail "--generated-dir without a value should fail"
fi
assert_contains_file "$case_args/output.log" "--generated-dir requires a path" "missing generated-dir argument"

# Happy path with --apply: validates custom generated output, metadata, RTX embed emission,
# and safe backup of tracked shader blobs before replacement.
case_apply="$TMP_ROOT/case-apply"
make_shader_fixture "$case_apply"
run_compile_shaders "$case_apply" "$case_apply/output.log" --generated-dir generated-shaders --apply

generated_dir="$case_apply/generated-shaders"
spirv_dir="$case_apply/src/renderers/vulkan/shaders/spirv"
rtx_inc="$case_apply/src/renderers/vulkan/vk_rtx_demo_spirv.inc"
glslang_log="$case_apply/glslang.log"

assert_contains_file "$case_apply/output.log" "Using glslangValidator at $case_apply/mock-bin/glslangValidator" "validator path log"
assert_contains_file "$case_apply/output.log" "Wrote $rtx_inc" "rtx embed write log"
assert_contains_file "$case_apply/output.log" "Backed up previous shader blobs to" "apply backup log"
assert_contains_file "$case_apply/output.log" "Shaders compiled into $generated_dir" "generated-dir completion log"

assert_contains_file "$generated_dir/shader_build_meta.txt" "glslang_validator_version=Mock glslangValidator 1.0" "metadata validator version"
assert_contains_file "$generated_dir/shader_data.c" "const unsigned char rtx_demo_rgen_spv" "generated shader data includes raygen"
assert_contains_file "$generated_dir/shader_binding.c" "vk.modules.forward_plus_tile_cull_cs = SHADER_MODULE( forward_plus_tile_cull_cs );" "generated binding includes Forward+ compute"
assert_contains_file "$rtx_inc" "Auto-generated by scripts/compile_shaders.sh" "rtx inc generated comment"
assert_contains_file "$rtx_inc" "static const uint8_t vk_rtx_demo_rgen_spv[]" "rtx raygen embed array"
assert_contains_file "$rtx_inc" "#define VK_RTX_DEMO_RCHIT_SPV_SIZE" "rtx closest-hit size macro"

assert_contains_file "$glslang_log" "rgen|glsl/rtx_demo.rgen" "raygen compile stage"
assert_contains_file "$glslang_log" "rmiss|glsl/rtx_demo.rmiss" "miss compile stage"
assert_contains_file "$glslang_log" "rchit|glsl/rtx_demo.rchit" "closest-hit compile stage"
assert_contains_file "$glslang_log" "comp|glsl/forward_plus_tile_cull.comp" "Forward+ compute compile stage"

assert_contains_file "$spirv_dir/shader_data.c" "const unsigned char rtx_demo_rgen_spv" "applied shader data"
assert_not_contains_file "$spirv_dir/shader_data.c" "old shader data" "applied shader data replacement"
assert_contains_file "$spirv_dir/shader_binding.c" "vk_bind_generated_shaders" "applied shader bindings"
assert_not_contains_file "$spirv_dir/shader_binding.c" "old shader binding" "applied shader binding replacement"

backup_data="$(first_matching_file "$generated_dir"/backups/*/shader_data.c.bak)" || fail "missing shader_data backup"
backup_binding="$(first_matching_file "$generated_dir"/backups/*/shader_binding.c.bak)" || fail "missing shader_binding backup"
assert_contains_file "$backup_data" "old shader data" "shader_data backup preserves previous blob"
assert_contains_file "$backup_binding" "old shader binding" "shader_binding backup preserves previous blob"

if compgen -G "$generated_dir/tmp_*.spv" >/dev/null; then
	fail "temporary SPIR-V files should be removed from generated dir"
fi

echo "PASS: test_compile_shaders_regressions"
