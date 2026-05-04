#!/usr/bin/env bash
# Regression checks for recent OpenGL glTF material/tangent fixes.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
GLTF_LOADER="$PROJECT_ROOT/src/renderers/vulkan/tr_model_gltf.c"
GLTF_OPENGL="$PROJECT_ROOT/src/renderers/opengl/tr_gltf_rb_opengl.c"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected to find '$needle'"
	fi
}

assert_count() {
	local haystack="$1"
	local needle="$2"
	local expected="$3"
	local context="$4"
	local count

	count="$(HAYSTACK="$haystack" NEEDLE="$needle" awk '
		BEGIN {
			n = ENVIRON["NEEDLE"];
			c = 0;
			while ((getline line < "/dev/stdin") > 0) {
				pos = 1;
				while ((idx = index(substr(line, pos), n)) > 0) {
					c++;
					pos += idx + length(n) - 1;
				}
			}
			print c;
		}
	' <<<"$haystack")"
	if [[ "$count" != "$expected" ]]; then
		fail "$context: expected $expected occurrences of '$needle', found $count"
	fi
}

assert_order() {
	local haystack="$1"
	local first="$2"
	local second="$3"
	local context="$4"
	local order

	order="$(FIRST="$first" SECOND="$second" awk '
		BEGIN {
			a = ENVIRON["FIRST"];
			b = ENVIRON["SECOND"];
			first_line = 0;
			second_line = 0;
		}
		{
			if (first_line == 0 && index($0, a) > 0) {
				first_line = NR;
			}
			if (second_line == 0 && index($0, b) > 0) {
				second_line = NR;
			}
		}
		END {
			if (first_line > 0 && second_line > 0 && first_line < second_line) {
				print "ok";
			} else {
				printf("bad:%d:%d\n", first_line, second_line);
			}
		}
	' <<<"$haystack")"
	if [[ "$order" != "ok" ]]; then
		fail "$context: expected '$first' before '$second' ($order)"
	fi
}

extract_function() {
	local file="$1"
	local name="$2"

	awk -v name="$name" '
		index($0, name "(") > 0 && in_fn == 0 {
			in_fn = 1;
		}
		in_fn {
			line = $0;
			print line;
			opens += gsub(/\{/, "{", line);
			closes += gsub(/\}/, "}", line);
			if (opens > 0 && opens == closes) {
				exit;
			}
		}
	' "$file"
}

if [[ ! -f "$GLTF_LOADER" ]]; then
	fail "missing glTF loader source: $GLTF_LOADER"
fi
if [[ ! -f "$GLTF_OPENGL" ]]; then
	fail "missing OpenGL glTF source: $GLTF_OPENGL"
fi

register_body="$(extract_function "$GLTF_LOADER" "R_GLTF_RegisterSurfaceShader")"
if [[ -z "$register_body" ]]; then
	fail "could not extract R_GLTF_RegisterSurfaceShader"
fi

assert_contains "$register_body" "if ( mat->normalTexture[0] )" "normal-texture shader branch"
assert_count "$register_body" "RE_RegisterShaderNoMip( normName )" "2" "normal shader lookup attempts"
assert_contains "$register_body" "COM_StripExtension( mat->normalTexture, normName, sizeof( normName ) );" "normal texture strips extension"
assert_contains "$register_body" "Q_strcat( normName, sizeof( normName ), \"_norm\" );" "normal shader suffix"
assert_contains "$register_body" "normName[len - 2] == '_'" "trailing _n detection"
assert_contains "$register_body" "normName[len - 1] == 'n' || normName[len - 1] == 'N'" "case-insensitive _n fallback"
assert_contains "$register_body" "normName[len - 2] = '\0';" "strip trailing _n before _norm fallback"
assert_contains "$register_body" "RE_RegisterShaderNoMip( mat->baseColorTexture )" "base-color shader fallback"
assert_order "$register_body" "if ( mat->normalTexture[0] )" "if ( mat->baseColorTexture[0] )" "normal shader lookup precedes base-color fallback"
assert_order "$register_body" "normName[len - 2] = '\0';" "if ( mat->baseColorTexture[0] )" "base_norm fallback precedes base-color fallback"

