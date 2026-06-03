#!/usr/bin/env bash
# Regression checks for the client demo record/playback module extraction.
# These source-level invariants protect demo parser bounds, protocol fallback,
# file naming, and command lifecycle behavior without requiring game data.
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
demo_path = root / "src/client/cl_demo.c"
demo_header_path = root / "src/client/cl_demo.h"
cl_main_path = root / "src/client/cl_main.c"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def read_file(path: Path) -> str:
    if not path.is_file():
        fail(f"missing source file: {path}")
    return path.read_text()


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//.*", "", text)


def find_function_body(text: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*\(", text)
    if not match:
        fail(f"{name}: function not found")

    start = text.find("{", match.end())
    if start < 0:
        fail(f"{name}: opening brace not found")

    depth = 0
    state = "code"
    i = start
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""

        if state == "code":
            line_start = i == 0 or text[i - 1] == "\n"
            if line_start and (text.startswith("#else", i) or text.startswith("#elif", i)):
                nested = 0
                line_end = text.find("\n", i)
                pos = len(text) if line_end < 0 else line_end + 1
                while pos < len(text):
                    next_end = text.find("\n", pos)
                    if next_end < 0:
                        next_end = len(text)
                    directive = text[pos:next_end].strip()
                    if directive.startswith(("#if ", "#ifdef ", "#ifndef ")):
                        nested += 1
                    elif directive.startswith("#endif"):
                        if nested == 0:
                            i = next_end + 1
                            break
                        nested -= 1
                    pos = next_end + 1
                else:
                    i = len(text)
                continue
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


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected {needle!r}")


def assert_not_contains(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        fail(f"{context}: unexpected {needle!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_count(haystack: str, needle: str, expected: int, context: str) -> None:
    count = haystack.count(needle)
    if count != expected:
        fail(f"{context}: expected {expected} occurrences of {needle!r}, found {count}")


def assert_order(haystack: str, first: str, second: str, context: str) -> None:
    first_idx = haystack.find(first)
    second_idx = haystack.find(second)
    if first_idx < 0 or second_idx < 0 or first_idx >= second_idx:
        fail(f"{context}: expected {first!r} before {second!r}")


def assert_no_function_body(text: str, name: str, context: str) -> None:
    if re.search(r"\b" + re.escape(name) + r"\s*\([^;]*\)\s*\{", strip_comments(text), flags=re.S):
        fail(f"{context}: {name} should live in cl_demo.c only")


demo = read_file(demo_path)
demo_header = read_file(demo_header_path)
cl_main = read_file(cl_main_path)

# Extraction hooks: cl_main should wire the new module through the public header
# while private demo commands and helpers stay out of cl_main.
assert_contains(cl_main, '#include "cl_demo.h"', "client main includes demo module header")
assert_contains(cl_main, "CL_Demo_InitCommands();", "client init command wiring")
assert_contains(cl_main, "CL_Demo_ShutdownCommands();", "client shutdown command wiring")
assert_contains(cl_main, "CL_Demo_WriteServerPacket( msg, headerBytes );", "live packet recording hook")
for private_name in ("CL_Record_f", "CL_PlayDemo_f", "CL_WriteGamestate", "CL_WriteSnapshot"):
    assert_no_function_body(cl_main, private_name, "demo module extraction")

assert_contains(demo_header, "void CL_Demo_InitCommands( void );", "demo init command declaration")
assert_contains(demo_header, "void CL_Demo_ShutdownCommands( void );", "demo shutdown command declaration")
assert_contains(demo_header, "void CL_Demo_WriteServerPacket( msg_t *msg, int headerBytes );", "packet write declaration")

stop_body = find_function_body(demo, "CL_StopRecord_f")
assert_count(stop_body, "FS_Write( &len, 4, clc.recordfile );", 2, "demo terminator writes sequence and length sentinels")
assert_contains(stop_body, "FS_FCloseFile( clc.recordfile );", "record file close")
assert_contains(stop_body, "clc.recordfile = FS_INVALID_HANDLE;", "record file handle reset")
assert_contains(stop_body, "if ( clc.dm68compat || clc.demoplaying )", "old protocol stop-record selection")
assert_contains(stop_body, "protocol = NEW_PROTOCOL_VERSION;", "new protocol stop-record selection")
assert_contains(stop_body, "if ( com_protocol->integer != DEFAULT_PROTOCOL_VERSION )", "custom protocol override")
assert_contains(stop_body, 'Com_sprintf( tempName, sizeof( tempName ), "%s.tmp", clc.recordName );', "temp demo name")
assert_contains(stop_body, 'Com_sprintf( finalName, sizeof( finalName ), "%s.%s%d", clc.recordName, DEMOEXT, protocol );', "final demo name")
assert_contains(stop_body, "if ( clc.explicitRecordName )", "explicit demo overwrite branch")
assert_contains(stop_body, "FS_Remove( finalName );", "explicit demo removes existing final")
assert_contains(stop_body, "while ( FS_FileExists( finalName ) && ++sequence < 1000 )", "auto-name collision loop bound")
assert_contains(stop_body, 'Com_sprintf( finalName, sizeof( finalName ), "%s-%02d.%s%d"', "auto-name collision suffix")
assert_contains(stop_body, "FS_Rename( tempName, finalName );", "record temp rename")
assert_contains(stop_body, "clc.demorecording = qfalse;", "recording flag reset")
assert_contains(stop_body, "clc.spDemoRecording = qfalse;", "single-player record flag reset")
assert_order(stop_body, "FS_FCloseFile( clc.recordfile );", "FS_Rename( tempName, finalName );", "demo finalization closes before rename")

record_body = find_function_body(demo, "CL_Record_f")
assert_contains(record_body, "if ( Cmd_Argc() > 2 )", "record rejects too many args")
assert_contains(record_body, "if ( clc.demorecording )", "record double-start guard")
assert_contains(record_body, "if ( cls.state != CA_ACTIVE )", "record active-level guard")
assert_contains(record_body, "NET_IsLocalAddress( &clc.serverAddress )", "local server smooth-recording warning")
assert_contains(record_body, "Q_strncpyz( demoName, Cmd_Argv( 1 ), sizeof( demoName ) );", "explicit record name bounded copy")
assert_contains(record_body, "COM_GetExtension( demoName );", "record extension detection")
assert_contains(record_body, "DEMOEXT, OLD_PROTOCOL_VERSION", "record strips old demo extension")
assert_contains(record_body, "DEMOEXT, NEW_PROTOCOL_VERSION", "record strips new demo extension")
assert_contains(record_body, "*( strrchr( demoName, '.' ) ) = '\\0';", "record extension strip")
assert_contains(record_body, "clc.explicitRecordName = qtrue;", "explicit record name flag")
assert_contains(record_body, "clc.explicitRecordName = qfalse;", "generated record name flag")
assert_contains(record_body, 'Q_strcat( name, sizeof( name ), ".tmp" );', "record writes temp file first")
assert_contains(record_body, "FS_FOpenFileWrite( name );", "record temp open")
assert_contains(record_body, "clc.recordName[0] = '\\0';", "record open failure clears name")
assert_contains(record_body, "clc.demowaiting = qtrue;", "record waits for first packet")
assert_contains(record_body, "clc.dm68compat = qtrue;", "record starts dm68-compatible")
assert_contains(record_body, "CL_WriteGamestate( qtrue );", "record emits initial gamestate")

read_body = find_function_body(demo, "CL_ReadDemoMessage")
assert_contains(read_body, "if ( clc.demofile == FS_INVALID_HANDLE )", "read handles absent demo file")
assert_count(read_body, "CL_DemoCompleted();", 5, "read completion paths")
assert_count(read_body, "if ( r != 4 )", 2, "read sequence and length short-read guards")
assert_contains(read_body, "clc.serverMessageSequence = LittleLong( s );", "demo sequence endian conversion")
assert_contains(read_body, "buf.cursize = LittleLong( buf.cursize );", "demo message length endian conversion")
assert_contains(read_body, "if ( buf.cursize == -1 )", "demo EOF sentinel")
assert_contains(read_body, "if ( buf.cursize > buf.maxsize )", "demo oversize guard")
assert_contains(read_body, 'Com_Error( ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );', "demo oversize drop")
assert_contains(read_body, "r = FS_Read( buf.data, buf.cursize, clc.demofile );", "demo payload read")
assert_contains(read_body, "if ( r != buf.cursize )", "demo truncated payload guard")
assert_contains(read_body, 'Com_Printf( "Demo file was truncated.\\n" );', "demo truncated payload warning")
assert_contains(read_body, "clc.lastPacketTime = cls.realtime;", "demo packet time update")
assert_contains(read_body, "buf.readcount = 0;", "demo parser readcount reset")
assert_contains(read_body, "clc.demoCommandSequence = clc.serverCommandSequence;", "demo command sequence reset before parse")
assert_contains(read_body, "CL_ParseServerMessage( &buf );", "demo server message parse")
assert_contains(read_body, "if ( clc.demorecording )", "demo re-recording gate")
assert_contains(read_body, "if ( clc.eventMask & EM_GAMESTATE )", "demo re-record gamestate event")
assert_contains(read_body, "else if ( clc.eventMask & ( EM_SNAPSHOT | EM_COMMAND ) )", "demo re-record snapshot/command event")
assert_order(read_body, "if ( buf.cursize > buf.maxsize )", "r = FS_Read( buf.data, buf.cursize, clc.demofile );", "demo validates length before payload read")
assert_order(read_body, "CL_ParseServerMessage( &buf );", "if ( clc.demorecording )", "demo re-recording after parse")

walk_body = find_function_body(demo, "CL_WalkDemoExt")
assert_contains(walk_body, "*handle = FS_INVALID_HANDLE;", "walk initializes output handle")
assert_contains(walk_body, "while ( demo_protocols[i] )", "walk honors protocol sentinel")
assert_contains(walk_body, 'Com_sprintf( name, name_len, "demos/%s.%s%d", arg, DEMOEXT, demo_protocols[i] );', "walk builds protocol-specific demo path")
assert_contains(walk_body, "FS_BypassPure();", "walk bypasses pure checks")
assert_contains(walk_body, "FS_FOpenFileRead( name, handle, qtrue );", "walk opens candidate demo")
assert_contains(walk_body, "FS_RestorePure();", "walk restores pure checks")
assert_contains(walk_body, "return demo_protocols[i];", "walk returns matching protocol")
assert_contains(walk_body, "return -1;", "walk reports no matching protocol")
assert_order(walk_body, "FS_BypassPure();", "FS_FOpenFileRead( name, handle, qtrue );", "walk bypasses pure before open")
assert_order(walk_body, "FS_FOpenFileRead( name, handle, qtrue );", "FS_RestorePure();", "walk restores pure after open")

callback_body = find_function_body(demo, "CL_DemoNameCallback_f")
assert_contains(callback_body, 'const int ext_len = (int)strlen( "." DEMOEXT );', "completion extension length")
assert_contains(callback_body, "const int num_len = 2;", "completion protocol digit length")
assert_contains(callback_body, "length <= ext_len + num_len", "completion short-name rejection")
assert_contains(callback_body, 'Q_stricmpn( filename + length - ( ext_len + num_len ), "." DEMOEXT, (size_t)ext_len ) != 0', "completion extension rejection")
assert_contains(callback_body, "if ( version == com_protocol->integer )", "completion current protocol acceptance")
assert_contains(callback_body, "if ( version < 66 || version > NEW_PROTOCOL_VERSION )", "completion protocol range rejection")
assert_contains(callback_body, "return qtrue;", "completion accepts supported protocol")

play_body = find_function_body(demo, "CL_PlayDemo_f")
assert_contains(play_body, "if ( Cmd_Argc() != 2 )", "playdemo rejects wrong arg count")
assert_contains(play_body, "arg = Cmd_Argv( 1 );", "playdemo reads argument")
assert_contains(play_body, "ext_test = strrchr( arg, '.' );", "playdemo extension detection")
assert_contains(play_body, "protocol = atoi( ext_test + ARRAY_LEN( DEMOEXT ) );", "playdemo protocol parse")
assert_contains(play_body, "for ( i = 0; demo_protocols[i]; i++ )", "playdemo supported protocol scan")
assert_contains(play_body, "if ( demo_protocols[i] || protocol == com_protocol->integer )", "playdemo custom protocol acceptance")
assert_contains(play_body, 'Com_sprintf( name, sizeof( name ), "demos/%s", arg );', "playdemo explicit path")
assert_contains(play_body, "FS_BypassPure();", "playdemo bypass pure for explicit open")
assert_contains(play_body, "FS_FOpenFileRead( name, &hFile, qtrue );", "playdemo explicit open")
assert_contains(play_body, "FS_RestorePure();", "playdemo restores pure for explicit open")
assert_contains(play_body, 'Com_Printf( "Protocol %d not supported for demos\\n", protocol );', "playdemo unsupported protocol warning")
assert_contains(play_body, "len = (size_t)( ext_test - arg );", "playdemo strips unsupported extension")
assert_contains(play_body, "if ( len > ARRAY_LEN( retry ) - 1 )", "playdemo retry length clamp")
assert_contains(play_body, "Q_strncpyz( retry, arg, len + 1 );", "playdemo retry bounded copy")
assert_contains(play_body, "retry[len] = '\\0';", "playdemo retry explicit terminator")
assert_contains(play_body, "protocol = CL_WalkDemoExt( retry, name, sizeof( name ), &hFile );", "playdemo unsupported extension fallback")
assert_contains(play_body, "protocol = CL_WalkDemoExt( arg, name, sizeof( name ), &hFile );", "playdemo implicit extension walk")
assert_contains(play_body, "if ( hFile == FS_INVALID_HANDLE )", "playdemo open failure guard")
assert_contains(play_body, "FS_FCloseFile( hFile );", "playdemo closes probe handle")
assert_contains(play_body, 'Cvar_Set( "sv_killserver", "2" );', "playdemo shuts down local server")
assert_contains(play_body, "CL_Disconnect( qtrue );", "playdemo disconnects before playback")
assert_contains(play_body, "FS_FOpenFileRead( name, &clc.demofile, qtrue )", "playdemo opens playback handle")
assert_contains(play_body, "shortname = slash + 1;", "playdemo basename extraction")
assert_contains(play_body, "Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );", "playdemo demoName bounded copy")
assert_contains(play_body, "Con_Close();", "playdemo closes console")
assert_contains(play_body, "cls.state = CA_CONNECTED;", "playdemo connected state")
assert_contains(play_body, "clc.demoplaying = qtrue;", "playdemo playback flag")
assert_contains(play_body, "Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );", "playdemo servername bounded copy")
assert_contains(play_body, "if ( protocol <= OLD_PROTOCOL_VERSION )", "playdemo legacy compat gate")
assert_contains(play_body, "clc.compat = qtrue;", "playdemo legacy compat enable")
assert_contains(play_body, "clc.compat = qfalse;", "playdemo legacy compat disable")
assert_contains(play_body, "while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED", "playdemo priming loop")
assert_contains(play_body, "CL_ReadDemoMessage();", "playdemo reads messages until primed")
assert_contains(play_body, "clc.firstDemoFrameSkipped = qfalse;", "playdemo resets first-frame skip flag")
assert_order(play_body, 'Cvar_Set( "sv_killserver", "2" );', "CL_Disconnect( qtrue );", "playdemo kills server before disconnect")
assert_order(play_body, "CL_Disconnect( qtrue );", "FS_FOpenFileRead( name, &clc.demofile, qtrue )", "playdemo reopens after disconnect")
assert_order(play_body, "cls.state = CA_CONNECTED;", "clc.demoplaying = qtrue;", "playdemo sets state before playback flag")
assert_order(play_body, "if ( protocol <= OLD_PROTOCOL_VERSION )", "while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED", "playdemo compat set before priming")

init_body = find_function_body(demo, "CL_Demo_InitCommands")
shutdown_body = find_function_body(demo, "CL_Demo_ShutdownCommands")
for command, handler in (("record", "CL_Record_f"), ("demo", "CL_PlayDemo_f"), ("stoprecord", "CL_StopRecord_f")):
    assert_contains(init_body, f'Cmd_AddCommand( "{command}", {handler} );', f"{command} command registration")
    assert_contains(shutdown_body, f'Cmd_RemoveCommand( "{command}" );', f"{command} command removal")
assert_contains(init_body, 'Cmd_SetCommandCompletionFunc( "record", CL_CompleteRecordName );', "record completion registration")
assert_contains(init_body, 'Cmd_SetCommandCompletionFunc( "demo", CL_CompleteDemoName );', "demo completion registration")

print("PASS: test_client_demo_regressions")
PY
