#!/usr/bin/env bash
# Regression checks for client demo record/playback extraction.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

python3 - "$PROJECT_ROOT" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
cl_demo = root / "src/client/cl_demo.c"
cl_demo_h = root / "src/client/cl_demo.h"
cl_main = root / "src/client/cl_main.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read(path: pathlib.Path) -> str:
    if not path.is_file():
        fail(f"missing source file: {path}")
    return path.read_text(encoding="utf-8")


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected to find {needle!r}")


def assert_not_contains(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        fail(f"{context}: unexpected {needle!r}")


def assert_order(haystack: str, first: str, second: str, context: str) -> None:
    first_pos = haystack.find(first)
    second_pos = haystack.find(second)
    if first_pos < 0 or second_pos < 0 or first_pos >= second_pos:
        fail(
            f"{context}: expected {first!r} before {second!r} "
            f"(positions {first_pos}, {second_pos})"
        )


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, re.MULTILINE | re.DOTALL):
        fail(f"{context}: expected pattern {pattern!r}")


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*", "", text)
    return text


def active_preprocessor_view(text: str) -> str:
    """Keep one branch from #if/#else blocks so brace matching is stable."""
    out: list[str] = []
    stack: list[bool] = []
    skipping = False
    for line in text.splitlines():
        stripped = line.lstrip()
        if stripped.startswith(("#if", "#ifdef", "#ifndef")):
            stack.append(skipping)
            out.append("")
            continue
        if stripped.startswith(("#elif", "#else")):
            if stack:
                skipping = True
            out.append("")
            continue
        if stripped.startswith("#endif"):
            if stack:
                skipping = stack.pop()
            out.append("")
            continue
        out.append("" if skipping else line)
    return "\n".join(out)


def extract_function(text: str, name: str) -> str:
    view = active_preprocessor_view(strip_comments(text))
    match = re.search(rf"^[^\n;]*\b{name}\s*\([^;]*\)\s*\{{", view, re.MULTILINE)
    if not match:
        fail(f"could not locate function {name}")
    start = match.start()
    brace = 0
    seen_open = False
    for index in range(match.start(), len(view)):
        char = view[index]
        if char == "{":
            brace += 1
            seen_open = True
        elif char == "}":
            brace -= 1
            if seen_open and brace == 0:
                return view[start : index + 1]
    fail(f"could not extract complete body for {name}")


demo_src = read(cl_demo)
demo_header = read(cl_demo_h)
main_src = read(cl_main)

assert_contains(main_src, '#include "cl_demo.h"', "cl_main demo module wiring")
assert_contains(main_src, "CL_Demo_WriteServerPacket( msg, headerBytes );", "packet-event demo write hook")
assert_contains(main_src, "CL_Demo_InitCommands();", "client init delegates demo commands")
assert_contains(main_src, "CL_Demo_ShutdownCommands();", "client shutdown delegates demo commands")
for command in ("record", "demo", "stoprecord"):
    assert_not_contains(main_src, f'Cmd_AddCommand( "{command}"', f"cl_main must not directly register {command}")
    assert_not_contains(main_src, f'Cmd_RemoveCommand( "{command}"', f"cl_main must not directly remove {command}")

for declaration in (
    "void CL_Demo_InitCommands( void );",
    "void CL_Demo_ShutdownCommands( void );",
    "void CL_Demo_WriteServerPacket( msg_t *msg, int headerBytes );",
):
    assert_contains(demo_header, declaration, "demo header public API")

write_packet = extract_function(demo_src, "CL_Demo_WriteServerPacket")
assert_contains(write_packet, "CL_WriteDemoMessage( msg, headerBytes );", "public demo packet wrapper")

stop_record = extract_function(demo_src, "CL_StopRecord_f")
assert_contains(stop_record, "if ( clc.recordfile != FS_INVALID_HANDLE )", "stoprecord open-file guard")
assert_contains(stop_record, "len = -1;", "stoprecord writes demo terminator sentinel")
assert_regex(stop_record, r"FS_Write\(\s*&len,\s*4,\s*clc\.recordfile\s*\);\s*FS_Write\(\s*&len,\s*4,\s*clc\.recordfile\s*\);", "stoprecord writes both sequence and length sentinels")
assert_contains(stop_record, "Com_sprintf( tempName, sizeof( tempName ), \"%s.tmp\", clc.recordName );", "stoprecord temp name")
assert_contains(stop_record, "Com_sprintf( finalName, sizeof( finalName ), \"%s.%s%d\", clc.recordName, DEMOEXT, protocol );", "stoprecord protocol extension")
assert_contains(stop_record, "if ( com_protocol->integer != DEFAULT_PROTOCOL_VERSION )", "stoprecord custom protocol override")
assert_contains(stop_record, "FS_Remove( finalName );", "explicit record name overwrite")
assert_contains(stop_record, "while ( FS_FileExists( finalName ) && ++sequence < 1000 )", "implicit record collision loop")
assert_contains(stop_record, "\"%s-%02d.%s%d\"", "implicit record sequence suffix")
assert_order(stop_record, "FS_FCloseFile( clc.recordfile );", "FS_Rename( tempName, finalName );", "stoprecord closes before rename")
assert_order(stop_record, "if ( clc.explicitRecordName )", "FS_Rename( tempName, finalName );", "stoprecord resolves collisions before rename")
assert_contains(stop_record, "clc.demorecording = qfalse;", "stoprecord clears recording flag")
assert_contains(stop_record, "clc.spDemoRecording = qfalse;", "stoprecord clears single-player flag")

