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
bin2hex = Path(os.environ["BIN2HEX"])

bindings = []
task_counter = 0

def compile_shader(stage, source, array_name, binding_expr=None, defines=""):
    global task_counter
    input_path = glsl_dir / source
    if not input_path.is_file():
        sys.exit(f"Shader source missing: {input_path}")
    tmp_spv = generated_dir / f"tmp_{task_counter:04d}.spv"
    defines_list = defines.split() if defines else []
    cmd = [str(glslang), "-S", stage, "-V", "-o", str(tmp_spv), str(input_path)] + defines_list
    print(f"  compiling {array_name}")
    subprocess.run(cmd, check=True)
    subprocess.run([str(bin2hex), str(tmp_spv), f"+{data_file}", array_name], check=True)
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

def compile_individual_shaders():
    print("Compiling standalone GLSL stages...")
    for stage, ext in (("vert", ".vert"), ("frag", ".frag"), ("geom", ".geom")):
        for path in sorted(glsl_dir.glob(f"*{ext}")):
            base = path.name[: -len(ext)]
            array_name = f"{base}_{stage}_spv"
            compile_shader(stage, path.name, array_name)

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
    compile_shader("frag", "gen_frag.tmpl", "frag_pbr_tx0_ent", defines="-DUSE_ENT_COLOR -DUSE_ATEST -DUSE_VK_PBR")
    compile_shader("frag", "gen_frag.tmpl", "frag_pbr_tx0_ent_fog", defines="-DUSE_ENT_COLOR -DUSE_ATEST -DUSE_FOG -DUSE_VK_PBR")

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
                        defines = join_flags(pbr_flags[i], tx_flags[j], mode_flags[m], env_flags[k], fog_flags[l])
                        name = f"vert_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{env_ids[k]}{fog_ids[l]}"
                        binding = join_indexes(f"vk.modules.vert.{mode_ids[m]}", [i, j, k, l])
                        compile_shader("vert", "gen_vert.tmpl", name, binding_expr=binding, defines=defines)

    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags) - 1):
            for m in range(len(mode_flags)):
                for k in range(len(fog_flags)):
                    extra = "-DUSE_ATEST" if j == 0 else ""
                    defines = join_flags(pbr_flags[i], tx_flags[j], mode_flags[m], fog_flags[k], extra)
                    name = f"frag_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{fog_ids[k]}"
                    binding = join_indexes(f"vk.modules.frag.{mode_ids[m]}", [i, j, k])
                    compile_shader("frag", "gen_frag.tmpl", name, binding_expr=binding, defines=defines)

    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(env_flags)):
                for l in range(len(fog_flags)):
                    defines = join_flags(pbr_flags[i], tx_flags[j], env_flags[k], fog_flags[l])
                    name = f"vert_{pbr_ids[i]}{tx_ids[j]}{env_ids[k]}{fog_ids[l]}"
                    binding = join_indexes("vk.modules.vert.gen", [i, j, 0, k, l])
                    compile_shader("vert", "gen_vert.tmpl", name, binding_expr=binding, defines=defines)
                    if j != 0:
                        defines_cl = join_flags(pbr_flags[i], tx_flags[j], cl_flags[j], env_flags[k], fog_flags[l])
                        name_cl = f"vert_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{env_ids[k]}{fog_ids[l]}"
                        binding_cl = join_indexes("vk.modules.vert.gen", [i, j, 1, k, l])
                        compile_shader("vert", "gen_vert.tmpl", name_cl, binding_expr=binding_cl, defines=defines_cl)

    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(fog_flags)):
                extra = "-DUSE_ATEST" if j == 0 else ""
                defines = join_flags(pbr_flags[i], tx_flags[j], fog_flags[k], extra)
                name = f"frag_{pbr_ids[i]}{tx_ids[j]}{fog_ids[k]}"
                binding = join_indexes("vk.modules.frag.gen", [i, j, 0, k])
                compile_shader("frag", "gen_frag.tmpl", name, binding_expr=binding, defines=defines)
                if j != 0:
                    defines_cl = join_flags(pbr_flags[i], tx_flags[j], cl_flags[j], fog_flags[k])
                    name_cl = f"frag_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{fog_ids[k]}"
                    binding_cl = join_indexes("vk.modules.frag.gen", [i, j, 1, k])
                    compile_shader("frag", "gen_frag.tmpl", name_cl, binding_expr=binding_cl, defines=defines_cl)

compile_individual_shaders()
compile_template_shaders()
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
compile_shader("vert", "terrain/terrain.vert", "terrain_vs", binding_expr="vk.modules.terrain_vs")
compile_shader("frag", "terrain/terrain.frag", "terrain_fs", binding_expr="vk.modules.terrain_fs")

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
VALIDATOR_VERSION="$("$GLSLANG_VALIDATOR" --version 2>&1 | head -n1 | tr '\n' ' ')"
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

    if [[ -f "$dst" ]]; then
      mkdir -p "$backup_base"
      cp "$dst" "$backup_base/$file.bak"
      created_backup=1
    fi

    if [[ -f "$dst" ]] && cmp -s "$src" "$dst"; then
      echo "Skipped $file (no changes)"
      continue
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
