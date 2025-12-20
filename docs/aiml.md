# AIML 2.1 Interpreter

The engine now ships with a lightweight AIML 2.1 parser/interpreter written in C++23. It is available whenever `USE_AIML` is enabled (default).

## API surface

Header: `src/common/aiml/aiml_c_api.h`

- `AIML_LoadFile(path)`: load an AIML file from the virtual filesystem (PK3-friendly).
- `AIML_LoadBuffer(name, data)`: load AIML from an in-memory buffer.
- `AIML_Respond(userId, input, out, outSize)`: evaluate user input and write a reply.
- `AIML_SetBotPredicate` / `AIML_GetBotPredicate`: configure bot properties exposed via `<bot name="..."/>`.
- `AIML_ResetSession(userId)` / `AIML_ResetAllSessions()`: clear per-user state.

The interpreter is initialized in `Qcommon_Init` and torn down in `Com_Shutdown`, so modules can call into it without extra setup.

## Supported AIML features

- Categories with `pattern`, optional `that`, and optional `topic` (including `<topic>` blocks).
- Wildcards (`*` and `_`) with capture for `<star>`, `<thatstar>`, and `<topicstar>`.
- Template evaluation: `<random><li>...</li></random>`, `<condition>` (simple name/value forms), `<set>`, `<get>`, `<bot>`, `<srai>`, `<think>`, and case transforms (`<uppercase>`, `<lowercase>`, `<formal>`, `<sentence>`).
- Per-user predicates, bot properties, and topic tracking (`<set name="topic">`).
- Basic whitespace normalization and punctuation stripping for matching.

## Usage example

```c
char response[512];
if (AIML_LoadFile("scripts/chat.aiml") && AIML_Respond("player1", "Hello there", response, sizeof(response))) {
	Com_Printf("Bot: %s\n", response);
}
```

A minimal example file lives at `tests/aiml_sample.aiml` and can be loaded directly for smoke testing.

## Notes and limitations

- XML parsing is intentionally conservative; malformed AIML will be rejected with a log message.
- Recursion depth for `<srai>` is capped to avoid runaway loops.
- If no category matches an input, the interpreter returns an empty string.

