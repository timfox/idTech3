#!/usr/bin/env bash
# Regression checks for botlib bounded string formatting/copying fixes.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

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

botlib_files = [
    root / "src/platform/botlib/be_ai_chat.c",
    root / "src/platform/win32/botlib/be_ai_chat.c",
]
precomp_files = [
    root / "src/platform/botlib/l_precomp.c",
    root / "src/platform/win32/botlib/l_precomp.c",
]


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    sys.exit(1)


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


def assert_contains(haystack: str, needle: str, context: str) -> None:
    if needle not in haystack:
        fail(f"{context}: expected {needle!r}")


def assert_regex(haystack: str, pattern: str, context: str) -> None:
    if not re.search(pattern, haystack, flags=re.S):
        fail(f"{context}: expected pattern {pattern!r}")


def assert_no_bare_sprintf(haystack: str, context: str) -> None:
    uncommented = strip_comments(haystack)
    if re.search(r"(?<![A-Za-z0-9_])(?:v?sprintf)\s*\(", uncommented):
        fail(f"{context}: found unbounded sprintf/vsprintf call")


for path in botlib_files:
    if not path.is_file():
        fail(f"missing source file: {path}")
    body = find_function_body(path.read_text(), "BotLoadChatMessage")
    context = str(path.relative_to(root)) + ":BotLoadChatMessage"

    assert_contains(body, "Q_strncpyz( &ptr[curlen], token.string, size - curlen );", context)
    assert_contains(body, "Com_sprintf( intbuf, sizeof( intbuf ), \"%cv%ld%c\"", context)
    assert_contains(body, "Q_strncpyz( &ptr[len], intbuf, size - len );", context)
    assert_contains(body, "Com_sprintf( &ptr[len], size - len, \"%cr%s%c\"", context)
    assert_regex(body, r"curlen\s*\+\s*strlen\s*\(\s*token\.string\s*\)\s*\+\s*1\s*>\s*\(size_t\)\s*size", context)
    assert_regex(body, r"len\s*\+\s*intlen\s*\+\s*1\s*>\s*size", context)
    assert_regex(body, r"\(size_t\)\s*len\s*\+\s*strlen\s*\(\s*token\.string\s*\)\s*\+\s*4\s*>\s*\(size_t\)\s*size", context)
    assert_no_bare_sprintf(body, context)

for path in precomp_files:
    if not path.is_file():
        fail(f"missing source file: {path}")
    text = path.read_text()

    for function in ("SourceError", "SourceWarning"):
        body = find_function_body(text, function)
        context = str(path.relative_to(root)) + f":{function}"
        assert_contains(body, "Q_vsnprintf(text, sizeof(text), fmt, ap);", context)
        assert_no_bare_sprintf(body, context)

    for function in (
        "PC_Directive_eval",
        "PC_Directive_evalfloat",
        "PC_DollarDirective_evalint",
        "PC_DollarDirective_evalfloat",
    ):
        body = find_function_body(text, function)
        context = str(path.relative_to(root)) + f":{function}"
        assert_contains(body, "Com_sprintf(token.string, MAX_TOKEN,", context)
        assert_no_bare_sprintf(body, context)

print("PASS: test_botlib_bounded_strings")
PY
