#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import subprocess
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GLSL_ROOT = ROOT / "src" / "renderers" / "vulkanrenderer" / "shaders" / "glsl"
SPIRV_DIR = ROOT / "src" / "renderers" / "vulkanrenderer" / "shaders" / "spirv"
OUT_DATA = SPIRV_DIR / "shader_data.c"
OUT_BINDINGS = SPIRV_DIR / "shader_binding.c"
METADATA_DEFAULT = SPIRV_DIR / "shader_permutations.json"

GLSLANG = os.environ.get("GLSLANG_VALIDATOR", "glslangValidator")


class Task:
    def __init__(self, input_file, stage, out_var, out_binding, defines):
        self.input_file = input_file
        self.stage = stage
        self.out_var = out_var
        self.out_binding = out_binding
        self.defines = defines
        self.spirv_out = None


def join_flags(flags):
    return " ".join([f for f in flags if f])


def join_indexes(obj, indexes):
    result = obj
    for idx in indexes:
        result += f"[{idx}]"
    return result


def create_task(tasks, f_name, stage, out_var, out_binding, defines):
    tasks.append(Task(f_name, stage, out_var, out_binding, defines))


def collect_tasks():
    tasks = []

    mode_flags = ["-DUSE_CLX_IDENT", "-DUSE_FIXED_COLOR"]
    mode_ids = ["ident1", "fixed"]

    pbr_flags = ["", "-DUSE_VK_PBR", "-DUSE_VK_PBR -DUSE_DESCRIPTOR_INDEXING"]
    pbr_ids = ["", "pbr_", "pbrdi_"]

    tx_flags = ["", "-DUSE_TX1", "-DUSE_TX2"]
    tx_ids = ["tx0", "tx1", "tx2"]

    cl_flags = ["", "-DUSE_CL1", "-DUSE_CL2"]
    cl_ids = ["", "cl", "cl"]

    env_flags = ["", "-DUSE_ENV"]
    env_ids = ["", "_env"]

    fog_flags = ["", "-DUSE_FOG"]
    fog_ids = ["", "_fog"]

    # single-texture fragment, depth-fragment
    create_task(tasks, "gen_frag.tmpl", "frag", "frag_tx0_df", None, " -DUSE_CLX_IDENT -DUSE_ATEST -DUSE_DF")

    # lighting shader variations
    create_task(tasks, "light_vert.tmpl", "vert", "vert_light", None, "")
    create_task(tasks, "light_vert.tmpl", "vert", "vert_light_fog", None, "-DUSE_FOG")
    create_task(tasks, "light_frag.tmpl", "frag", "frag_light", None, "")
    create_task(tasks, "light_frag.tmpl", "frag", "frag_light_fog", None, "-DUSE_FOG")
    create_task(tasks, "light_frag.tmpl", "frag", "frag_light_line", None, "-DUSE_LINE")
    create_task(tasks, "light_frag.tmpl", "frag", "frag_light_line_fog", None, "-DUSE_LINE -DUSE_FOG")

    # entity fragment shaders
    create_task(tasks, "gen_frag.tmpl", "frag", "frag_tx0_ent", None, "-DUSE_ENT_COLOR -DUSE_ATEST")
    create_task(tasks, "gen_frag.tmpl", "frag", "frag_tx0_ent_fog", None, "-DUSE_ENT_COLOR -DUSE_ATEST  -DUSE_FOG")
    create_task(tasks, "gen_frag.tmpl", "frag", "frag_pbr_tx0_ent", None, "-DUSE_ENT_COLOR -DUSE_ATEST  -DUSE_VK_PBR")
    create_task(tasks, "gen_frag.tmpl", "frag", "frag_pbr_tx0_ent_fog", None, "-DUSE_ENT_COLOR -DUSE_ATEST  -DUSE_FOG  -DUSE_VK_PBR")
    create_task(tasks, "gen_frag.tmpl", "frag", "frag_pbrdi_tx0_ent", None, "-DUSE_ENT_COLOR -DUSE_ATEST  -DUSE_VK_PBR -DUSE_DESCRIPTOR_INDEXING")
    create_task(tasks, "gen_frag.tmpl", "frag", "frag_pbrdi_tx0_ent_fog", None, "-DUSE_ENT_COLOR -DUSE_ATEST  -DUSE_FOG  -DUSE_VK_PBR -DUSE_DESCRIPTOR_INDEXING")

    # ident / fixed vertex shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags) - 1):  # tx [0,1 only]
            for m in range(len(mode_flags)):
                for k in range(len(env_flags)):
                    for l in range(len(fog_flags)):
                        defines = join_flags([pbr_flags[i], tx_flags[j], mode_flags[m], env_flags[k], fog_flags[l]])
                        name = f"vert_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{env_ids[k]}{fog_ids[l]}"
                        ids = join_indexes(f"vk.modules.vert.{mode_ids[m]}", [i, j, k, l])
                        create_task(tasks, "gen_vert.tmpl", "vert", name, ids, defines)

    # ident / fixed fragment shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags) - 1):
            for m in range(len(mode_flags)):
                for k in range(len(fog_flags)):
                    defines = join_flags([pbr_flags[i], tx_flags[j], mode_flags[m], fog_flags[k]])
                    if j == 0:
                        defines = f"{defines} -DUSE_ATEST"
                    name = f"frag_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{fog_ids[k]}"
                    ids = join_indexes(f"vk.modules.frag.{mode_ids[m]}", [i, j, k])
                    create_task(tasks, "gen_frag.tmpl", "frag", name, ids, defines)

    # generic vertex shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(env_flags)):
                for l in range(len(fog_flags)):
                    defines = join_flags([pbr_flags[i], tx_flags[j], env_flags[k], fog_flags[l]])
                    name = f"vert_{pbr_ids[i]}{tx_ids[j]}{env_ids[k]}{fog_ids[l]}"
                    ids = join_indexes("vk.modules.vert.gen", [i, j, 0, k, l])
                    create_task(tasks, "gen_vert.tmpl", "vert", name, ids, defines)
                    if j != 0:
                        defines_cl = join_flags([pbr_flags[i], tx_flags[j], cl_flags[j], env_flags[k], fog_flags[l]])
                        name_cl = f"vert_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{env_ids[k]}{fog_ids[l]}"
                        ids_cl = join_indexes("vk.modules.vert.gen", [i, j, 1, k, l])
                        create_task(tasks, "gen_vert.tmpl", "vert", name_cl, ids_cl, defines_cl)

    # generic fragment shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(fog_flags)):
                defines = join_flags([pbr_flags[i], tx_flags[j], fog_flags[k]])
                if j == 0:
                    defines = f"{defines} -DUSE_ATEST"
                name = f"frag_{pbr_ids[i]}{tx_ids[j]}{fog_ids[k]}"
                ids = join_indexes("vk.modules.frag.gen", [i, j, 0, k])
                create_task(tasks, "gen_frag.tmpl", "frag", name, ids, defines)
                if j != 0:
                    defines_cl = join_flags([pbr_flags[i], tx_flags[j], cl_flags[j], fog_flags[k]])
                    name_cl = f"frag_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{fog_ids[k]}"
                    ids_cl = join_indexes("vk.modules.frag.gen", [i, j, 1, k])
                    create_task(tasks, "gen_frag.tmpl", "frag", name_cl, ids_cl, defines_cl)

    # individual shaders (.vert/.frag/.geom)
    for entry in sorted(GLSL_ROOT.iterdir()):
        if entry.suffix not in {".vert", ".frag", ".geom"}:
            continue
        stage = entry.suffix[1:]
        base_name = entry.stem
        out_var = f"{base_name}_{stage}_spv"
        create_task(tasks, entry.name, stage, out_var, None, "")

    return tasks