record = extract_function(demo_src, "CL_Record_f")
assert_contains(record, "if ( Cmd_Argc() > 2 )", "record usage arg guard")
assert_contains(record, "if ( clc.demorecording )", "record duplicate guard")
assert_contains(record, "if ( cls.state != CA_ACTIVE )", "record active-level guard")
assert_contains(record, "NET_IsLocalAddress( &clc.serverAddress )", "record local synchronous warning")
assert_contains(record, "Q_strncpyz( demoName, Cmd_Argv( 1 ), sizeof( demoName ) );", "record bounded arg copy")
assert_contains(record, "Q_stricmp( ext, demoExt ) == 0", "record strips known demo extension")
assert_contains(record, "Com_sprintf( name, sizeof( name ), \"demos/%s\", demoName );", "record writes under demos directory")
assert_contains(record, "clc.explicitRecordName = qtrue;", "record explicit-name marker")
assert_contains(record, "clc.explicitRecordName = qfalse;", "record auto-name marker")
assert_contains(record, "Q_strcat( name, sizeof( name ), \".tmp\" );", "record temp file suffix")
assert_contains(record, "if ( clc.recordfile == FS_INVALID_HANDLE )", "record open failure guard")
assert_contains(record, "clc.recordName[0] = '\\0';", "record clears failed record name")
assert_order(record, "FS_FOpenFileWrite( name );", "clc.demorecording = qtrue;", "record only enables flag after file opens")
assert_contains(record, "clc.demowaiting = qtrue;", "record waits for first packet")
assert_contains(record, "clc.dm68compat = qtrue;", "record starts with legacy-compatible stream")
assert_order(record, "clc.dm68compat = qtrue;", "CL_WriteGamestate( qtrue );", "record compatibility set before initial gamestate")

read_demo = extract_function(demo_src, "CL_ReadDemoMessage")
assert_contains(read_demo, "if ( clc.demofile == FS_INVALID_HANDLE )", "demo read invalid handle guard")
assert_contains(read_demo, "r = FS_Read( &s, 4, clc.demofile );", "demo read sequence")
assert_contains(read_demo, "if ( r != 4 )", "demo read short-header guard")
assert_contains(read_demo, "clc.serverMessageSequence = LittleLong( s );", "demo sequence endian conversion")
assert_contains(read_demo, "MSG_Init( &buf, bufData, MAX_MSGLEN );", "demo read message init")
assert_contains(read_demo, "r = FS_Read( &buf.cursize, 4, clc.demofile );", "demo read payload length")
assert_contains(read_demo, "buf.cursize = LittleLong( buf.cursize );", "demo length endian conversion")
assert_contains(read_demo, "if ( buf.cursize == -1 )", "demo EOF sentinel handling")
assert_contains(read_demo, "if ( buf.cursize > buf.maxsize )", "demo max length guard")
assert_contains(read_demo, 'Com_Error( ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );', "demo oversized payload drop")
assert_contains(read_demo, "r = FS_Read( buf.data, buf.cursize, clc.demofile );", "demo payload read")
assert_contains(read_demo, "if ( r != buf.cursize )", "demo truncated payload guard")
assert_contains(read_demo, 'Com_Printf( "Demo file was truncated.\\n" );', "demo truncation diagnostic")
assert_order(read_demo, "if ( buf.cursize > buf.maxsize )", "r = FS_Read( buf.data, buf.cursize, clc.demofile );", "demo validates length before payload read")
assert_order(read_demo, "if ( r != buf.cursize )", "CL_ParseServerMessage( &buf );", "demo rejects truncation before parser")
assert_order(read_demo, "clc.demoCommandSequence = clc.serverCommandSequence;", "CL_ParseServerMessage( &buf );", "demo command sequence sync before parse")
assert_contains(read_demo, "if ( clc.demorecording )", "demo rerecord branch")
assert_contains(read_demo, "CL_WriteGamestate( qfalse );", "demo rerecord gamestate path")
assert_contains(read_demo, "CL_WriteSnapshot();", "demo rerecord snapshot path")

walk_demo = extract_function(demo_src, "CL_WalkDemoExt")
assert_contains(walk_demo, "*handle = FS_INVALID_HANDLE;", "demo extension walk initializes handle")
assert_contains(walk_demo, "while ( demo_protocols[i] )", "demo extension walk uses protocol table")
assert_contains(walk_demo, "Com_sprintf( name, name_len, \"demos/%s.%s%d\", arg, DEMOEXT, demo_protocols[i] );", "demo extension walk filename")
assert_order(walk_demo, "FS_BypassPure();", "FS_FOpenFileRead( name, handle, qtrue );", "demo extension walk bypasses pure before open")
assert_order(walk_demo, "FS_FOpenFileRead( name, handle, qtrue );", "FS_RestorePure();", "demo extension walk restores pure after open")
assert_contains(walk_demo, "return demo_protocols[i];", "demo extension walk returns matched protocol")

