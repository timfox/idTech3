#!/usr/bin/env bash
# Regression checks for the extracted client demo record/playback module.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${1:-$(cd "$SCRIPT_DIR/../.." && pwd)}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

command -v python3 >/dev/null 2>&1 || fail "python3 not in PATH"

python3 - "$PROJECT_ROOT" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
cl_demo = root / "src/client/cl_demo.c"
cl_demo_h = root / "src/client/cl_demo.h"
cl_main = root / "src/client/cl_main.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read(path: Path) -> str:
    if not path.is_file():
        fail(f"missing source file: {path}")
    return path.read_text()


def find_function_body(text: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*\(", text)
    if not match:
        fail(f"{name}: function not found")

    start = text.find("{", match.end())
    if start < 0:
        fail(f"{name}: opening brace not found")

    depth = 0
    i = start
    state = "code"
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line_comment"
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block_comment"
                i += 2
                continue
            if ch == '"':
                state = "string"
                i += 1
                continue
            if ch == "'":
                state = "char"
                i += 1
                continue
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return text[start : i + 1]
        elif state == "line_comment":
            if ch == "\n":
                state = "code"
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                state = "code"
                i += 2
                continue
        elif state == "string":
            if ch == "\\":
                i += 2
                continue
            if ch == '"':
                state = "code"
        elif state == "char":
            if ch == "\\":
                i += 2
                continue
            if ch == "'":
                state = "code"
        i += 1

    fail(f"{name}: closing brace not found")


def strip_preprocessor_else_branches(body: str) -> str:
    """Keep active-looking source guards stable across #ifdef variants."""
    lines = []
    skip_depth = None
    depth = 0
    for line in body.splitlines():
        stripped = line.lstrip()
        if stripped.startswith("#if"):
            if skip_depth is None:
                lines.append(line)
            depth += 1
            continue
        if stripped.startswith("#else") or stripped.startswith("#elif"):
            if skip_depth is None:
                skip_depth = depth
            continue
        if stripped.startswith("#endif"):
            if skip_depth == depth:
                skip_depth = None
            depth -= 1
            if skip_depth is None:
                lines.append(line)
            continue
        if skip_depth is None:
            lines.append(line)
    return "\n".join(lines)


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected {needle!r}")


def assert_not_regex(haystack: str, pattern: str, context: str) -> None:
    if re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: unexpected pattern {pattern!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_order(haystack: str, first: str, second: str, context: str) -> None:
    first_pos = haystack.find(first)
    second_pos = haystack.find(second)
    if first_pos < 0 or second_pos < 0 or first_pos >= second_pos:
        fail(f"{context}: expected {first!r} before {second!r}")


demo_text = read(cl_demo)
header_text = read(cl_demo_h)
main_text = read(cl_main)

# The module boundary must remain intact: cl_main owns integration points while
# record/playback implementation stays in cl_demo.c.
assert_contains(main_text, '#include "cl_demo.h"', "cl_main demo include")
assert_contains(header_text, "void CL_Demo_InitCommands( void );", "demo init declaration")
assert_contains(header_text, "void CL_Demo_ShutdownCommands( void );", "demo shutdown declaration")
assert_contains(header_text, "void CL_Demo_WriteServerPacket( msg_t *msg, int headerBytes );", "demo packet declaration")
assert_contains(main_text, "CL_Demo_InitCommands();", "client command registration hook")
assert_contains(main_text, "CL_Demo_ShutdownCommands();", "client command shutdown hook")
assert_contains(main_text, "CL_Demo_WriteServerPacket( msg, headerBytes );", "server packet recording hook")
for name in ("CL_Record_f", "CL_PlayDemo_f", "CL_ReadDemoMessage", "CL_WriteGamestate"):
    assert_not_regex(main_text, r"\b(?:static\s+)?(?:void|int|qboolean)\s+" + re.escape(name) + r"\s*\(", f"cl_main should not define {name}")

stop_body = find_function_body(demo_text, "CL_StopRecord_f")
assert_contains(stop_body, "len = -1;", "record stop sentinel")
assert_regex(stop_body, r"FS_Write\s*\(\s*&len\s*,\s*4\s*,\s*clc\.recordfile\s*\).*FS_Write\s*\(\s*&len\s*,\s*4\s*,\s*clc\.recordfile\s*\)", "record stop writes two sentinels")
assert_contains(stop_body, "if ( clc.dm68compat || clc.demoplaying )", "record protocol compatibility branch")
assert_contains(stop_body, "protocol = OLD_PROTOCOL_VERSION;", "old protocol finalization")
assert_contains(stop_body, "protocol = NEW_PROTOCOL_VERSION;", "new protocol finalization")
assert_contains(stop_body, "if ( com_protocol->integer != DEFAULT_PROTOCOL_VERSION )", "custom protocol override")
assert_contains(stop_body, 'Com_sprintf( tempName, sizeof( tempName ), "%s.tmp", clc.recordName );', "temporary demo path")
assert_contains(stop_body, "if ( clc.explicitRecordName )", "explicit record overwrite branch")
assert_contains(stop_body, "FS_Remove( finalName );", "explicit record removes previous file")
assert_contains(stop_body, "while ( FS_FileExists( finalName ) && ++sequence < 1000 )", "automatic record collision loop")
assert_order(stop_body, "FS_Remove( finalName );", "FS_Rename( tempName, finalName );", "remove before explicit rename")
assert_order(stop_body, "while ( FS_FileExists( finalName ) && ++sequence < 1000 )", "FS_Rename( tempName, finalName );", "collision scan before rename")

record_body = find_function_body(demo_text, "CL_Record_f")
assert_contains(record_body, "if ( Cmd_Argc() > 2 )", "record argc guard")
assert_contains(record_body, "if ( clc.demorecording )", "record already-active guard")
assert_contains(record_body, "if ( cls.state != CA_ACTIVE )", "record active-state guard")
assert_contains(record_body, "Q_strncpyz( demoName, Cmd_Argv( 1 ), sizeof( demoName ) );", "explicit demo name bounded copy")
assert_contains(record_body, 'Com_sprintf( demoExt, sizeof( demoExt ), "%s%d", DEMOEXT, OLD_PROTOCOL_VERSION );', "old extension strip")
assert_contains(record_body, 'Com_sprintf( demoExt, sizeof( demoExt ), "%s%d", DEMOEXT, NEW_PROTOCOL_VERSION );', "new extension strip")
assert_contains(record_body, "clc.explicitRecordName = qtrue;", "explicit record flag")
assert_contains(record_body, "clc.explicitRecordName = qfalse;", "automatic record flag")
assert_contains(record_body, "clc.demowaiting = qtrue;", "record waits for first packet")
assert_contains(record_body, "clc.dm68compat = qtrue;", "initial old-protocol compatibility")
assert_contains(record_body, "CL_WriteGamestate( qtrue );", "record starts with gamestate")

read_body = find_function_body(demo_text, "CL_ReadDemoMessage")
assert_contains(read_body, "if ( clc.demofile == FS_INVALID_HANDLE )", "demo invalid handle guard")
assert_contains(read_body, "r = FS_Read( &s, 4, clc.demofile );", "demo reads sequence first")
assert_contains(read_body, "if ( r != 4 )", "demo short sequence/length guard")
assert_contains(read_body, "buf.cursize = LittleLong( buf.cursize );", "demo length endian conversion")
assert_contains(read_body, "if ( buf.cursize == -1 )", "demo EOF sentinel")
assert_contains(read_body, "if ( buf.cursize > buf.maxsize )", "demo oversize guard")
assert_contains(read_body, 'Com_Error( ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );', "demo oversize drop")
assert_order(read_body, "if ( buf.cursize > buf.maxsize )", "FS_Read( buf.data, buf.cursize, clc.demofile );", "oversize check before payload read")
assert_contains(read_body, 'Com_Printf( "Demo file was truncated.\\n" );', "demo truncated read diagnostic")
assert_order(read_body, "CL_ParseServerMessage( &buf );", "if ( clc.demorecording )", "parse before re-record decision")
assert_contains(read_body, "CL_WriteGamestate( qfalse );", "re-record gamestate on event")
assert_contains(read_body, "CL_WriteSnapshot();", "re-record snapshot on event")

walk_body = find_function_body(demo_text, "CL_WalkDemoExt")
assert_contains(walk_body, "while ( demo_protocols[i] )", "protocol fallback iteration")
assert_contains(walk_body, 'Com_sprintf( name, name_len, "demos/%s.%s%d", arg, DEMOEXT, demo_protocols[i] );', "protocol fallback filename")
assert_order(walk_body, "FS_BypassPure();", "FS_FOpenFileRead( name, handle, qtrue );", "pure bypass before fallback read")
assert_order(walk_body, "FS_FOpenFileRead( name, handle, qtrue );", "FS_RestorePure();", "pure restore after fallback read")
assert_contains(walk_body, "return demo_protocols[i];", "protocol fallback return")

callback_body = find_function_body(demo_text, "CL_DemoNameCallback_f")
assert_contains(callback_body, 'Q_stricmpn( filename + length - ( ext_len + num_len ), "." DEMOEXT, (size_t)ext_len )', "demo completion extension filter")
assert_contains(callback_body, "if ( version == com_protocol->integer )", "demo completion current protocol")
assert_contains(callback_body, "if ( version < 66 || version > NEW_PROTOCOL_VERSION )", "demo completion protocol range")

play_body = find_function_body(strip_preprocessor_else_branches(demo_text), "CL_PlayDemo_f")
assert_contains(play_body, "if ( Cmd_Argc() != 2 )", "demo argc guard")
assert_contains(play_body, "ext_test = strrchr( arg, '.' );", "explicit extension parsing")
assert_contains(play_body, "protocol = atoi( ext_test + ARRAY_LEN( DEMOEXT ) );", "explicit protocol parse")
assert_contains(play_body, "if ( demo_protocols[i] || protocol == com_protocol->integer )", "supported explicit protocol branch")
assert_order(play_body, "FS_BypassPure();", "FS_FOpenFileRead( name, &hFile, qtrue );", "pure bypass before explicit read")
assert_order(play_body, "FS_FOpenFileRead( name, &hFile, qtrue );", "FS_RestorePure();", "pure restore after explicit read")
assert_contains(play_body, 'Com_Printf( "Protocol %d not supported for demos\\n", protocol );', "unsupported protocol diagnostic")
assert_contains(play_body, "if ( len > ARRAY_LEN( retry ) - 1 )", "unsupported protocol retry length cap")
assert_contains(play_body, "Q_strncpyz( retry, arg, len + 1 );", "unsupported protocol retry bounded copy")
assert_contains(play_body, "retry[len] = '\\0';", "unsupported protocol retry terminator")
assert_contains(play_body, "protocol = CL_WalkDemoExt( retry, name, sizeof( name ), &hFile );", "unsupported protocol fallback")
assert_contains(play_body, "FS_FCloseFile( hFile );", "test handle close before playback")
assert_order(play_body, 'Cvar_Set( "sv_killserver", "2" );', "CL_Disconnect( qtrue );", "killserver before disconnect")
assert_order(play_body, "CL_Disconnect( qtrue );", "FS_FOpenFileRead( name, &clc.demofile, qtrue )", "disconnect before playback reopen")
assert_contains(play_body, "Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );", "demo name bounded copy")
assert_order(play_body, "cls.state = CA_CONNECTED;", "clc.demoplaying = qtrue;", "connected before demoplaying")
assert_order(play_body, "clc.demoplaying = qtrue;", "Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );", "demoplaying before servername")
assert_contains(play_body, "if ( protocol <= OLD_PROTOCOL_VERSION )", "compat protocol branch")
assert_order(play_body, "if ( protocol <= OLD_PROTOCOL_VERSION )", "while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED", "compat set before priming")
assert_contains(play_body, "CL_ReadDemoMessage();", "priming reads demo messages")
assert_contains(play_body, "clc.firstDemoFrameSkipped = qfalse;", "demo first-frame reset")

init_body = find_function_body(demo_text, "CL_Demo_InitCommands")
shutdown_body = find_function_body(demo_text, "CL_Demo_ShutdownCommands")
for command in ("record", "demo", "stoprecord"):
    assert_contains(init_body, f'Cmd_AddCommand( "{command}",', f"{command} command registration")
    assert_contains(shutdown_body, f'Cmd_RemoveCommand( "{command}" );', f"{command} command removal")
assert_contains(init_body, 'Cmd_SetCommandCompletionFunc( "record", CL_CompleteRecordName );', "record completion registration")
assert_contains(init_body, 'Cmd_SetCommandCompletionFunc( "demo", CL_CompleteDemoName );', "demo completion registration")

print("PASS: test_client_demo_regressions")
PY
