#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# Usage: GLSLANG_VALIDATOR=/path/to/glslangValidator ./scripts/compile_shaders.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$SCRIPT_DIR/CMakeLists.txt" ]]; then
  PROJECT_ROOT="$SCRIPT_DIR"
elif [[ -f "$SCRIPT_DIR/../CMakeLists.txt" ]]; then
  PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
else
  echo "Error: Could not find CMakeLists.txt near script location." >&2
  exit 1
fi

SHADER_DIR="$PROJECT_ROOT/src/renderers/vulkan/shaders"
SPIRV_DIR="$SHADER_DIR/spirv"
TOOLS_DIR="$SHADER_DIR/tools"

APPLY=0
GENERATED_DIR="${GENERATED_SPIRV_DIR:-$SPIRV_DIR/generated}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --apply|-a)
      APPLY=1
      shift
      ;;
    --generated-dir|-o)
      if [[ -z "${2:-}" ]]; then
        echo "--generated-dir requires a path" >&2
        exit 1
      fi
      GENERATED_DIR="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

# Resolve GENERATED_DIR to absolute path so Python (cwd=SHADER_DIR) writes to the correct location
if [[ "$GENERATED_DIR" != /* ]]; then
  GENERATED_DIR="$PROJECT_ROOT/$GENERATED_DIR"
fi

GLSLANG_VALIDATOR="${GLSLANG_VALIDATOR:-$(command -v glslangValidator || true)}"
if [[ -z "$GLSLANG_VALIDATOR" ]]; then
  echo "Error: glslangValidator was not found in PATH. Install the Vulkan SDK or set GLSLANG_VALIDATOR." >&2
  exit 1
fi

PYTHON="${PYTHON:-$(command -v python3 || command -v python || true)}"
if [[ -z "$PYTHON" ]]; then
  echo "Error: python3 is required but was not found in PATH." >&2
  exit 1
fi

CC="${CC:-cc}"

BIN2HEX="$TOOLS_DIR/bin2hex"
BINDSHADER="$TOOLS_DIR/bindshader"
for helper in bin2hex bindshader; do
  src="$TOOLS_DIR/${helper}.c"
  out="$TOOLS_DIR/$helper"
  if [[ ! -f "$src" ]]; then
    echo "Error: shader helper source missing at $src" >&2
    exit 1
  fi
  if [[ ! -x "$out" || "$src" -nt "$out" ]]; then
    printf "Compiling shader helper %s\n" "$helper"
    "$CC" -std=c11 -O2 "$src" -o "$out"
  fi
done

mkdir -p "$SPIRV_DIR"
mkdir -p "$GENERATED_DIR"
rm -f "$GENERATED_DIR/shader_data.c" "$GENERATED_DIR/shader_binding.c" "$GENERATED_DIR"/tmp_*.spv

echo "Using glslangValidator at $GLSLANG_VALIDATOR"

( 
  cd "$SHADER_DIR"
  export GLSLANG_VALIDATOR
  export BIN2HEX
  export BINDSHADER
  export GENERATED_DIR
  export PROJECT_ROOT

  "$PYTHON" - <<'PY'
import os
import subprocess
import sys
from pathlib import Path

glsl_dir = Path("glsl")
spirv_dir = Path("spirv")
generated_dir = Path(os.environ.get("GENERATED_DIR", str(spirv_dir / "generated")))
data_file = generated_dir / "shader_data.c"
binding_file = generated_dir / "shader_binding.c"

if not glsl_dir.is_dir():
    sys.exit(f"GLSL directory not found: {glsl_dir.resolve()}")

generated_dir.mkdir(parents=True, exist_ok=True)

for path in (data_file, binding_file):
    if path.exists():
        path.unlink()

glslang = Path(os.environ["GLSLANG_VALIDATOR"])

bindings = []
task_counter = 0
rtx_spv_bytes = {}
grtx_spv_bytes = {}
pathtrace_spv_bytes = {}
hybrid1_spv_bytes = {}
raygun_spv_bytes = {}

def append_shader_data(spv_path, array_name):
    data = spv_path.read_bytes()
    with data_file.open("a", encoding="utf-8") as f:
        f.write(f"const unsigned char {array_name}[{len(data)}] = {{\n")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            bytes_text = ", ".join(f"0x{byte:02X}" for byte in chunk)
            f.write(f"\t{bytes_text}")
            if offset + 16 < len(data):
                f.write(",")
            f.write("\n")
        f.write("};\n")

def compile_shader(stage, source, array_name, binding_expr=None, defines="", rtx_collect=False, grtx_collect=False, pathtrace_collect=False, hybrid1_collect=False, raygun_collect=False):
    global task_counter
    input_path = glsl_dir / source
    if not input_path.is_file():
        sys.exit(f"Shader source missing: {input_path}")
    tmp_spv = generated_dir / f"tmp_{task_counter:04d}.spv"
    defines_list = defines.split() if defines else []
    # --target-env vulkan1.2: explicit Vulkan context (avoids OpenGL/ES semantics; 64-bit
    # fragment outputs are disallowed in OpenGL ARB_gpu_shader_fp64 but glslang still
    # rejects them even for Vulkan target as of glslang 15.x)
    cmd = [str(glslang), "-S", stage, "-V", "--target-env", "vulkan1.2", "-o", str(tmp_spv), str(input_path)] + defines_list
    print(f"  compiling {array_name}")
    subprocess.run(cmd, check=True)
    if rtx_collect:
        rtx_spv_bytes[array_name] = tmp_spv.read_bytes()
    if grtx_collect:
        grtx_spv_bytes[array_name] = tmp_spv.read_bytes()
    if pathtrace_collect:
        pathtrace_spv_bytes[array_name] = tmp_spv.read_bytes()
    if hybrid1_collect:
        hybrid1_spv_bytes[array_name] = tmp_spv.read_bytes()
    if raygun_collect:
        raygun_spv_bytes[array_name] = tmp_spv.read_bytes()
    append_shader_data(tmp_spv, array_name)
    if binding_expr:
        bindings.append((binding_expr, array_name))
    try:
        tmp_spv.unlink()
    except FileNotFoundError:
        pass
    task_counter += 1

def join_flags(*flags):
    return " ".join(flag for flag in flags if flag).strip()

def join_indexes(base, indexes):
    return base + "".join(f"[{idx}]" for idx in indexes)

def with_forward_plus_frag(defines):
    """PBR gen_frag variants bind set=18 Forward+ SSBOs when USE_FORWARD_PLUS_FRAG is set."""
    if defines and "-DUSE_VK_PBR" in defines:
        return defines + " -DUSE_FORWARD_PLUS_FRAG"
    return defines

def with_forward_plus_vert(defines):
    """PBR gen_vert outputs world position for Forward+ tile shading when set."""
    if defines and "-DUSE_VK_PBR" in defines:
        return defines + " -DUSE_FORWARD_PLUS_WORLD_POS"
    return defines

def compile_individual_shaders():
    print("Compiling standalone GLSL stages...")
    for stage, ext in (("vert", ".vert"), ("frag", ".frag"), ("geom", ".geom")):
        for path in sorted(glsl_dir.glob(f"*{ext}")):
            base = path.name[: -len(ext)]
            array_name = f"{base}_{stage}_spv"
            compile_shader(stage, path.name, array_name, defines="")

    # NV mesh font shader (optional; requires glslang with GL_NV_mesh_shader varyings)
    try:
        compile_shader("mesh", "mesh_nv_ui_vector_font.mesh", "mesh_nv_ui_vector_font_mesh")
    except subprocess.CalledProcessError:
        print("WARN: mesh_nv_ui_vector_font.mesh skipped (NV mesh varyings unsupported by this glslang)")

def compile_template_shaders():
    print("Compiling template-driven shaders...")
    compile_shader("frag", "gen_frag.tmpl", "frag_tx0_df", defines="-DUSE_CLX_IDENT -DUSE_ATEST -DUSE_DF")

    # flowmap water shaders (flow vectors offset texture UVs)
    compile_shader("frag", "gen_frag.tmpl", "frag_tx0_flowmap", binding_expr="vk.modules.frag.flowmap[0]", defines="-DUSE_FLOWMAP -DUSE_ATEST")
    compile_shader("frag", "gen_frag.tmpl", "frag_tx0_flowmap_fog", binding_expr="vk.modules.frag.flowmap[1]", defines="-DUSE_FLOWMAP -DUSE_ATEST -DUSE_FOG")

    compile_shader("vert", "light_vert.tmpl", "vert_light")
    compile_shader("vert", "light_vert.tmpl", "vert_light_fog", defines="-DUSE_FOG")
    compile_shader("frag", "light_frag.tmpl", "frag_light")
    compile_shader("frag", "light_frag.tmpl", "frag_light_fog", defines="-DUSE_FOG")
    compile_shader("frag", "light_frag.tmpl", "frag_light_line", defines="-DUSE_LINE")
    compile_shader("frag", "light_frag.tmpl", "frag_light_line_fog", defines="-DUSE_LINE -DUSE_FOG")

    compile_shader("frag", "gen_frag.tmpl", "frag_tx0_ent", defines="-DUSE_ENT_COLOR -DUSE_ATEST")
    compile_shader("frag", "gen_frag.tmpl", "frag_tx0_ent_fog", defines="-DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG")
    compile_shader("frag", "gen_frag.tmpl", "frag_pbr_tx0_ent", defines=with_forward_plus_frag("-DUSE_ENT_COLOR -DUSE_ATEST -DUSE_VK_PBR"))
    compile_shader("frag", "gen_frag.tmpl", "frag_pbr_tx0_ent_fog", defines=with_forward_plus_frag("-DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG -DUSE_VK_PBR"))

    mode_flags = ["-DUSE_CLX_IDENT", "-DUSE_FIXED_COLOR"]
    mode_ids = ["ident1", "fixed"]
    pbr_flags = ["", "-DUSE_VK_PBR"]
    pbr_ids = ["", "pbr_"]
    tx_flags = ["", "-DUSE_TX1", "-DUSE_TX2"]
    tx_ids = ["tx0", "tx1", "tx2"]
    env_flags = ["", "-DUSE_ENV"]
    env_ids = ["", "_env"]
    fog_flags = ["", "-DUSE_FOG"]
    fog_ids = ["", "_fog"]
    cl_flags = ["", "-DUSE_CL1", "-DUSE_CL2"]
    cl_ids = ["", "cl", "cl"]

    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags) - 1):
            for m in range(len(mode_flags)):
                for k in range(len(env_flags)):
                    for l in range(len(fog_flags)):
                        defines = with_forward_plus_vert(join_flags(pbr_flags[i], tx_flags[j], mode_flags[m], env_flags[k], fog_flags[l]))
                        name = f"vert_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{env_ids[k]}{fog_ids[l]}"
                        binding = join_indexes(f"vk.modules.vert.{mode_ids[m]}", [i, j, k, l])
                        compile_shader("vert", "gen_vert.tmpl", name, binding_expr=binding, defines=defines)

    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags) - 1):
            for m in range(len(mode_flags)):
                for k in range(len(fog_flags)):
                    extra = "-DUSE_ATEST" if j == 0 else ""
                    defines = with_forward_plus_frag(join_flags(pbr_flags[i], tx_flags[j], mode_flags[m], fog_flags[k], extra))
                    name = f"frag_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{fog_ids[k]}"
                    binding = join_indexes(f"vk.modules.frag.{mode_ids[m]}", [i, j, k])
                    compile_shader("frag", "gen_frag.tmpl", name, binding_expr=binding, defines=defines)

    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(env_flags)):
                for l in range(len(fog_flags)):
                    defines = with_forward_plus_vert(join_flags(pbr_flags[i], tx_flags[j], env_flags[k], fog_flags[l]))
                    name = f"vert_{pbr_ids[i]}{tx_ids[j]}{env_ids[k]}{fog_ids[l]}"
                    binding = join_indexes("vk.modules.vert.gen", [i, j, 0, k, l])
                    compile_shader("vert", "gen_vert.tmpl", name, binding_expr=binding, defines=defines)
                    if j != 0:
                        defines_cl = with_forward_plus_vert(join_flags(pbr_flags[i], tx_flags[j], cl_flags[j], env_flags[k], fog_flags[l]))
                        name_cl = f"vert_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{env_ids[k]}{fog_ids[l]}"
                        binding_cl = join_indexes("vk.modules.vert.gen", [i, j, 1, k, l])
                        compile_shader("vert", "gen_vert.tmpl", name_cl, binding_expr=binding_cl, defines=defines_cl)

    # PBR glTF GPU skinning/morph: same vertex layout as gen[i][j][*][k][l] + USE_GLTF_GPU_SKIN
    # Sixth index: 0=bind-pose tangent, 1=Gram–Schmidt (GLTF_GPU_TANGENT_FIX), 2=topology+MikkT-inspired (TOPO)
    for i in range(len(pbr_flags)):
        if pbr_flags[i] != "-DUSE_VK_PBR":
            continue
        for j in range(len(tx_flags)):
            for k in range(len(env_flags)):
                for l in range(len(fog_flags)):
                    for tan_mode in (0, 1, 2):
                        tan_mode_def = f"-DGLTF_GPU_TANGENT_MODE={tan_mode}"
                        if tan_mode == 0:
                            tan_extra = ""
                            tan_suffix = ""
                        elif tan_mode == 1:
                            tan_extra = "-DGLTF_GPU_TANGENT_FIX"
                            tan_suffix = "_tfix"
                        else:
                            tan_extra = "-DGLTF_GPU_TANGENT_FIX -DGLTF_GPU_TANGENT_TOPO"
                            tan_suffix = "_ttopo"
                        defines = with_forward_plus_vert(
                            join_flags(
                                pbr_flags[i], "-DUSE_GLTF_GPU_SKIN", tan_mode_def, tan_extra, tx_flags[j], env_flags[k], fog_flags[l]
                            )
                        )
                        name = f"vert_gltfgpu_{pbr_ids[i]}{tx_ids[j]}{env_ids[k]}{fog_ids[l]}{tan_suffix}"
                        binding = join_indexes("vk.modules.vert.gen_gltf_gpu", [i, j, 0, k, l, tan_mode])
                        compile_shader("vert", "gen_vert.tmpl", name, binding_expr=binding, defines=defines)
                        if j != 0:
                            defines_cl = with_forward_plus_vert(
                                join_flags(
                                    pbr_flags[i], "-DUSE_GLTF_GPU_SKIN", tan_mode_def, tan_extra, tx_flags[j], cl_flags[j], env_flags[k], fog_flags[l]
                                )
                            )
                            name_cl = f"vert_gltfgpu_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{env_ids[k]}{fog_ids[l]}{tan_suffix}"
                            binding_cl = join_indexes("vk.modules.vert.gen_gltf_gpu", [i, j, 1, k, l, tan_mode])
                            compile_shader("vert", "gen_vert.tmpl", name_cl, binding_expr=binding_cl, defines=defines_cl)

    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(fog_flags)):
                extra = "-DUSE_ATEST" if j == 0 else ""
                defines = with_forward_plus_frag(join_flags(pbr_flags[i], tx_flags[j], fog_flags[k], extra))
                name = f"frag_{pbr_ids[i]}{tx_ids[j]}{fog_ids[k]}"
                binding = join_indexes("vk.modules.frag.gen", [i, j, 0, k])
                compile_shader("frag", "gen_frag.tmpl", name, binding_expr=binding, defines=defines)
                if j != 0:
                    defines_cl = with_forward_plus_frag(join_flags(pbr_flags[i], tx_flags[j], cl_flags[j], fog_flags[k]))
                    name_cl = f"frag_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{fog_ids[k]}"
                    binding_cl = join_indexes("vk.modules.frag.gen", [i, j, 1, k])
                    compile_shader("frag", "gen_frag.tmpl", name_cl, binding_expr=binding_cl, defines=defines_cl)

compile_individual_shaders()
compile_template_shaders()

# HDR64 (64-bit) shader variants: glslang applies OpenGL/ES restriction (ARB_gpu_shader_fp64
# disallows 64-bit fragment outputs) even in Vulkan target. Vulkan SPIR-V supports 64-bit
# formats (VK_FORMAT_R64G64B64A64_SFLOAT). When glslang adds Vulkan-specific support for
# dvec4 fragment outputs, re-enable compile_hdr64_shaders() and return RGBA64F from get_hdr_format.

compile_shader("vert", "volumetric/volumetric_fog.vert", "volumetric_fog_vs", binding_expr="vk.modules.volumetric_fog_vs")
compile_shader("frag", "volumetric/volumetric_fog.frag", "volumetric_fog_fs", binding_expr="vk.modules.volumetric_fog_fs")
compile_shader("comp", "volumetric/volumetric_fog.comp", "volumetric_fog_cs", binding_expr="vk.modules.volumetric_fog_cs")
compile_shader("comp", "volumetric/fluid_advect.comp", "fluid_advect_cs", binding_expr="vk.modules.fluid_advect_cs")
compile_shader("comp", "volumetric/fluid_divergence.comp", "fluid_divergence_cs", binding_expr="vk.modules.fluid_divergence_cs")
compile_shader("comp", "volumetric/fluid_pressure.comp", "fluid_pressure_cs", binding_expr="vk.modules.fluid_pressure_cs")
compile_shader("comp", "volumetric/fluid_gradient.comp", "fluid_gradient_cs", binding_expr="vk.modules.fluid_gradient_cs")
compile_shader("comp", "volumetric/depth_resolve_msaa.comp", "volumetric_depth_resolve_msaa_cs", binding_expr="vk.modules.volumetric_depth_resolve_msaa_cs")
compile_shader("comp", "postfx/luminance.comp", "luminance_cs", binding_expr="vk.modules.luminance_cs")
compile_shader("comp", "vegetation_wind.comp", "vegetation_wind_cs", binding_expr="vk.modules.vegetation_wind_cs")
compile_shader("comp", "terrain/cbt_terrain.comp", "cbt_terrain_cs", binding_expr="vk.modules.cbt_terrain_cs")
compile_shader("comp", "forward_plus_tile_cull.comp", "forward_plus_tile_cull_cs", binding_expr="vk.modules.forward_plus_tile_cull_cs")
compile_shader("comp", "deferred_gbuffer_fill.comp", "deferred_gbuffer_fill_cs", binding_expr="vk.modules.deferred_gbuffer_fill_cs")
compile_shader("frag", "deferred_gbuffer_debug.frag", "deferred_gbuffer_debug_fs", binding_expr="vk.modules.deferred_gbuffer_debug_fs")
compile_shader("comp", "deferred_lighting.comp", "deferred_lighting_cs", binding_expr="vk.modules.deferred_lighting_cs")
compile_shader("comp", "ndgi/ndgi_decompress.comp", "ndgi_decompress_cs", binding_expr="vk.modules.ndgi_decompress_cs")
compile_shader("comp", "niv/niv_shade.comp", "niv_shade_cs", binding_expr="vk.modules.niv_shade_cs")
compile_shader("comp", "niv/niv_composite.comp", "niv_composite_cs", binding_expr="vk.modules.niv_composite_cs")
compile_shader("comp", "nslm/nslm_froxel.comp", "nslm_froxel_cs", binding_expr="vk.modules.nslm_froxel_cs")
compile_shader("comp", "nist/nist_refine.comp", "nist_refine_cs", binding_expr="vk.modules.nist_refine_cs")
compile_shader("comp", "nist/nist_composite.comp", "nist_composite_cs", binding_expr="vk.modules.nist_composite_cs")

compile_shader("comp", "nvc/nvc_cache.comp", "nvc_cache_cs", binding_expr="vk.modules.nvc_cache_cs")
compile_shader("comp", "nvc/nvc_restir.comp", "nvc_restir_cs", binding_expr="vk.modules.nvc_restir_cs")
compile_shader("comp", "nvc/nvc_composite.comp", "nvc_composite_cs", binding_expr="vk.modules.nvc_composite_cs")

compile_shader("comp", "fsa/fsa_importance.comp", "fsa_importance_cs", binding_expr="vk.modules.fsa_importance_cs")
compile_shader("comp", "fsa/fsa_denoise.comp", "fsa_denoise_cs", binding_expr="vk.modules.fsa_denoise_cs")
compile_shader("comp", "vfgi/vfgi_decode.comp", "vfgi_decode_cs", binding_expr="vk.modules.vfgi_decode_cs")
compile_shader("comp", "vfgi/vfgi_composite.comp", "vfgi_composite_cs", binding_expr="vk.modules.vfgi_composite_cs")
compile_shader("comp", "renderformer/rf_transport.comp", "rf_transport_cs", binding_expr="vk.modules.rf_transport_cs")
compile_shader("comp", "renderformer/rf_decode.comp", "rf_decode_cs", binding_expr="vk.modules.rf_decode_cs")
compile_shader("comp", "renderformer/rf_composite.comp", "rf_composite_cs", binding_expr="vk.modules.rf_composite_cs")
compile_shader("comp", "wpt/wpt_enqueue.comp", "wpt_enqueue_cs", binding_expr="vk.modules.wpt_enqueue_cs")
compile_shader("comp", "wpt/wpt_wave.comp", "wpt_wave_cs", binding_expr="vk.modules.wpt_wave_cs")
compile_shader("comp", "wpt/wpt_composite.comp", "wpt_composite_cs", binding_expr="vk.modules.wpt_composite_cs")
compile_shader("comp", "mgs/mgs_prepare.comp", "mgs_prepare_cs", binding_expr="vk.modules.mgs_prepare_cs")
compile_shader("comp", "mgs/mgs_splat.comp", "mgs_splat_cs", binding_expr="vk.modules.mgs_splat_cs")
compile_shader("comp", "mgs/mgs_composite.comp", "mgs_composite_cs", binding_expr="vk.modules.mgs_composite_cs")
compile_shader("comp", "vksplat/vksplat_project_fwd.comp", "vksplat_project_fwd_cs", binding_expr="vk.modules.vksplat_project_fwd_cs")
compile_shader("comp", "vksplat/vksplat_tile_cull.comp", "vksplat_tile_cull_cs", binding_expr="vk.modules.vksplat_tile_cull_cs")
compile_shader("comp", "vksplat/vksplat_raster_fwd.comp", "vksplat_raster_fwd_cs", binding_expr="vk.modules.vksplat_raster_fwd_cs")
compile_shader("comp", "vksplat/vksplat_adam.comp", "vksplat_adam_cs", binding_expr="vk.modules.vksplat_adam_cs")
compile_shader("comp", "curast/curast_clear.comp", "curast_clear_cs", binding_expr="vk.modules.curast_clear_cs")
compile_shader("comp", "curast/curast_stage1.comp", "curast_stage1_cs", binding_expr="vk.modules.curast_stage1_cs")
compile_shader("comp", "curast/curast_resolve.comp", "curast_resolve_cs", binding_expr="vk.modules.curast_resolve_cs")
compile_shader("comp", "graph/graph_bfs_expand.comp", "graph_bfs_expand_cs", binding_expr="vk.modules.graph_bfs_expand_cs")
compile_shader("comp", "arc_blanc/arc_blanc_htilde.comp", "arc_blanc_htilde_cs", binding_expr="vk.modules.arc_blanc_htilde_cs")
compile_shader("comp", "arc_blanc/arc_blanc_fft_1d.comp", "arc_blanc_fft_1d_cs", binding_expr="vk.modules.arc_blanc_fft_1d_cs")
compile_shader("comp", "arc_blanc/arc_blanc_extract.comp", "arc_blanc_extract_cs", binding_expr="vk.modules.arc_blanc_extract_cs")
compile_shader("comp", "arc_blanc/arc_blanc_combine.comp", "arc_blanc_combine_cs", binding_expr="vk.modules.arc_blanc_combine_cs")
compile_shader("comp", "arc_blanc/arc_blanc_velocity.comp", "arc_blanc_velocity_cs", binding_expr="vk.modules.arc_blanc_velocity_cs")
compile_shader("comp", "arc_blanc/arc_blanc_velocity_accum.comp", "arc_blanc_velocity_accum_cs", binding_expr="vk.modules.arc_blanc_velocity_accum_cs")
compile_shader("comp", "mimir/mimir_clear.comp", "mimir_clear_cs", binding_expr="vk.modules.mimir_clear_cs")
compile_shader("comp", "mimir/mimir_brownian.comp", "mimir_brownian_cs", binding_expr="vk.modules.mimir_brownian_cs")
compile_shader("comp", "mimir/mimir_splat.comp", "mimir_splat_cs", binding_expr="vk.modules.mimir_splat_cs")
compile_shader("comp", "iris/iris_clear.comp", "iris_clear_cs", binding_expr="vk.modules.iris_clear_cs")
compile_shader("comp", "iris/iris_spd.comp", "iris_spd_cs", binding_expr="vk.modules.iris_spd_cs")
compile_shader("comp", "iris/iris_compose.comp", "iris_compose_cs", binding_expr="vk.modules.iris_compose_cs")
compile_shader("comp", "iris/iris_overlay.comp", "iris_overlay_cs", binding_expr="vk.modules.iris_overlay_cs")
compile_shader("comp", "wsp/wsp_clear_tiles.comp", "wsp_clear_tiles_cs", binding_expr="vk.modules.wsp_clear_tiles_cs")
compile_shader("comp", "wsp/wsp_prepare.comp", "wsp_prepare_cs", binding_expr="vk.modules.wsp_prepare_cs")
compile_shader("comp", "wsp/wsp_tile_bin.comp", "wsp_tile_bin_cs", binding_expr="vk.modules.wsp_tile_bin_cs")
compile_shader("comp", "wsp/wsp_tile_draw.comp", "wsp_tile_draw_cs", binding_expr="vk.modules.wsp_tile_draw_cs")
compile_shader("comp", "wsp/wsp_composite.comp", "wsp_composite_cs", binding_expr="vk.modules.wsp_composite_cs")
compile_shader("frag", "deferred_lighting_composite.frag", "deferred_lighting_composite_fs", binding_expr="vk.modules.deferred_lighting_composite_fs")
compile_shader("vert", "terrain/terrain.vert", "terrain_vs", binding_expr="vk.modules.terrain_vs")
compile_shader("frag", "terrain/terrain.frag", "terrain_fs", binding_expr="vk.modules.terrain_fs")

# KHR ray tracing demo shaders (SPIR-V also embedded in vk_rtx_demo_spirv.inc; recompile to refresh blobs)
compile_shader("rgen", "rtx_demo.rgen", "rtx_demo_rgen_spv", rtx_collect=True)
compile_shader("rmiss", "rtx_demo.rmiss", "rtx_demo_rmiss_spv", rtx_collect=True)
compile_shader("rchit", "rtx_demo.rchit", "rtx_demo_rchit_spv", rtx_collect=True)

compile_shader("rgen", "grtx/grtx_trace.rgen", "grtx_trace_rgen_spv", grtx_collect=True)
compile_shader("rmiss", "grtx/grtx_miss.rmiss", "grtx_miss_rmiss_spv", grtx_collect=True)
compile_shader("rchit", "grtx/grtx_gaussian.rchit", "grtx_gaussian_rchit_spv", grtx_collect=True)

compile_shader("rgen", "pt_mega.rgen", "pt_mega_rgen_spv", pathtrace_collect=True)
compile_shader("rgen", "pt_wave.rgen", "pt_wave_rgen_spv", pathtrace_collect=True)
compile_shader("rmiss", "pt_miss.rmiss", "pt_miss_rmiss_spv", pathtrace_collect=True)
compile_shader("rchit", "pt_hit.rchit", "pt_hit_rchit_spv", pathtrace_collect=True)
compile_shader("comp", "pt_wave_compact.comp", "pt_wave_compact_cs", pathtrace_collect=True)
compile_shader("comp", "pt_denoise.comp", "pt_denoise_cs", pathtrace_collect=True)
compile_shader("comp", "pt_composite.comp", "pt_composite_cs", pathtrace_collect=True)

compile_shader("rgen", "hybrid1/hybrid1_shadow.rgen", "hybrid1_shadow_rgen_spv", hybrid1_collect=True)
compile_shader("rmiss", "hybrid1/hybrid1_shadow.rmiss", "hybrid1_shadow_rmiss_spv", hybrid1_collect=True)
compile_shader("rchit", "hybrid1/hybrid1_shadow.rchit", "hybrid1_shadow_rchit_spv", hybrid1_collect=True)
compile_shader("rgen", "hybrid1/hybrid1_spec.rgen", "hybrid1_spec_rgen_spv", hybrid1_collect=True)
compile_shader("rmiss", "hybrid1/hybrid1_spec.rmiss", "hybrid1_spec_rmiss_spv", hybrid1_collect=True)
compile_shader("rchit", "hybrid1/hybrid1_spec.rchit", "hybrid1_spec_rchit_spv", hybrid1_collect=True)
compile_shader("rgen", "hybrid1/hybrid1_diffuse.rgen", "hybrid1_diffuse_rgen_spv", hybrid1_collect=True)
compile_shader("rmiss", "hybrid1/hybrid1_diffuse.rmiss", "hybrid1_diffuse_rmiss_spv", hybrid1_collect=True)
compile_shader("rchit", "hybrid1/hybrid1_diffuse.rchit", "hybrid1_diffuse_rchit_spv", hybrid1_collect=True)
compile_shader("comp", "hybrid1/hybrid1_temporal.comp", "hybrid1_temporal_cs", hybrid1_collect=True)
compile_shader("comp", "hybrid1/hybrid1_atrous.comp", "hybrid1_atrous_cs", hybrid1_collect=True)
compile_shader("comp", "hybrid1/hybrid1_composite.comp", "hybrid1_composite_cs", hybrid1_collect=True)

compile_shader("rgen", "raygun/raygun.rgen", "raygun_rgen_spv", raygun_collect=True)
compile_shader("rmiss", "raygun/raygun.rmiss", "raygun_rmiss_spv", raygun_collect=True)
compile_shader("rchit", "raygun/raygun.rchit", "raygun_rchit_spv", raygun_collect=True)
compile_shader("rchit", "raygun/raygun_shadow.rchit", "raygun_shadow_rchit_spv", raygun_collect=True)
compile_shader("comp", "raygun/raygun_fxaa.comp", "raygun_fxaa_cs", raygun_collect=True)

compile_shader("vert", "dressi/dressi_soft.vert", "dressi_soft_vs", binding_expr="vk.modules.dressi_soft_vs")
compile_shader("frag", "dressi/dressi_soft.frag", "dressi_soft_fs", binding_expr="vk.modules.dressi_soft_fs")
compile_shader("comp", "dressi/dressi_blend.comp", "dressi_blend_cs", binding_expr="vk.modules.dressi_blend_cs")
compile_shader("comp", "dressi/dressi_composite.comp", "dressi_composite_cs", binding_expr="vk.modules.dressi_composite_cs")
compile_shader("comp", "dressi/dressi_inverse_uv.comp", "dressi_inverse_uv_cs", binding_expr="vk.modules.dressi_inverse_uv_cs")

def write_vk_rtx_demo_spirv_inc():
    """Emit src/renderers/vulkan/extensions/rtx/vk_rtx_demo_spirv.inc for USE_VULKAN_RTX embedded SPIR-V."""
    root = Path(os.environ.get("PROJECT_ROOT", "")).resolve()
    if not root or not root.is_dir():
        sys.exit("PROJECT_ROOT must be set for rtx demo SPIR-V embed")
    out_path = root / "src/renderers/vulkan/extensions/rtx/vk_rtx_demo_spirv.inc"
    mapping = [
        ("rtx_demo_rgen_spv", "vk_rtx_demo_rgen_spv", "VK_RTX_DEMO_RGEN_SPV_SIZE"),
        ("rtx_demo_rmiss_spv", "vk_rtx_demo_rmiss_spv", "VK_RTX_DEMO_RMISS_SPV_SIZE"),
        ("rtx_demo_rchit_spv", "vk_rtx_demo_rchit_spv", "VK_RTX_DEMO_RCHIT_SPV_SIZE"),
    ]
    lines = []
    lines.append("/* Auto-generated by scripts/compile_shaders.sh — do not edit by hand */")
    lines.append("")
    for src_name, c_array, size_macro in mapping:
        if src_name not in rtx_spv_bytes:
            sys.exit(f"Missing RTX SPIR-V blob for {src_name}")
        data = rtx_spv_bytes[src_name]
        lines.append(f"/* {src_name}.spv {len(data)} bytes */")
        lines.append(f"static const uint8_t {c_array}[] = {{")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            bytes_text = ", ".join(f"0x{b:02X}" for b in chunk)
            suffix = "," if offset + 16 < len(data) else ""
            lines.append(f"\t{bytes_text}{suffix}")
        lines.append("};")
        lines.append(f"#define {size_macro} ({len(data)}u)")
        lines.append("")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {out_path}")

write_vk_rtx_demo_spirv_inc()

def write_vk_grtx_spirv_inc():
    """Emit src/renderers/vulkan/extensions/rtx/vk_grtx_spirv.inc for USE_VULKAN_RTX GRTX embedded SPIR-V."""
    root = Path(os.environ.get("PROJECT_ROOT", "")).resolve()
    if not root or not root.is_dir():
        sys.exit("PROJECT_ROOT must be set for GRTX SPIR-V embed")
    out_path = root / "src/renderers/vulkan/extensions/rtx/vk_grtx_spirv.inc"
    mapping = [
        ("grtx_trace_rgen_spv", "vk_grtx_trace_rgen_spv", "VK_GRTX_TRACE_RGEN_SPV_SIZE"),
        ("grtx_miss_rmiss_spv", "vk_grtx_miss_rmiss_spv", "VK_GRTX_MISS_RMISS_SPV_SIZE"),
        ("grtx_gaussian_rchit_spv", "vk_grtx_gaussian_rchit_spv", "VK_GRTX_GAUSSIAN_RCHIT_SPV_SIZE"),
    ]
    lines = []
    lines.append("/* Auto-generated by scripts/compile_shaders.sh — do not edit by hand */")
    lines.append("")
    for src_name, c_array, size_macro in mapping:
        if src_name not in grtx_spv_bytes:
            sys.exit(f"Missing GRTX SPIR-V blob for {src_name}")
        data = grtx_spv_bytes[src_name]
        lines.append(f"/* {src_name}.spv {len(data)} bytes */")
        lines.append(f"static const uint8_t {c_array}[] = {{")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            bytes_text = ", ".join(f"0x{b:02X}" for b in chunk)
            suffix = "," if offset + 16 < len(data) else ""
            lines.append(f"\t{bytes_text}{suffix}")
        lines.append("};")
        lines.append(f"#define {size_macro} ({len(data)}u)")
        lines.append("")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {out_path}")

write_vk_grtx_spirv_inc()

def write_vk_pathtrace_spirv_inc():
    """Emit src/renderers/vulkan/vk_pathtrace_spirv.inc for USE_VULKAN_RTX path trace experiment."""
    root = Path(os.environ.get("PROJECT_ROOT", "")).resolve()
    if not root or not root.is_dir():
        sys.exit("PROJECT_ROOT must be set for pathtrace SPIR-V embed")
    out_path = root / "src/renderers/vulkan/vk_pathtrace_spirv.inc"
    mapping = [
        ("pt_mega_rgen_spv", "vk_pt_mega_rgen_spv", "VK_PT_MEGA_RGEN_SPV_SIZE"),
        ("pt_wave_rgen_spv", "vk_pt_wave_rgen_spv", "VK_PT_WAVE_RGEN_SPV_SIZE"),
        ("pt_miss_rmiss_spv", "vk_pt_miss_rmiss_spv", "VK_PT_MISS_RMISS_SPV_SIZE"),
        ("pt_hit_rchit_spv", "vk_pt_hit_rchit_spv", "VK_PT_HIT_RCHIT_SPV_SIZE"),
        ("pt_wave_compact_cs", "vk_pt_wave_compact_cs_spv", "VK_PT_WAVE_COMPACT_CS_SPV_SIZE"),
        ("pt_denoise_cs", "vk_pt_denoise_cs_spv", "VK_PT_DENOISE_CS_SPV_SIZE"),
        ("pt_composite_cs", "vk_pt_composite_cs_spv", "VK_PT_COMPOSITE_CS_SPV_SIZE"),
    ]
    lines = []
    lines.append("/* Auto-generated by scripts/compile_shaders.sh — do not edit by hand */")
    lines.append("")
    for src_name, c_array, size_macro in mapping:
        if src_name not in pathtrace_spv_bytes:
            sys.exit(f"Missing PathTrace SPIR-V blob for {src_name}")
        data = pathtrace_spv_bytes[src_name]
        lines.append(f"/* {src_name}.spv {len(data)} bytes */")
        lines.append(f"static const uint8_t {c_array}[] = {{")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            bytes_text = ", ".join(f"0x{b:02X}" for b in chunk)
            suffix = "," if offset + 16 < len(data) else ""
            lines.append(f"\t{bytes_text}{suffix}")
        lines.append("};")
        lines.append(f"#define {size_macro} ({len(data)}u)")
        lines.append("")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {out_path}")

write_vk_pathtrace_spirv_inc()

def write_vk_hybrid1_spirv_inc():
    """Emit src/renderers/vulkan/extensions/rtx/vk_hybrid1_spirv.inc for USE_VULKAN_RTX Hybrid Rendering 1."""
    root = Path(os.environ.get("PROJECT_ROOT", "")).resolve()
    if not root or not root.is_dir():
        sys.exit("PROJECT_ROOT must be set for hybrid1 SPIR-V embed")
    out_path = root / "src/renderers/vulkan/extensions/rtx/vk_hybrid1_spirv.inc"
    mapping = [
        ("hybrid1_shadow_rgen_spv", "vk_hybrid1_shadow_rgen_spv", "VK_HYBRID1_SHADOW_RGEN_SPV_SIZE"),
        ("hybrid1_shadow_rmiss_spv", "vk_hybrid1_shadow_rmiss_spv", "VK_HYBRID1_SHADOW_RMISS_SPV_SIZE"),
        ("hybrid1_shadow_rchit_spv", "vk_hybrid1_shadow_rchit_spv", "VK_HYBRID1_SHADOW_RCHIT_SPV_SIZE"),
        ("hybrid1_spec_rgen_spv", "vk_hybrid1_spec_rgen_spv", "VK_HYBRID1_SPEC_RGEN_SPV_SIZE"),
        ("hybrid1_spec_rmiss_spv", "vk_hybrid1_spec_rmiss_spv", "VK_HYBRID1_SPEC_RMISS_SPV_SIZE"),
        ("hybrid1_spec_rchit_spv", "vk_hybrid1_spec_rchit_spv", "VK_HYBRID1_SPEC_RCHIT_SPV_SIZE"),
        ("hybrid1_diffuse_rgen_spv", "vk_hybrid1_diffuse_rgen_spv", "VK_HYBRID1_DIFFUSE_RGEN_SPV_SIZE"),
        ("hybrid1_diffuse_rmiss_spv", "vk_hybrid1_diffuse_rmiss_spv", "VK_HYBRID1_DIFFUSE_RMISS_SPV_SIZE"),
        ("hybrid1_diffuse_rchit_spv", "vk_hybrid1_diffuse_rchit_spv", "VK_HYBRID1_DIFFUSE_RCHIT_SPV_SIZE"),
        ("hybrid1_temporal_cs", "vk_hybrid1_temporal_cs_spv", "VK_HYBRID1_TEMPORAL_CS_SPV_SIZE"),
        ("hybrid1_atrous_cs", "vk_hybrid1_atrous_cs_spv", "VK_HYBRID1_ATROUS_CS_SPV_SIZE"),
        ("hybrid1_composite_cs", "vk_hybrid1_composite_cs_spv", "VK_HYBRID1_COMPOSITE_CS_SPV_SIZE"),
    ]
    lines = []
    lines.append("/* Auto-generated by scripts/compile_shaders.sh — do not edit by hand */")
    lines.append("")
    for src_name, c_array, size_macro in mapping:
        if src_name not in hybrid1_spv_bytes:
            sys.exit(f"Missing Hybrid1 SPIR-V blob for {src_name}")
        data = hybrid1_spv_bytes[src_name]
        lines.append(f"/* {src_name}.spv {len(data)} bytes */")
        lines.append(f"static const uint8_t {c_array}[] = {{")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            bytes_text = ", ".join(f"0x{b:02X}" for b in chunk)
            suffix = "," if offset + 16 < len(data) else ""
            lines.append(f"\t{bytes_text}{suffix}")
        lines.append("};")
        lines.append(f"#define {size_macro} ({len(data)}u)")
        lines.append("")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {out_path}")

write_vk_hybrid1_spirv_inc()

def write_vk_raygun_spirv_inc():
    """Emit src/renderers/vulkan/vk_raygun_spirv.inc for Raygun RT demo (arXiv:2001.09792)."""
    root = Path(os.environ.get("PROJECT_ROOT", "")).resolve()
    if not root or not root.is_dir():
        sys.exit("PROJECT_ROOT must be set for raygun SPIR-V embed")
    out_path = root / "src/renderers/vulkan/vk_raygun_spirv.inc"
    mapping = [
        ("raygun_rgen_spv", "vk_raygun_rgen_spv", "VK_RAYGUN_RGEN_SPV_SIZE"),
        ("raygun_rmiss_spv", "vk_raygun_rmiss_spv", "VK_RAYGUN_RMISS_SPV_SIZE"),
        ("raygun_rchit_spv", "vk_raygun_rchit_spv", "VK_RAYGUN_RCHIT_SPV_SIZE"),
        ("raygun_shadow_rchit_spv", "vk_raygun_shadow_rchit_spv", "VK_RAYGUN_SHADOW_RCHIT_SPV_SIZE"),
        ("raygun_fxaa_cs", "vk_raygun_fxaa_cs_spv", "VK_RAYGUN_FXAA_CS_SPV_SIZE"),
    ]
    lines = []
    lines.append("/* Auto-generated by scripts/compile_shaders.sh — do not edit by hand */")
    lines.append("")
    for src_name, c_array, size_macro in mapping:
        if src_name not in raygun_spv_bytes:
            sys.exit(f"Missing Raygun SPIR-V blob for {src_name}")
        data = raygun_spv_bytes[src_name]
        lines.append(f"/* {src_name}.spv {len(data)} bytes */")
        lines.append(f"static const uint8_t {c_array}[] = {{")
        for offset in range(0, len(data), 16):
            chunk = data[offset:offset + 16]
            bytes_text = ", ".join(f"0x{b:02X}" for b in chunk)
            suffix = "," if offset + 16 < len(data) else ""
            lines.append(f"\t{bytes_text}{suffix}")
        lines.append("};")
        lines.append(f"#define {size_macro} ({len(data)}u)")
        lines.append("")
    out_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Wrote {out_path}")

write_vk_raygun_spirv_inc()

with binding_file.open("w") as f:
    f.write("// this file is autogenerated during shader compilation\n")
    f.write("static void vk_set_shader_name( VkShaderModule shader, const char *name ) {\n")
    f.write("    SET_OBJECT_NAME( shader, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );\n")
    f.write("}\n")
    f.write("void vk_bind_generated_shaders( void ){\n")
    for binding, var in bindings:
        f.write(f"    {binding} = SHADER_MODULE( {var} );\n")
        f.write(f"    vk_set_shader_name( {binding}, \"{var}\" );\n")
    f.write("}\n")

print(f"Wrote {task_counter} shader binaries and {len(bindings)} bindings.")
PY
)

METADATA_FILE="$GENERATED_DIR/shader_build_meta.txt"
VALIDATOR_VERSION="$("$GLSLANG_VALIDATOR" --version 2>&1 | head -n1)"
TIMESTAMP="$(date -u +%FT%TZ)"
shader_data_generated="$GENERATED_DIR/shader_data.c"
shader_binding_generated="$GENERATED_DIR/shader_binding.c"

compute_sha256() {
  local file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
  else
    echo "sha256-unavailable"
  fi
}

sha256_data="missing"
sha256_binding="missing"
[[ -f "$shader_data_generated" ]] && sha256_data="$(compute_sha256 "$shader_data_generated")"
[[ -f "$shader_binding_generated" ]] && sha256_binding="$(compute_sha256 "$shader_binding_generated")"

cat <<EOF > "$METADATA_FILE"
glslang_validator_path=${GLSLANG_VALIDATOR}
glslang_validator_version=${VALIDATOR_VERSION}
generated_at=${TIMESTAMP}
shader_data_sha256=${sha256_data}
shader_binding_sha256=${sha256_binding}
EOF

apply_shaders() {
  local src_dir="$GENERATED_DIR"
  local dst_dir="$SPIRV_DIR"
  local backup_base="$GENERATED_DIR/backups/$(date -u +%Y%m%dT%H%M%SZ)"
  local created_backup=0
  local updated=0

  for file in shader_data.c shader_binding.c; do
    local src="$src_dir/$file"
    local dst="$dst_dir/$file"
    if [[ ! -f "$src" ]]; then
      echo "Generated shader file missing: $src" >&2
      return 1
    fi

    if [[ -f "$dst" ]] && cmp -s "$src" "$dst"; then
      echo "Skipped $file (no changes)"
      continue
    fi

    if [[ -f "$dst" ]]; then
      mkdir -p "$backup_base"
      cp "$dst" "$backup_base/$file.bak"
      created_backup=1
    fi

    cp "$src" "$dst"
    echo "Updated $dst"
    updated=1
  done

  if [[ "$created_backup" -eq 1 ]]; then
    echo "Backed up previous shader blobs to $backup_base"
  fi

  if [[ "$updated" -eq 0 ]]; then
    echo "Shader blobs already matched tracked versions."
  fi
}

if [[ "$APPLY" -eq 1 ]]; then
  apply_shaders
else
  echo "Run $0 --apply to install the generated shaders into ${SPIRV_DIR}"
fi

echo "Shaders compiled into $GENERATED_DIR"
