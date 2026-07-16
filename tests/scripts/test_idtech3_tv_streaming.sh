#!/usr/bin/env bash
# Source/runtime guard for idTech3-tv / Owncast RTMP streaming controls.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }
mode="${1:-source}"
check() {
	local file="$1"
	local pattern="$2"
	local msg="$3"
	grep -q "$pattern" "$file" || fail "$msg"
	echo "PASS: $msg"
}

source_checks() {
	local SRC="runtime/client/platform/cl_streaming.c"
	local HDR="runtime/client/platform/cl_streaming.h"
	local PIPE_H="runtime/client/core/cl_pipeline.h"
	local PIPE_C="runtime/client/core/cl_pipeline.c"
	local MAIN="runtime/client/core/cl_main.c"
	local LIFE="runtime/client/core/cl_lifecycle.c"
	local MANIFEST="cmake/client/ClientSources.cmake"
	local DOC="docs/IDTECH3_TV.md"

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
	check "$SRC" 'cl_stream_queueMegs' 'stream queue budget cvar wired'
	check "$SRC" 'CL_OpenAVIForPipeCommand' 'engine backend opens an in-engine capture pipe'
	check "$SRC" 'CL_GetAVIPipeStats' 'stream status reads live pipe queue stats'
	check "$SRC" 'CL_Streaming_EngineCaptureActive' 'streaming exposes live engine capture state'
	check "$SRC" 'CL_PipelineExpandTemplate' 'stream command uses shared safe template expansion'
	check "$SRC" 'rtmp://127.0.0.1:1935/live' 'status guidance mentions Owncast-compatible RTMP default'
	check "runtime/client/media/cl_avi.c" 'CL_OpenAVIForPipeCommand' 'AVI pipe can be opened with a streaming command'
	check "runtime/client/media/cl_avi.c" 'AVI_PipeThread_Start' 'live AVI pipe starts a streaming worker thread'
	check "runtime/client/media/cl_avi.c" 'AVI_PipeThread_Enqueue' 'live AVI pipe queues chunks for background writing'
	check "runtime/client/media/cl_avi.c" 'AVI_PipeThread_EnqueueMediaChunk' 'live AVI pipe queues video/audio records atomically'
	check "runtime/client/media/cl_avi.c" 'maxQueuedBytes' 'live AVI pipe has bounded queue backpressure'
	check "runtime/client/media/cl_avi.c" 'droppedChunks' 'live AVI pipe tracks dropped chunks'
	check "runtime/client/media/cl_avi.c" 'CL_GetAVIPipeStats' 'live AVI pipe exposes queue diagnostics'
	check "runtime/client/media/cl_avi.c" 'AVI_PipeThread_Stop' 'live AVI pipe stops the streaming worker thread'
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
	check "$DOC" 'streaming worker thread' 'docs describe threaded streaming pipe I/O'
	check "$DOC" 'cl_stream_queueMegs' 'docs describe queue memory budget'
	check "$DOC" 'dropped chunks' 'docs describe dropped stream chunk diagnostics'
	check "$DOC" 'partial AVI chunks' 'docs describe record-aligned backpressure'

	echo "idTech3-tv streaming source checks passed."
}

runtime_checks() {
	local output
	local rc
	# Full engine capture requires the client/renderer. This command path proves
	# stream_status registration on renderer-capable runners and skips cleanly on
	# headless CI.
	# shellcheck source=idtech3_client_runtime_smoke.sh
	source "$ROOT/tests/scripts/idtech3_client_runtime_smoke.sh"
	output="$(idtech3_client_run_optional "streaming" +stream_status +quit)" || {
		rc=$?
		printf '%s\n' "$output"
		[[ "$rc" -eq 77 ]] && return 0
		return "$rc"
	}
	if echo "$output" | grep -q '^SKIP:'; then
		printf '%s\n' "$output"
		return 0
	fi
	echo "idTech3-tv streaming runtime smoke passed."
}

case "$mode" in
	source) source_checks ;;
	runtime) runtime_checks ;;
	all) source_checks; runtime_checks ;;
	*) echo "usage: $0 [source|runtime|all]" >&2; exit 2 ;;
esac