def compile_task(task, index):
    SPIRV_DIR.mkdir(parents=True, exist_ok=True)
    task.spirv_out = SPIRV_DIR / f"tmp_{index + 1}.spv"
    input_path = GLSL_ROOT / task.input_file
    cmd = [
        GLSLANG,
        "-S",
        task.stage,
        "-V",
        "-o",
        str(task.spirv_out),
        str(input_path),
    ]
    if task.defines.strip():
        cmd.extend(task.defines.strip().split())

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"Failed to compile {task.input_file} ({task.out_var}):\n{result.stderr}")

    data = task.spirv_out.read_bytes()
    task.spirv_out.unlink(missing_ok=True)
    return data


def write_shader_data(tasks, shader_data):
    with OUT_DATA.open("w", encoding="utf-8") as f:
        for task, data in zip(tasks, shader_data):
            if not data:
                continue
            f.write(f"const unsigned char {task.out_var}[{len(data)}] = {{\n\t")
            for i, byte in enumerate(data):
                f.write(f"0x{byte:02X}")
                if i + 1 < len(data):
                    if (i + 1) % 16:
                        f.write(", ")
                    else:
                        f.write(",\n\t")
            f.write("\n};\n")


def write_shader_bindings(tasks, shader_data):
    with OUT_BINDINGS.open("w", encoding="utf-8") as f:
        f.write("// this file is autogenerated during shader compilation\n")
        f.write("static void vk_set_shader_name( VkShaderModule shader, const char *name ) {\n")
        f.write("    SET_OBJECT_NAME( shader, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );\n")
        f.write("}\n")
        f.write("void vk_bind_generated_shaders( void ){\n")
        for task, data in zip(tasks, shader_data):
            if not data:
                continue
            if not task.out_binding:
                continue
            f.write(f"    {task.out_binding} = SHADER_MODULE( {task.out_var} );\n")
            f.write(f"    vk_set_shader_name( {task.out_binding}, \"{task.out_var}\" );\n")
        f.write("}\n")


def get_git_sha():
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=True,
            cwd=ROOT,
            text=True,
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return "unknown"


def build_metadata(tasks, shader_data):
    global_hasher = hashlib.sha256()
    entries = []
    for task, data in zip(tasks, shader_data):
        if not data:
            continue
        hasher = hashlib.sha256()
        hasher.update(data)
        task_hash = hasher.hexdigest()
        global_hasher.update(data)
        entries.append(
            {
                "name": task.out_var,
                "stage": task.stage,
                "hash": task_hash,
                "defines": task.defines.strip() if task.defines else "",
            }
        )
    return {
        "git_sha": get_git_sha(),
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "shader_hash": global_hasher.hexdigest(),
        "task_count": len(entries),
        "tasks": entries,
    }


def write_metadata(metadata, out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(metadata, indent=2))


def parse_args():
    parser = argparse.ArgumentParser(description="Compile Vulkan GLSL shaders to SPIR-V")
    parser.add_argument(
        "--metadata-out",
        type=Path,
        default=METADATA_DEFAULT,
        help="Write shader permutation metadata to this JSON file",
    )
    parser.add_argument(
        "--metadata-only",
        action="store_true",
        help="Only collect metadata without generating C sources",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    tasks = collect_tasks()
    shader_data = [compile_task(task, idx) for idx, task in enumerate(tasks)]
    metadata = build_metadata(tasks, shader_data)
    if not args.metadata_only:
        write_shader_data(tasks, shader_data)
        write_shader_bindings(tasks, shader_data)
    write_metadata(metadata, args.metadata_out)
    print(
        f"Generated {OUT_DATA} and {OUT_BINDINGS} with {len(tasks)} shaders, metadata at {args.metadata_out}"
    )


if __name__ == "__main__":
    main()
