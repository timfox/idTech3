#!/usr/bin/env bash
# Source guard for idTech3-tv / Owncast RTMP streaming controls.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
check() {
	local file="$1"
	local pattern="$2"
	local msg="$3"
	grep -q "$pattern" "$file" || fail "$msg"
	echo "PASS: $msg"
}

SRC="runtime/client/platform/cl_streaming.c"
HDR="runtime/client/platform/cl_streaming.h"
PIPE_H="runtime/client/core/cl_pipeline.h"
PIPE_C="runtime/client/core/cl_pipeline.c"
MAIN="runtime/client/core/cl_main.c"
LIFE="runtime/client/core/cl_lifecycle.c"
MANIFEST="cmake/client/ClientSources.cmake"
DOC="docs/IDTECH3_TV.md"

[[ -f "$SRC" ]] || fail "missing streaming source"
[[ -f "$HDR" ]] || fail "missing streaming header"

check "$SRC" 'stream_start' 'stream_start command implemented'
check "$SRC" 'stream_stop' 'stream_stop command implemented'
check "$SRC" 'stream_status' 'stream_status command implemented'
check "$SRC" 'cl_stream_url' 'RTMP URL cvar wired'
check "$SRC" 'cl_stream_key' 'protected stream key cvar wired'
check "$SRC" 'CVAR_PROTECTED' 'stream key is protected'
check "$SRC" 'idTech3-tv / Owncast' 'integration targets external idTech3-tv/Owncast service'
check "$SRC" 'cl_stream_backend' 'stream backend selector cvar wired'
check "$SRC" 'CL_OpenAVIForPipeCommand' 'engine backend opens an in-engine capture pipe'
check "$SRC" 'CL_Streaming_EngineCaptureActive' 'streaming exposes live engine capture state'
check "$SRC" 'CL_PipelineExpandTemplate' 'stream command uses shared safe template expansion'
check "$SRC" 'rtmp://127.0.0.1:1935/live' 'status guidance mentions Owncast-compatible RTMP default'
check "runtime/client/media/cl_avi.c" 'CL_OpenAVIForPipeCommand' 'AVI pipe can be opened with a streaming command'
check "runtime/client/media/cl_demo.c" 'CL_Streaming_EngineCaptureActive' 'live streaming captures frames outside demo playback'
check "$PIPE_H" 'const char \*url;' 'pipeline template has streaming URL token field'
check "$PIPE_H" 'const char \*key;' 'pipeline template has streaming key token field'
check "$PIPE_C" "p\\[1\\] == 'U'" 'pipeline expands %U RTMP URL'
check "$PIPE_C" "p\\[1\\] == 'K'" 'pipeline expands %K stream key'
check "$PIPE_C" "p\\[1\\] == 'V'" 'pipeline expands %V video bitrate'
check "$MAIN" 'CL_Streaming_Init' 'client initializes streaming controls'
check "$LIFE" 'CL_Streaming_Shutdown' 'client shuts down streaming controls'
check "$MANIFEST" 'platform/cl_streaming.c' 'streaming source included in client manifest'
check "$DOC" 'stream_start' 'streaming docs mention start command'
check "$DOC" 'cl_stream_cmd' 'streaming docs mention command template'
check "$DOC" 'renderer capture path' 'docs describe engine-owned frame capture'
check "$DOC" 'mixed game audio' 'docs describe engine-owned audio capture'

echo "idTech3-tv streaming integration checks passed."