name_callback = extract_function(demo_src, "CL_DemoNameCallback_f")
assert_contains(name_callback, "if ( length <= ext_len + num_len", "demo completion length guard")
assert_contains(name_callback, "Q_stricmpn( filename + length - ( ext_len + num_len ), \".\" DEMOEXT", "demo completion extension guard")
assert_contains(name_callback, "version = atoi( filename + length - num_len );", "demo completion version parse")
assert_contains(name_callback, "if ( version == com_protocol->integer )", "demo completion accepts current protocol")
assert_contains(name_callback, "if ( version < 66 || version > NEW_PROTOCOL_VERSION )", "demo completion rejects outside supported protocol range")

play_demo = extract_function(demo_src, "CL_PlayDemo_f")
assert_contains(play_demo, "if ( Cmd_Argc() != 2 )", "playdemo usage guard")
assert_contains(play_demo, "ext_test = strrchr( arg, '.' );", "playdemo explicit extension detection")
assert_contains(play_demo, "protocol = atoi( ext_test + ARRAY_LEN( DEMOEXT ) );", "playdemo protocol parse")
assert_contains(play_demo, "if ( demo_protocols[i] || protocol == com_protocol->integer )", "playdemo accepts current custom protocol")
assert_order(play_demo, "FS_BypassPure();", "FS_FOpenFileRead( name, &hFile, qtrue );", "playdemo explicit file bypasses pure")
assert_order(play_demo, "FS_FOpenFileRead( name, &hFile, qtrue );", "FS_RestorePure();", "playdemo explicit file restores pure")
assert_contains(play_demo, "Com_Printf( \"Protocol %d not supported for demos\\n\", protocol );", "playdemo unsupported protocol diagnostic")
assert_contains(play_demo, "Q_strncpyz( retry, arg, len + 1 );", "playdemo bounded retry name copy")
assert_contains(play_demo, "protocol = CL_WalkDemoExt( retry, name, sizeof( name ), &hFile );", "playdemo fallback after unsupported explicit extension")
assert_contains(play_demo, "protocol = CL_WalkDemoExt( arg, name, sizeof( name ), &hFile );", "playdemo extensionless protocol fallback")
assert_contains(play_demo, "if ( hFile == FS_INVALID_HANDLE )", "playdemo missing file guard")
assert_order(play_demo, "FS_FCloseFile( hFile );", "Cvar_Set( \"sv_killserver\", \"2\" );", "playdemo closes probe before disconnect")
assert_order(play_demo, "Cvar_Set( \"sv_killserver\", \"2\" );", "CL_Disconnect( qtrue );", "playdemo kills server before disconnect")
assert_order(play_demo, "CL_Disconnect( qtrue );", "FS_FOpenFileRead( name, &clc.demofile, qtrue )", "playdemo opens real demofile after disconnect")
assert_contains(play_demo, "Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );", "playdemo stores short demo name")
assert_contains(play_demo, "cls.state = CA_CONNECTED;", "playdemo connected state")
assert_contains(play_demo, "clc.demoplaying = qtrue;", "playdemo flag set")
assert_contains(play_demo, "Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );", "playdemo server name")
assert_contains(play_demo, "if ( protocol <= OLD_PROTOCOL_VERSION )", "playdemo compatibility branch")
assert_order(play_demo, "if ( protocol <= OLD_PROTOCOL_VERSION )", "CL_ReadDemoMessage();", "playdemo compatibility set before priming")
assert_contains(play_demo, "clc.firstDemoFrameSkipped = qfalse;", "playdemo resets skipped-frame flag")

init_commands = extract_function(demo_src, "CL_Demo_InitCommands")
assert_contains(init_commands, 'Cmd_AddCommand( "record", CL_Record_f );', "demo command init adds record")
assert_contains(init_commands, 'Cmd_SetCommandCompletionFunc( "record", CL_CompleteRecordName );', "demo command init record completion")
assert_contains(init_commands, 'Cmd_AddCommand( "demo", CL_PlayDemo_f );', "demo command init adds demo")
assert_contains(init_commands, 'Cmd_SetCommandCompletionFunc( "demo", CL_CompleteDemoName );', "demo command init demo completion")
assert_contains(init_commands, 'Cmd_AddCommand( "stoprecord", CL_StopRecord_f );', "demo command init adds stoprecord")

shutdown_commands = extract_function(demo_src, "CL_Demo_ShutdownCommands")
assert_contains(shutdown_commands, 'Cmd_RemoveCommand( "record" );', "demo command shutdown removes record")
assert_contains(shutdown_commands, 'Cmd_RemoveCommand( "demo" );', "demo command shutdown removes demo")
assert_contains(shutdown_commands, 'Cmd_RemoveCommand( "stoprecord" );', "demo command shutdown removes stoprecord")

print("PASS: test_client_demo_regressions")
PY