normal_path_body="$(extract_function "$GLTF_OPENGL" "RB_GLTF_ImagePathLooksLikeNormalMap")"
if [[ -z "$normal_path_body" ]]; then
	fail "could not extract RB_GLTF_ImagePathLooksLikeNormalMap"
fi

assert_contains "$normal_path_body" "if ( !path || !path[0] )" "normal-map path empty guard"
assert_contains "$normal_path_body" "Q_stristr( path, \"norm\" )" "normal-map norm heuristic"
assert_contains "$normal_path_body" "Q_stristr( path, \"bump\" )" "normal-map bump heuristic"
assert_contains "$normal_path_body" "Q_stristr( path, \"nmap\" )" "normal-map nmap heuristic"
assert_contains "$normal_path_body" "Q_stristr( path, \"_n.\" )" "normal-map Q3 _n heuristic"

shader_body="$(extract_function "$GLTF_OPENGL" "RB_GLTF_ShaderUsesNormalMap")"
if [[ -z "$shader_body" ]]; then
	fail "could not extract RB_GLTF_ShaderUsesNormalMap"
fi

assert_contains "$shader_body" "if ( !sh || sh->defaultShader )" "shader normal-map default guard"
assert_contains "$shader_body" "i < sh->numUnfoggedPasses" "shader normal-map stage scan"
assert_contains "$shader_body" "b < NUM_TEXTURE_BUNDLES" "shader normal-map bundle scan"
assert_contains "$shader_body" "a < tb->numImageAnimations && a < MAX_IMAGE_ANIMATIONS" "shader normal-map animation bounds"
assert_contains "$shader_body" "RB_GLTF_ImagePathLooksLikeNormalMap( img->imgName )" "shader normal-map path heuristic use"

recompute_body="$(extract_function "$GLTF_OPENGL" "RB_GLTF_RecomputeQtangentsForTessRange")"
if [[ -z "$recompute_body" ]]; then
	fail "could not extract RB_GLTF_RecomputeQtangentsForTessRange"
fi

assert_contains "$recompute_body" "if ( numVerts <= 0 || numIndexes < 3 )" "qtangent recompute empty guard"
assert_contains "$recompute_body" "denom = duv1[0] * duv2[1] - duv2[0] * duv1[1]" "qtangent UV determinant"
assert_contains "$recompute_body" "if ( fabsf( denom ) < 1e-12f )" "qtangent degenerate UV guard"
assert_contains "$recompute_body" "d = DotProduct( n, tanAcc[k] );" "qtangent Gram-Schmidt dot"
assert_contains "$recompute_body" "t[0] = tanAcc[k][0] - n[0] * d;" "qtangent tangent projection"
assert_contains "$recompute_body" "if ( VectorLength( t ) < 1e-8f )" "qtangent fallback axis guard"
assert_contains "$recompute_body" "tess.qtangent[vi][3] = ( d < 0.0f ) ? -1.0f : 1.0f;" "qtangent handedness preservation"
assert_order "$recompute_body" "ri.Hunk_FreeTempMemory( btAcc );" "ri.Hunk_FreeTempMemory( tanAcc );" "qtangent temp memory cleanup order"

surface_body="$(extract_function "$GLTF_OPENGL" "RB_GLTFSurface")"
if [[ -z "$surface_body" ]]; then
	fail "could not extract RB_GLTFSurface"
fi

assert_contains "$surface_body" "r_gltfCpuQtangent && r_gltfCpuQtangent->integer" "CPU qtangent cvar gate"
assert_contains "$surface_body" "tess.shader && RB_GLTF_ShaderUsesNormalMap( tess.shader )" "CPU qtangent normal-map gate"
assert_contains "$surface_body" "RB_GLTF_RecomputeQtangentsForTessRange( base, surf->numVertices, idxBase, surf->numIndices );" "CPU qtangent recompute call"
assert_order "$surface_body" "tess.numIndexes += surf->numIndices;" "RB_GLTF_RecomputeQtangentsForTessRange( base, surf->numVertices, idxBase, surf->numIndices );" "CPU qtangent recompute after index upload"

echo "PASS: test_gltf_opengl_regressions"
