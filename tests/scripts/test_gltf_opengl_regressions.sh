#!/usr/bin/env bash
# Regression checks for recent OpenGL glTF material and tangent fixes.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

OPENGL_GLTF="$PROJECT_ROOT/src/renderers/opengl/tr_gltf_rb_opengl.c"
VULKAN_GLTF="$PROJECT_ROOT/src/renderers/vulkan/tr_model_gltf.c"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected '$needle'"
	fi
}

assert_order() {
	local haystack="$1"
	local first="$2"
	local second="$3"
	local context="$4"
	local first_line second_line

	first_line="$(printf '%s\n' "$haystack" | awk -v needle="$first" 'index($0, needle) { print NR; exit }')"
	second_line="$(printf '%s\n' "$haystack" | awk -v needle="$second" 'index($0, needle) { print NR; exit }')"
	if [[ -z "$first_line" || -z "$second_line" || "$first_line" -ge "$second_line" ]]; then
		fail "$context: expected '$first' before '$second'"
	fi
}

function_body() {
	local file="$1"
	local signature="$2"
	awk -v signature="$signature" '
		index($0, signature) { in_func = 1 }
		in_func { print }
		in_func {
			for (i = 1; i <= length($0); i++) {
				ch = substr($0, i, 1)
				if (ch == "{") depth++
				else if (ch == "}") depth--
			}
			if (depth == 0 && NR > 1) exit
		}
	' "$file"
}

if [[ ! -f "$OPENGL_GLTF" ]]; then
	fail "missing OpenGL glTF source: $OPENGL_GLTF"
fi
if [[ ! -f "$VULKAN_GLTF" ]]; then
	fail "missing Vulkan glTF source: $VULKAN_GLTF"
fi

register_shader_body="$(function_body "$VULKAN_GLTF" "static shader_t *R_GLTF_RegisterSurfaceShader")"
normal_path_body="$(function_body "$OPENGL_GLTF" "static qboolean RB_GLTF_ImagePathLooksLikeNormalMap")"
normal_shader_body="$(function_body "$OPENGL_GLTF" "static qboolean RB_GLTF_ShaderUsesNormalMap")"
qtangent_body="$(function_body "$OPENGL_GLTF" "static void RB_GLTF_RecomputeQtangentsForTessRange")"
surface_body="$(function_body "$OPENGL_GLTF" "void RB_GLTFSurface")"

assert_contains "$register_shader_body" "COM_StripExtension( mat->normalTexture, normName, sizeof( normName ) );" "normal shader registration strips normalTexture extension"
assert_contains "$register_shader_body" "Q_strcat( normName, sizeof( normName ), \"_norm\" );" "normal shader registration tries _norm shader"
assert_contains "$register_shader_body" "normName[len - 2] == '_'" "normal shader registration detects trailing _n"
assert_contains "$register_shader_body" "normName[len - 1] == 'n' || normName[len - 1] == 'N'" "normal shader registration handles lowercase and uppercase _n"
assert_contains "$register_shader_body" "normName[len - 2] = '\\0';" "normal shader registration strips trailing _n before retry"
assert_contains "$register_shader_body" "RE_RegisterShaderNoMip( mat->baseColorTexture )" "normal shader registration falls back to base color texture"
assert_order "$register_shader_body" "RE_RegisterShaderNoMip( normName )" "RE_RegisterShaderNoMip( mat->baseColorTexture )" "normal shader registration tries normal shader before base color"

assert_contains "$normal_path_body" "if ( !path || !path[0] )" "normal-map path heuristic keeps empty path guard"
assert_contains "$normal_path_body" "Q_stristr( path, \"norm\" )" "normal-map path heuristic recognizes norm"
assert_contains "$normal_path_body" "Q_stristr( path, \"bump\" )" "normal-map path heuristic recognizes bump"
assert_contains "$normal_path_body" "Q_stristr( path, \"nmap\" )" "normal-map path heuristic recognizes nmap"
assert_contains "$normal_path_body" "Q_stristr( path, \"_n.\" )" "normal-map path heuristic recognizes Q3 _n textures"

assert_contains "$normal_shader_body" "if ( !sh || sh->defaultShader )" "normal-map shader scan ignores null/default shaders"
assert_contains "$normal_shader_body" "i < sh->numUnfoggedPasses" "normal-map shader scan iterates shader stages"
assert_contains "$normal_shader_body" "b < NUM_TEXTURE_BUNDLES" "normal-map shader scan iterates texture bundles"
assert_contains "$normal_shader_body" "a < tb->numImageAnimations && a < MAX_IMAGE_ANIMATIONS" "normal-map shader scan bounds image animations"
assert_contains "$normal_shader_body" "RB_GLTF_ImagePathLooksLikeNormalMap( img->imgName )" "normal-map shader scan uses shared path heuristic"

assert_contains "$qtangent_body" "fabsf( denom ) < 1e-12f" "qtangent recompute skips degenerate UVs"
assert_contains "$qtangent_body" "d = DotProduct( n, tanAcc[k] );" "qtangent recompute projects tangent against final normal"
assert_contains "$qtangent_body" "t[0] = tanAcc[k][0] - n[0] * d;" "qtangent recompute applies Gram-Schmidt projection"
assert_contains "$qtangent_body" "if ( VectorLength( t ) < 1e-8f )" "qtangent recompute keeps fallback for degenerate tangents"
assert_contains "$qtangent_body" "CrossProduct( n, up, t );" "qtangent recompute generates fallback axis"
assert_contains "$qtangent_body" "tess.qtangent[vi][3] = ( d < 0.0f ) ? -1.0f : 1.0f;" "qtangent recompute preserves handedness from bitangent accumulator"
assert_contains "$qtangent_body" "ri.Hunk_FreeTempMemory( btAcc );" "qtangent recompute frees bitangent temp memory"
assert_contains "$qtangent_body" "ri.Hunk_FreeTempMemory( tanAcc );" "qtangent recompute frees tangent temp memory"
assert_order "$qtangent_body" "ri.Hunk_FreeTempMemory( btAcc );" "ri.Hunk_FreeTempMemory( tanAcc );" "qtangent recompute frees allocations in reverse order"

assert_contains "$surface_body" "if ( r_gltfCpuQtangent && r_gltfCpuQtangent->integer &&" "surface path gates CPU qtangent recompute behind cvar"
assert_contains "$surface_body" "tess.shader && RB_GLTF_ShaderUsesNormalMap( tess.shader )" "surface path only recomputes qtangents for normal-mapped shaders"
assert_contains "$surface_body" "RB_GLTF_RecomputeQtangentsForTessRange( base, surf->numVertices, idxBase, surf->numIndices );" "surface path recomputes qtangents over uploaded tess range"
assert_order "$surface_body" "tess.numIndexes += surf->numIndices;" "RB_GLTF_RecomputeQtangentsForTessRange( base, surf->numVertices, idxBase, surf->numIndices );" "surface path recomputes after indices are uploaded"

echo "PASS: test_gltf_opengl_regressions"
