#!/usr/bin/env bash
# Regression guard: keep BotLoadChatMessage bounded string APIs.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BOTLIB_C="${1:-$PROJECT_ROOT/src/platform/botlib/be_ai_chat.c}"
WIN32_BOTLIB_C="${2:-$PROJECT_ROOT/src/platform/win32/botlib/be_ai_chat.c}"

fail() {
	echo "FAIL: $*" >&2
	exit 1
}

extract_function() {
	local file="$1"
	awk '
		/static int BotLoadChatMessage\( source_t \*source, char \*chatmessagestring, int size \)/ { in_fn=1 }
		in_fn { print }
		in_fn && /\} \/\/end of the function BotLoadChatMessage/ { exit }
	' "$file"
}

assert_contains() {
	local haystack="$1"
	local needle="$2"
	local context="$3"
	if [[ "$haystack" != *"$needle"* ]]; then
		fail "$context: expected '$needle'"
	fi
}

assert_no_unbounded_calls() {
	local content="$1"
	local context="$2"
	local sanitized
	sanitized="$(printf '%s\n' "$content" | sed 's://.*$::')"
	if printf '%s\n' "$sanitized" | rg -n '\bstrcpy\s*\(' >/dev/null; then
		fail "$context: found unbounded strcpy call"
	fi
	if printf '%s\n' "$sanitized" | rg -n '(^|[^A-Za-z0-9_])sprintf\s*\(' >/dev/null; then
		fail "$context: found unbounded sprintf call"
	fi
}

check_file() {
	local file="$1"
	[ -f "$file" ] || fail "missing source file: $file"

	local body
	body="$(extract_function "$file")"
	[ -n "$body" ] || fail "could not extract BotLoadChatMessage from $file"

	assert_contains "$body" "Q_strncpyz( &ptr[curlen], token.string, size - curlen );" "$file fixed-string copy"
	assert_contains "$body" "intlen = Com_sprintf( intbuf, sizeof( intbuf ), \"%cv%ld%c\", ESCAPE_CHAR, (long)token.intvalue, ESCAPE_CHAR );" "$file integer formatting"
	assert_contains "$body" "Q_strncpyz( &ptr[len], intbuf, size - len );" "$file integer copy"
	assert_contains "$body" "Com_sprintf( &ptr[len], size - len, \"%cr%s%c\", ESCAPE_CHAR, token.string, ESCAPE_CHAR );" "$file random-string formatting"
	assert_no_unbounded_calls "$body" "$file BotLoadChatMessage body"
}

check_file "$BOTLIB_C"
check_file "$WIN32_BOTLIB_C"

echo "PASS: test_botlib_chat_message_bounds"
