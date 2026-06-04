#!/usr/bin/env python3
"""Regression guards for Forward+ luminance sorting and PBR specialization maps."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def expect(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def read_rel(*parts: str) -> str:
    path = ROOT.joinpath(*parts)
    if not path.is_file():
        fail(f"missing file: {path}")
    return path.read_text(encoding="utf-8")


def require_literal(text: str, literal: str, context: str) -> None:
    expect(literal in text, f"{context}: expected literal {literal!r}")


def parse_uint_define(text: str, name: str) -> int:
    match = re.search(rf"^\s*#define\s+{re.escape(name)}\s+(\d+)u\b", text, re.MULTILINE)
    if not match:
        fail(f"missing unsigned define {name}")
    return int(match.group(1))


def parse_shader_constants(text: str) -> dict[str, int]:
    constants: dict[str, int] = {}
    for line in text.splitlines():
        line = line.split("//", 1)[0]
        match = re.search(
            r"layout\s*\(\s*constant_id\s*=\s*(\d+)\s*\)\s*const\s+\w+\s+(\w+)\b",
            line,
        )
        if match:
            constants[match.group(2)] = int(match.group(1))
    return constants


def check_pbr_fragment_specialization_bounds() -> None:
    source = read_rel("src", "renderers", "vulkan", "vk_create_pipeline.c")
    shader = read_rel("src", "renderers", "vulkan", "shaders", "glsl", "gen_frag.tmpl")

    pbr_capacity_match = re.search(
        r"#ifdef USE_VK_PBR.*?VkSpecializationMapEntry\s+spec_entries\[(\d+)\];"
        r"\s*_Static_assert\s*\(\s*sizeof\(\s*spec_entries\s*\)\s*/\s*"
        r"sizeof\(\s*spec_entries\[0\]\s*\)\s*>=\s*(\d+)u",
        source,
        re.DOTALL,
    )
    non_pbr_capacity_match = re.search(
        r"#else\s+VkSpecializationMapEntry\s+spec_entries\[(\d+)\];"
        r"\s*_Static_assert\s*\(\s*sizeof\(\s*spec_entries\s*\)\s*/\s*"
        r"sizeof\(\s*spec_entries\[0\]\s*\)\s*>=\s*(\d+)u",
        source,
        re.DOTALL,
    )
    expect(pbr_capacity_match is not None, "missing PBR spec_entries[] capacity/static assert")
    expect(non_pbr_capacity_match is not None, "missing non-PBR spec_entries[] capacity/static assert")
    pbr_capacity = int(pbr_capacity_match.group(1))
    pbr_static_min = int(pbr_capacity_match.group(2))
    non_pbr_capacity = int(non_pbr_capacity_match.group(1))
    non_pbr_static_min = int(non_pbr_capacity_match.group(2))

    add_region_match = re.search(r"#define ADD_FRAG_SPEC.*?#undef ADD_FRAG_SPEC", source, re.DOTALL)
    expect(add_region_match is not None, "missing ADD_FRAG_SPEC region")
    add_region = add_region_match.group(0)
    pbr_region_match = re.search(r"#ifdef USE_VK_PBR(.*?)#endif", add_region, re.DOTALL)
    expect(pbr_region_match is not None, "missing USE_VK_PBR ADD_FRAG_SPEC block")

    spec_pattern = re.compile(r"^\s*ADD_FRAG_SPEC\(\s*(\d+)\s*,\s*(\w+)\s*\);", re.MULTILINE)
    all_specs = [(int(cid), field) for cid, field in spec_pattern.findall(add_region)]
    pbr_specs = [(int(cid), field) for cid, field in spec_pattern.findall(pbr_region_match.group(1))]
    base_specs = [(cid, field) for cid, field in all_specs if (cid, field) not in pbr_specs]

    expect(base_specs, "no base fragment specialization entries found")
    expect(pbr_specs, "no PBR fragment specialization entries found")
    expect(len({cid for cid, _ in all_specs}) == len(all_specs), "duplicate fragment constant_id entries")
    expect([cid for cid, _ in all_specs] == list(range(0, max(cid for cid, _ in all_specs) + 1)),
           "fragment specialization constant IDs must remain contiguous from 0")
    expect(len(base_specs) <= non_pbr_capacity, "base fragment specialization entries exceed non-PBR array")
    expect(len(base_specs) <= non_pbr_static_min, "non-PBR static assert no longer covers base entries")
    expect(len(all_specs) <= pbr_capacity, "PBR fragment specialization entries exceed PBR array")
    expect(len(all_specs) <= pbr_static_min, "PBR static assert no longer covers all fragment entries")

    shader_constants = parse_shader_constants(shader)
    require_literal(shader, "forward_plus_shade_strength", "gen_frag Forward+ shade constant")
    for cid, field in all_specs:
        if field in shader_constants:
            expect(shader_constants[field] == cid,
                   f"constant_id mismatch for {field}: C has {cid}, gen_frag.tmpl has {shader_constants[field]}")

    require_literal(source, "ADD_FRAG_SPEC( 40, forward_plus_shade_strength );",
                    "Forward+ shade specialization map entry")
    require_literal(source, "Com_Clamp( 0.0f, 4.0f, r_forwardPlusShade->value )",
                    "Forward+ shade strength clamp")
    require_literal(source, "fragment specialization map overflow",
                    "fragment specialization overflow diagnostic")


def check_forward_plus_luminance_sort() -> None:
    forward = read_rel("src", "renderers", "vulkan", "vk_forward_plus.c")
    tr_init = read_rel("src", "renderers", "vulkan", "tr_init.c")
    shader = read_rel("src", "renderers", "vulkan", "shaders", "glsl", "forward_plus_tile_cull.comp")

    max_per_tile_c = parse_uint_define(forward, "VK_FP_MAX_PER_TILE")
    max_per_tile_shader = parse_uint_define(shader, "MAX_PER_TILE")
    expect(max_per_tile_c == max_per_tile_shader,
           f"Forward+ C max-per-tile ({max_per_tile_c}) must match shader ({max_per_tile_shader})")

    cvar_match = re.search(
        r'r_forwardPlusLuminanceSort\s*=\s*ri\.Cvar_Get\(\s*"r_forwardPlusLuminanceSort"\s*,\s*"1"\s*,\s*([^)]+)\);',
        tr_init,
    )
    expect(cvar_match is not None, "r_forwardPlusLuminanceSort must be registered default-on")
    expect("CVAR_LATCH" not in cvar_match.group(1),
           "r_forwardPlusLuminanceSort must stay runtime-toggleable, not latched")
    require_literal(tr_init, 'ri.Cvar_CheckRange( r_forwardPlusLuminanceSort, "0", "1", CV_INTEGER );',
                    "luminance-sort cvar range")
    require_literal(tr_init, "no vid_restart", "luminance-sort cvar description")
    require_literal(forward, "[VK][Forward+] r_forwardPlusLuminanceSort=1",
                    "luminance-sort startup log")

    require_literal(forward, "uint32_t luminance_sort;", "luminance-sort push struct member")
    require_literal(forward,
                    "push.luminance_sort = ( r_forwardPlusLuminanceSort && r_forwardPlusLuminanceSort->integer ) ? 1u : 0u;",
                    "luminance-sort push assignment")
    require_literal(forward,
                    "qvkCmdPushConstants( vk.cmd->command_buffer, vk.forward_plus.pipeline_layout",
                    "Forward+ push constants upload")
    require_literal(forward, "sizeof( push )", "Forward+ push constants size")

    require_literal(shader, "uint luminanceSort;", "shader push constant member")
    require_literal(shader, "if (pc.luminanceSort != 0u && candCount > cap)",
                    "shader luminance-sort overload guard")
    require_literal(shader, "if (candLum[j] > bestVal)", "shader brightest-light comparison")
    require_literal(shader, "candLi[bestJ] = candLi[k];", "shader candidate index swap")
    require_literal(shader, "candLum[bestJ] = candLum[k];", "shader candidate luminance swap")


def main() -> None:
    check_pbr_fragment_specialization_bounds()
    check_forward_plus_luminance_sort()
    print("PASS: test_vulkan_forward_plus_pbr_guards")


if __name__ == "__main__":
    main()
