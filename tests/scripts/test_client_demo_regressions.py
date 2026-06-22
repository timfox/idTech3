#!/usr/bin/env python3
"""Regression guards for client demo record/playback extraction.

These checks protect behavior moved from cl_main.c into cl_demo.c without
requiring a renderer, game data, or real .dm_* files.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected to find {needle!r}")


def assert_not_contains(haystack: str, needle: str, context: str) -> None:
    if needle in haystack:
        fail(f"{context}: did not expect to find {needle!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, re.MULTILINE | re.DOTALL):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_order(haystack: str, context: str, *needles: str) -> None:
    cursor = -1
    for needle in needles:
        pos = haystack.find(needle, cursor + 1)
        if pos < 0:
            fail(f"{context}: missing {needle!r}")
        if pos <= cursor:
            fail(f"{context}: {needle!r} appears out of order")
        cursor = pos


def strip_alternate_preprocessor_branches(source: str) -> str:
    """Keep first visible branch for brace-count extraction.

    CL_PlayDemo_f contains an #ifdef USE_CURL / #else pair where both branches
    open the same while loop and share one closing brace. Counting both source
    branches would make a textual function extractor think the function never
    closes, so keep the first branch and skip #else/#elif bodies.
    """

    output: list[str] = []
    skip_stack: list[bool] = []

    for line in source.splitlines():
        stripped = line.strip()
        in_skipped_branch = any(skip_stack)

        if stripped.startswith(("#if", "#ifdef", "#ifndef")):
            if in_skipped_branch:
                skip_stack.append(True)
            else:
                output.append(line)
                skip_stack.append(False)
            continue

        if stripped.startswith(("#else", "#elif")) and skip_stack:
            if len(skip_stack) == 1 or not any(skip_stack[:-1]):
                output.append(line)
            skip_stack[-1] = True
            continue

        if stripped.startswith("#endif") and skip_stack:
            was_skipping = skip_stack.pop()
            if not was_skipping and not any(skip_stack):
                output.append(line)
            continue

        if not any(skip_stack):
            output.append(line)

    return "\n".join(output) + "\n"


def extract_function(source: str, name: str) -> str:
    prepared = strip_alternate_preprocessor_branches(source)
    lines = prepared.splitlines()
    start = None
    signature_re = re.compile(rf"\b{name}\s*\(")

    for index, line in enumerate(lines):
        if signature_re.search(line):
            start = index
            break

    if start is None:
        fail(f"could not find function {name}")

    body: list[str] = []
    opens = 0
    closes = 0
    seen_open = False

    for line in lines[start:]:
        body.append(line)
        opens += line.count("{")
        closes += line.count("}")
        seen_open = seen_open or "{" in line
        if seen_open and opens == closes:
            return "\n".join(body)

    fail(f"could not extract complete function {name}")
    return ""


def main() -> None:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2]
    cl_demo_path = root / "src/client/cl_demo.c"
    cl_demo_header_path = root / "src/client/cl_demo.h"
    cl_main_path = root / "src/client/cl_main.c"

    for path in (cl_demo_path, cl_demo_header_path, cl_main_path):
        if not path.is_file():
            fail(f"missing expected source file: {path}")

    cl_demo = cl_demo_path.read_text()
    cl_demo_header = cl_demo_header_path.read_text()
    cl_main = cl_main_path.read_text()

    assert_contains(cl_main, '#include "cl_demo.h"', "cl_main includes demo module API")
    assert_contains(cl_main, "CL_Demo_WriteServerPacket( msg, headerBytes );", "packet event delegates demo writes")
    assert_contains(cl_main, "CL_Demo_InitCommands();", "client init registers demo commands")
    assert_contains(cl_main, "CL_Demo_ShutdownCommands();", "client shutdown unregisters demo commands")

    for private_name in ("CL_Record_f", "CL_PlayDemo_f", "CL_WriteGamestate", "CL_WriteSnapshot"):
        assert_not_contains(cl_main, private_name + "(", f"{private_name} stays private to cl_demo.c")

    assert_contains(cl_demo_header, "void CL_Demo_InitCommands( void );", "demo command init exported")
    assert_contains(cl_demo_header, "void CL_Demo_ShutdownCommands( void );", "demo command shutdown exported")
    assert_contains(cl_demo_header, "void CL_Demo_WriteServerPacket( msg_t *msg, int headerBytes );", "demo packet writer exported")

    stop_record = extract_function(cl_demo, "CL_StopRecord_f")
    assert_contains(stop_record, "len = -1;", "stoprecord writes demo EOF sentinel")
    assert_regex(
        stop_record,
        r"FS_Write\s*\(\s*&len,\s*4,\s*clc\.recordfile\s*\)\s*;\s*"
        r"FS_Write\s*\(\s*&len,\s*4,\s*clc\.recordfile\s*\)",
        "stoprecord writes both sequence and length EOF sentinels",
    )
    assert_order(
        stop_record,
        "stoprecord closes and invalidates handle before rename",
        "FS_FCloseFile( clc.recordfile );",
        "clc.recordfile = FS_INVALID_HANDLE;",
        'Com_sprintf( tempName, sizeof( tempName ), "%s.tmp", clc.recordName );',
        "FS_Rename( tempName, finalName );",
    )
    assert_regex(
        stop_record,
        r"if\s*\(\s*clc\.explicitRecordName\s*\)\s*\{\s*FS_Remove\s*\(\s*finalName\s*\)\s*;\s*\}\s*else\s*\{"
        r".*while\s*\(\s*FS_FileExists\s*\(\s*finalName\s*\)\s*&&\s*\+\+sequence\s*<\s*1000\s*\)",
        "explicit demo names overwrite while automatic names avoid collisions",
    )

    record = extract_function(cl_demo, "CL_Record_f")
    assert_order(
        record,
        "record guard order",
        "if ( Cmd_Argc() > 2 )",
        "if ( clc.demorecording )",
        "if ( cls.state != CA_ACTIVE )",
    )
    assert_contains(record, "clc.explicitRecordName = qtrue;", "record explicit filename flag")
    assert_contains(record, "clc.explicitRecordName = qfalse;", "record autogenerated filename flag")
    assert_contains(record, "FS_FOpenFileWrite( name );", "record opens temporary demo output")
    assert_order(
        record,
        "failed record open clears stale record name before returning",
        "if ( clc.recordfile == FS_INVALID_HANDLE )",
        "clc.recordName[0] = '\\0';",
        "return;",
    )
    assert_order(
        record,
        "record initializes state before first gamestate write",
        "clc.demorecording = qtrue;",
        "clc.demowaiting = qtrue;",
        "clc.dm68compat = qtrue;",
        "CL_WriteGamestate( qtrue );",
    )

    read_demo = extract_function(cl_demo, "CL_ReadDemoMessage")
    assert_order(
        read_demo,
        "demo message parser validates headers before payload read",
        "r = FS_Read( &s, 4, clc.demofile );",
        "if ( r != 4 )",
        "clc.serverMessageSequence = LittleLong( s );",
        "r = FS_Read( &buf.cursize, 4, clc.demofile );",
        "buf.cursize = LittleLong( buf.cursize );",
        "if ( buf.cursize == -1 )",
        "if ( buf.cursize > buf.maxsize )",
        "r = FS_Read( buf.data, buf.cursize, clc.demofile );",
        "if ( r != buf.cursize )",
        "CL_ParseServerMessage( &buf );",
    )
    assert_contains(read_demo, 'Com_Error( ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN" );', "oversized demo payload is fatal")
    assert_contains(read_demo, 'Com_Printf( "Demo file was truncated.\\n" );', "truncated demo payload is diagnosed")
    assert_order(
        read_demo,
        "recording during playback writes only after parsing events",
        "CL_ParseServerMessage( &buf );",
        "if ( clc.demorecording )",
        "if ( clc.eventMask & EM_GAMESTATE )",
        "CL_WriteGamestate( qfalse );",
        "else if ( clc.eventMask & ( EM_SNAPSHOT | EM_COMMAND ) )",
        "CL_WriteSnapshot();",
    )

    walk_demo = extract_function(cl_demo, "CL_WalkDemoExt")
    assert_contains(walk_demo, "while ( demo_protocols[i] )", "demo extension fallback iterates known protocols")
    assert_order(
        walk_demo,
        "pure-server state is bypassed only around demo file lookup",
        "Com_sprintf( name, name_len, \"demos/%s.%s%d\", arg, DEMOEXT, demo_protocols[i] );",
        "FS_BypassPure();",
        "FS_FOpenFileRead( name, handle, qtrue );",
        "FS_RestorePure();",
        "if ( *handle != FS_INVALID_HANDLE )",
    )

    complete_name = extract_function(cl_demo, "CL_DemoNameCallback_f")
    assert_contains(complete_name, "version == com_protocol->integer", "completion accepts current protocol")
    assert_contains(complete_name, "version < 66 || version > NEW_PROTOCOL_VERSION", "completion rejects unsupported protocol range")

    play_demo = extract_function(cl_demo, "CL_PlayDemo_f")
    assert_order(
        play_demo,
        "explicit demo extension validates protocol before opening",
        "protocol = atoi( ext_test + ARRAY_LEN( DEMOEXT ) );",
        "for ( i = 0; demo_protocols[i]; i++ )",
        "if ( demo_protocols[i] || protocol == com_protocol->integer )",
        "FS_BypassPure();",
        "FS_FOpenFileRead( name, &hFile, qtrue );",
        "FS_RestorePure();",
    )
    assert_order(
        play_demo,
        "unsupported explicit protocol falls back to basename walk safely",
        'Com_Printf( "Protocol %d not supported for demos\\n", protocol );',
        "len = (size_t)( ext_test - arg );",
        "if ( len > ARRAY_LEN( retry ) - 1 )",
        "Q_strncpyz( retry, arg, len + 1 );",
        "retry[len] = '\\0';",
        "protocol = CL_WalkDemoExt( retry, name, sizeof( name ), &hFile );",
    )
    assert_order(
        play_demo,
        "playback closes probe before disconnect and real reopen",
        "FS_FCloseFile( hFile );",
        "hFile = FS_INVALID_HANDLE;",
        'Cvar_Set( "sv_killserver", "2" );',
        "CL_Disconnect( qtrue );",
        "FS_FOpenFileRead( name, &clc.demofile, qtrue )",
    )
    assert_order(
        play_demo,
        "playback state is initialized before priming reads",
        "Q_strncpyz( clc.demoName, shortname, sizeof( clc.demoName ) );",
        "cls.state = CA_CONNECTED;",
        "clc.demoplaying = qtrue;",
        "Q_strncpyz( cls.servername, shortname, sizeof( cls.servername ) );",
        "if ( protocol <= OLD_PROTOCOL_VERSION )",
        "while ( cls.state >= CA_CONNECTED && cls.state < CA_PRIMED",
        "CL_ReadDemoMessage();",
        "clc.firstDemoFrameSkipped = qfalse;",
    )

    init_commands = extract_function(cl_demo, "CL_Demo_InitCommands")
    for command in ("record", "demo", "stoprecord"):
        assert_contains(init_commands, f'Cmd_AddCommand( "{command}"', f"{command} command registered")
    assert_contains(init_commands, 'Cmd_SetCommandCompletionFunc( "record", CL_CompleteRecordName );', "record completion registered")
    assert_contains(init_commands, 'Cmd_SetCommandCompletionFunc( "demo", CL_CompleteDemoName );', "demo completion registered")

    shutdown_commands = extract_function(cl_demo, "CL_Demo_ShutdownCommands")
    for command in ("record", "demo", "stoprecord"):
        assert_contains(shutdown_commands, f'Cmd_RemoveCommand( "{command}" );', f"{command} command removed")

    print("PASS: test_client_demo_regressions")


if __name__ == "__main__":
    main()
