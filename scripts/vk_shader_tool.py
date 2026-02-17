#!/usr/bin/env python3
"""Deterministic Vulkan shader codegen/check tool.

Usage:
  scripts/vk_shader_tool.py regen
  scripts/vk_shader_tool.py check
"""

from __future__ import annotations

import argparse
import dataclasses
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADER_ROOT = ROOT / "src/renderers/vulkanrenderer/shaders"
GLSL_ROOT = SHADER_ROOT / "glsl"
SPIRV_ROOT = SHADER_ROOT / "spirv"
OUT_DATA = SPIRV_ROOT / "shader_data.c"
OUT_BIND = SPIRV_ROOT / "shader_binding.c"

DEFAULT_GLSLANG = Path("/usr/bin/glslangValidator")


@dataclasses.dataclass(frozen=True)
class Task:
    input_file: str
    stage: str
    output_var: str
    binding_name: str | None
    defines: tuple[str, ...]


@dataclasses.dataclass
class Compiled:
    var_name: str
    binding_name: str | None
    data: bytes


def _join_indexes(base: str, indexes: tuple[int, ...]) -> str:
    return base + "".join(f"[{idx}]" for idx in indexes)


def _compile_task(compiler: Path, task: Task, out_spv: Path) -> bytes:
    cmd = [
        str(compiler),
        "-s",  # silent unless compile fails
        "-S",
        task.stage,
        "-V",
        "-o",
        str(out_spv),
        str(GLSL_ROOT / task.input_file),
        *task.defines,
    ]

    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        verbose_cmd = cmd.copy()
        if "-s" in verbose_cmd:
            verbose_cmd.remove("-s")
        verbose = subprocess.run(verbose_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        sys.stderr.write(f"Shader compile failed for {task.output_var}\n")
        sys.stderr.write("Command: " + " ".join(verbose_cmd) + "\n")
        if verbose.stdout:
            sys.stderr.write(verbose.stdout)
        if verbose.stderr:
            sys.stderr.write(verbose.stderr)
        raise RuntimeError("shader compile failed")

    data = out_spv.read_bytes()
    out_spv.unlink(missing_ok=True)
    return data


def build_tasks() -> list[Task]:
    tasks: list[Task] = []

    def add(input_file: str, stage: str, var: str, binding: str | None, defines: tuple[str, ...] = ()) -> None:
        tasks.append(Task(input_file=input_file, stage=stage, output_var=var, binding_name=binding, defines=defines))

    # Individual shaders (.vert/.frag/.geom), deterministic order.
    for stage, ext in (("vert", ".vert"), ("frag", ".frag"), ("geom", ".geom")):
        for f in sorted(GLSL_ROOT.glob(f"*{ext}")):
            stem = f.name[: -len(ext)]
            add(f.name, stage, f"{stem}_{stage}_spv", None)

    mode_flags = ("-DUSE_CLX_IDENT", "-DUSE_FIXED_COLOR")
    mode_ids = ("ident1", "fixed")

    pbr_flags = ("", "-DUSE_VK_PBR")
    pbr_ids = ("", "pbr_")

    tx_flags = ("", "-DUSE_TX1", "-DUSE_TX2")
    tx_ids = ("tx0", "tx1", "tx2")

    cl_flags = ("", "-DUSE_CL1", "-DUSE_CL2")
    cl_ids = ("", "cl", "cl")

    env_flags = ("", "-DUSE_ENV")
    env_ids = ("", "_env")

    fog_flags = ("", "-DUSE_FOG")
    fog_ids = ("", "_fog")

    # Standalone template variants.
    add("gen_frag.tmpl", "frag", "frag_tx0_df", None, ("-DUSE_CLX_IDENT", "-DUSE_ATEST", "-DUSE_DF"))

    add("light_vert.tmpl", "vert", "vert_light", None)
    add("light_vert.tmpl", "vert", "vert_light_fog", None, ("-DUSE_FOG",))

    add("light_frag.tmpl", "frag", "frag_light", None)
    add("light_frag.tmpl", "frag", "frag_light_fog", None, ("-DUSE_FOG",))
    add("light_frag.tmpl", "frag", "frag_light_line", None, ("-DUSE_LINE",))
    add("light_frag.tmpl", "frag", "frag_light_line_fog", None, ("-DUSE_LINE", "-DUSE_FOG"))

    add("gen_frag.tmpl", "frag", "frag_tx0_ent", None, ("-DUSE_ENT_COLOR", "-DUSE_ATEST"))
    add("gen_frag.tmpl", "frag", "frag_tx0_ent_fog", None, ("-DUSE_ENT_COLOR", "-DUSE_ATEST", "-DUSE_FOG"))
    add("gen_frag.tmpl", "frag", "frag_pbr_tx0_ent", None, ("-DUSE_ENT_COLOR", "-DUSE_ATEST", "-DUSE_VK_PBR"))
    add("gen_frag.tmpl", "frag", "frag_pbr_tx0_ent_fog", None, ("-DUSE_ENT_COLOR", "-DUSE_ATEST", "-DUSE_FOG", "-DUSE_VK_PBR"))

    # ident/fixed vertex shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags) - 1):
            for m in range(len(mode_flags)):
                for k in range(len(env_flags)):
                    for l in range(len(fog_flags)):
                        defines = tuple(x for x in (pbr_flags[i], tx_flags[j], mode_flags[m], env_flags[k], fog_flags[l]) if x)
                        name = f"vert_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{env_ids[k]}{fog_ids[l]}"
                        bind = _join_indexes(f"vk.modules.vert.{mode_ids[m]}", (i, j, k, l))
                        add("gen_vert.tmpl", "vert", name, bind, defines)

    # ident/fixed fragment shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags) - 1):
            for m in range(len(mode_flags)):
                for k in range(len(fog_flags)):
                    defs = [x for x in (pbr_flags[i], tx_flags[j], mode_flags[m], fog_flags[k]) if x]
                    if j == 0:
                        defs.append("-DUSE_ATEST")
                    name = f"frag_{pbr_ids[i]}{tx_ids[j]}_{mode_ids[m]}{fog_ids[k]}"
                    bind = _join_indexes(f"vk.modules.frag.{mode_ids[m]}", (i, j, k))
                    add("gen_frag.tmpl", "frag", name, bind, tuple(defs))

    # generic vertex shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(env_flags)):
                for l in range(len(fog_flags)):
                    defines = tuple(x for x in (pbr_flags[i], tx_flags[j], env_flags[k], fog_flags[l]) if x)
                    name = f"vert_{pbr_ids[i]}{tx_ids[j]}{env_ids[k]}{fog_ids[l]}"
                    bind = _join_indexes("vk.modules.vert.gen", (i, j, 0, k, l))
                    add("gen_vert.tmpl", "vert", name, bind, defines)

                    if j != 0:
                        defines_cl = tuple(x for x in (pbr_flags[i], tx_flags[j], cl_flags[j], env_flags[k], fog_flags[l]) if x)
                        name_cl = f"vert_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{env_ids[k]}{fog_ids[l]}"
                        bind_cl = _join_indexes("vk.modules.vert.gen", (i, j, 1, k, l))
                        add("gen_vert.tmpl", "vert", name_cl, bind_cl, defines_cl)

    # generic fragment shaders
    for i in range(len(pbr_flags)):
        for j in range(len(tx_flags)):
            for k in range(len(fog_flags)):
                defs = [x for x in (pbr_flags[i], tx_flags[j], fog_flags[k]) if x]
                if j == 0:
                    defs.append("-DUSE_ATEST")
                name = f"frag_{pbr_ids[i]}{tx_ids[j]}{fog_ids[k]}"
                bind = _join_indexes("vk.modules.frag.gen", (i, j, 0, k))
                add("gen_frag.tmpl", "frag", name, bind, tuple(defs))

                if j != 0:
                    defs_cl = tuple(x for x in (pbr_flags[i], tx_flags[j], cl_flags[j], fog_flags[k]) if x)
                    name_cl = f"frag_{pbr_ids[i]}{tx_ids[j]}_{cl_ids[j]}{fog_ids[k]}"
                    bind_cl = _join_indexes("vk.modules.frag.gen", (i, j, 1, k))
                    add("gen_frag.tmpl", "frag", name_cl, bind_cl, defs_cl)

    return tasks


def write_shader_data(compiled: list[Compiled], out_file: Path) -> None:
    line_len = 16
    with out_file.open("w", newline="\n") as f:
        for item in compiled:
            size = len(item.data)
            f.write(f"const unsigned char {item.var_name}[{size}] = {{\n\t")
            for i, b in enumerate(item.data):
                f.write(f"0x{b:02X}")
                if i + 1 < size:
                    if (i + 1) % line_len:
                        f.write(", ")
                    else:
                        f.write(",\n\t")
            f.write("\n};\n")


def write_shader_bindings(compiled: list[Compiled], out_file: Path) -> None:
    with out_file.open("w", newline="\n") as f:
        f.write("// this file is autogenerated during shader compilation\n")
        f.write("static void vk_set_shader_name( VkShaderModule shader, const char *name ) {\n")
        f.write("    SET_OBJECT_NAME( shader, name, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT );\n")
        f.write("}\n")
        f.write("void vk_bind_generated_shaders( void ){\n")

        for item in compiled:
            if not item.binding_name:
                continue
            f.write(f"    {item.binding_name} = SHADER_MODULE( {item.var_name} );\n")
            f.write(f"    vk_set_shader_name( {item.binding_name}, \"{item.var_name}\" );\n")

        f.write("}\n")


def regen(compiler: Path) -> int:
    if not compiler.exists():
        print(f"error: glslangValidator not found at {compiler}", file=sys.stderr)
        return 2

    tasks = build_tasks()
    if not tasks:
        print("error: no shader tasks generated", file=sys.stderr)
        return 2

    SPIRV_ROOT.mkdir(parents=True, exist_ok=True)

    compiled: list[Compiled] = []
    with tempfile.TemporaryDirectory(prefix="vk_spv_codegen_") as td:
        tempdir = Path(td)
        for idx, task in enumerate(tasks, start=1):
            tmp_spv = tempdir / f"tmp_{idx}.spv"
            data = _compile_task(compiler, task, tmp_spv)
            compiled.append(Compiled(var_name=task.output_var, binding_name=task.binding_name, data=data))

    write_shader_bindings(compiled, OUT_BIND)
    write_shader_data(compiled, OUT_DATA)

    bound = sum(1 for c in compiled if c.binding_name)
    print(f"Wrote {len(compiled)} shaders -> {OUT_DATA}")
    print(f"Wrote {bound} bindings -> {OUT_BIND}")
    return 0


def _git_changed(ref: str) -> set[str]:
    cmd = ["git", "diff", "--name-only", ref, "--"]
    res = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        raise RuntimeError(res.stderr.strip() or "git diff failed")
    return {line.strip() for line in res.stdout.splitlines() if line.strip()}


def check(ref: str) -> int:
    changed = _git_changed(ref)

    shader_prefix = "src/renderers/vulkanrenderer/shaders/glsl/"
    shader_changed = any(path.startswith(shader_prefix) for path in changed)

    generated_files = {
        "src/renderers/vulkanrenderer/shaders/spirv/shader_data.c",
        "src/renderers/vulkanrenderer/shaders/spirv/shader_binding.c",
    }
    changed_generated = generated_files.intersection(changed)

    if shader_changed and changed_generated != generated_files:
        missing = sorted(generated_files - changed_generated)
        print("error: GLSL changed but generated SPIR-V C files are not fully updated.", file=sys.stderr)
        for m in missing:
            print(f"  missing: {m}", file=sys.stderr)
        print("run: scripts/vk_shader_tool.py regen", file=sys.stderr)
        return 1

    print("Shader generation check passed.")
    return 0


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Vulkan shader tool")
    sub = p.add_subparsers(dest="cmd", required=True)

    regen_p = sub.add_parser("regen", help="Regenerate shader_data.c and shader_binding.c")
    regen_p.add_argument(
        "--compiler",
        type=Path,
        default=DEFAULT_GLSLANG,
        help=f"Path to glslangValidator (default: {DEFAULT_GLSLANG})",
    )

    check_p = sub.add_parser("check", help="Fail if GLSL changed without regenerated C blobs")
    check_p.add_argument(
        "--ref",
        default="HEAD",
        help="Git ref to diff against (default: HEAD)",
    )

    return p.parse_args()


def main() -> int:
    args = parse_args()

    if args.cmd == "regen":
        return regen(args.compiler)

    if args.cmd == "check":
        return check(args.ref)

    return 2


if __name__ == "__main__":
    raise SystemExit(main())
