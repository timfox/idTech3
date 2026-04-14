# Codebase Quality Audit — Phase 2

**Date**: 2026-02-28  
**Scope**: String safety, unbounded functions, memory, and remaining project code (excluding `external/`)

---

## 1. String Safety — Remaining `strcpy` / `strcat`

### High priority (user/network-controlled input)

| Location | Issue | Risk |
|----------|-------|------|
| `cl_curl.c:898` | `strcpy(dl->Name, name)` | `name` from HTTP `Content-Disposition` header; malicious server can overflow `dl->Name[MAX_OSPATH]` |
| `snd_dma.c:283` | `strcpy(sfx->soundName, name)` | `name` has length check at 246–249 in same file, but only in `S_RegisterSound_`; `S_FindName` uses it without prior check. Defensive: use `Q_strncpyz` |

### Medium priority (parsed/trusted but unvalidated)

| Location | Issue | Notes |
|----------|-------|-------|
| `tr_bsp.c:2361` (Vulkan) | `strcpy(w->entityString, p)` | `p` from BSP lump; allocation is `l->filelen+1`. Assumes null terminator in lump; `memcpy` + explicit null safer |
| `tr_bsp.c:2496,2499` (Vulkan) | `strcpy(spawnVarChars + ..., keyname/com_token)` | Bounds checked before copy; safe given check, but `Q_strncpyz` would be more defensive |
| `tr_bsp.c:2045` (Vulkan), `tr_bsp.c:1817` (OpenGL) | `strcpy(out[72].shader, "textures/...")` | Fixed string; low risk |
| `sv_client.c:337` | `strcpy(str, "**")` | Fixed 2-char string; safe |
| `net_ip.c:596,598,619,621` | `strcpy(s, "loopback")` / `strcpy(s, "bot")` | Fixed strings; safe |
| `sv_filter.c:503` | `strcpy(node->p2.string, p2)` | `p2` from filter logic; verify `p2.string` size vs input |
| `sv_ccmds.c:1227,1235` | `strcpy(nc, cl->name)` / `strcpy(ac, s)` | Need buffer sizes; `cl->name` is bounded |
| `sv_ccmds.c:1338–1339, 1384–1385` | `strcpy` + `strcat` for console text | `text[MAX_STRING_CHARS]`, `p` truncated to 1000; safe but `Com_sprintf` clearer |
| `files.c` (multiple) | `strcpy` for pak names, paths | Most from internal paths; verify each destination size |
| `common.c:1805,2731` | `strcpy(out, in)` | Need context for buffer sizes |
| `q_shared.c:2164` | `strcpy(s+len1, newi)` | Inside `Info_SetValueForKey`; bounds depend on `maxSize` |

### Lower priority (OpenGL ARB/VBO program strings)

| Location | Issue | Notes |
|----------|-------|-------|
| `tr_arb.c`, `tr_vbo.c` | `strcat(program, ...)` / `strcpy(buf, ...)` | Building shader strings; buffer sizes should be validated |

---

## 2. Unbounded `sprintf`

### Project code (non-external)

| Location | Issue | Status |
|----------|-------|--------|
| `cl_cgame.c:735,737` | `sprintf` in VM traps | ✅ Fixed: `Com_sprintf` with `MAX_STRING_CHARS` |
| `sv_game.c:964,967` | `sprintf` in VM traps | ✅ Fixed: `Com_sprintf` with `MAX_STRING_CHARS` |
| `be_ai_chat.c:951,969` | `sprintf` for chat tokens | ✅ Fixed: `Com_sprintf` / `Q_strncpyz` with bounds |
| `platform/win32/botlib/be_ai_chat.c` (same paths) | Duplicate tree still used `sprintf` / `strcpy` in `BotLoadChatMessage` | ✅ Fixed (2026-04-11): match `src/botlib/be_ai_chat.c` |
| `platform/botlib/be_ai_chat.c` | `strcpy` for fixed string append in `BotLoadChatMessage` | ✅ Fixed (2026-04-11): `Q_strncpyz` |
| `l_precomp.c` (5 places) | `sprintf(token.string, ...)` | ✅ Fixed: `Com_sprintf` with `MAX_TOKEN` |
| `platform/botlib/l_precomp.c`, `platform/win32/botlib/l_precomp.c` (5 places each) | Duplicate tree still used `sprintf` on token strings | ✅ Fixed (2026-04-11): `Com_sprintf(..., MAX_TOKEN, ...)` to match `src/botlib/l_precomp.c` |
| `bindshader.c:26`, `bin2hex.c` | Build tools | ✅ Fixed: `snprintf` with `sizeof(buf)` |
| `tr_arb.c`, `tr_vbo.c` | Already use `Com_sprintf` / `Q_strcat` | ✅ No change needed |
| Vulkan `vk_*.c` tree, `sv_client.c`, `vm.c`, `cl_curl.c` | First-party `sprintf` largely migrated to `Com_sprintf`; re-scan with `rg` on `src/` excluding `external/` | Triage new hits |

---

## 3. `strcat` Without Bounds

| Location | Issue |
|----------|-------|
| `tr_arb.c` (many) | `strcat(program, ...)` — program buffer size unclear |
| `tr_vbo.c` (many) | `strcat(buf, ...)` — same concern |
| `unix_main.c:1137–1138` | `strcat(cmdline, " ")` / `strcat(cmdline, argv[i])` — `len` precomputed; safe |
| `common.c:3272` | `strcat(&cl_cdkey[16], buffer)` — CD key handling; verify bounds |
| `common.c:3468–3580` | `strcat(vendor, ...)` — CPU vendor string; fixed suffixes |
| `cl_cgame.c:305,314–315` | `strcat(bigConfigString, ...)` — length checked before; safe |
| `win_syscon.c:650` | `strcat(s_wcd.consoleText, "\n")` — verify buffer size |

---

## 4. TODO / FIXME / XXX

**First-party engine tree** (`src/` excluding `external/`): triage doc `docs/TODO_TRIAGE.md` tracks actionable items; a **2026-04-10** `rg 'TODO|FIXME' src --glob '!**/external/**'` pass reported **no literal `TODO`/`FIXME`** in client/game/qcommon/renderers/server/botlib/navigation/physics/platform/audio.

**Third-party** (`src/external/`): many vendor `TODO`/`FIXME` strings remain; treat as upstream scope unless we vendor-patch.

Historical note: older “150+ TODO” counts mixed external trees; use the triage doc + scoped `rg` for current truth.

---

## 5. Memory and Allocation

- **Mikktspace** (`mikktspace.c`): Uses `malloc`/`free` with null checks; pattern looks correct.
- **Common**: `calloc` for zone/hunk with null checks.
- **Files**: `malloc` in `files.c:685` with null check.
- **Unix**: `malloc` for `cmdline` in `unix_main.c`; length computed to fit `argv`.
- **snd_dma.c**: `malloc` for `dma_buffer2`; no obvious leak.

No critical leaks identified in project code; external libs not audited in depth.

---

## 6. Logic and Correctness

- **Entity string** (`tr_bsp.c`): `strcpy(w->entityString, p)` assumes BSP entity lump is null-terminated. If not, reads past lump. Safer: `memcpy` + explicit null.
- **Spawn vars** (`tr_bsp.c`): Bounds check before `strcpy`; logic is sound.
- **Cubemap origin/radius** (`tr_bsp.c`): `sscanf` return checks added in Phase 1.

---

## 7. Suggested Priorities

### P0 — Security / robustness ✅ DONE

1. **cl_curl.c:898** — `strcpy(dl->Name, name)` → `Q_strncpyz(dl->Name, name, sizeof(dl->Name))` (Content-Disposition is untrusted).
2. **snd_dma.c:283** — `strcpy(sfx->soundName, name)` → `Q_strncpyz(sfx->soundName, name, sizeof(sfx->soundName))` (defense in depth).

### P1 — Consistency and safety ✅ DONE

3. **sv_client.c:403,405** — `sprintf` → `Com_sprintf` with buffer size.
4. **Legacy `vk.c`** — `sprintf` → `Com_sprintf` (addressed when Vulkan was split; no `vk.c` today).
5. **tr_bsp.c:2361** (Vulkan + OpenGL) — `strcpy` → `Com_Memcpy` + explicit null (BSP lump may lack terminator).
6. **cl_curl.c:764,766** — `sprintf(dl->progress, ...)` → `Com_sprintf`.

### P2 — Cleanup

6. Replace remaining `sprintf` in project code with `Com_sprintf`.
7. Replace `strcat` in `tr_arb.c` / `tr_vbo.c` with `Q_strcat` or equivalent bounded append.
8. Triage TODO/FIXME comments and resolve or document.

---

## 8. Summary

Phase 1 addressed model loaders, image creation, TLD, fog/color parsing, and several `sprintf` usages. Phase 2 highlights:

- **2 high-priority** string safety fixes (curl download name, sound name).
- **~15 medium-priority** `strcpy`/`sprintf`/`strcat` replacements for consistency.
- **TODO/FIXME**: first-party tree triaged in `docs/TODO_TRIAGE.md` (no literal tags in engine dirs as of 2026-04-10 scan); external/vendor code excluded.
- No critical memory leaks found in project code.

Constitution principles (backward compatibility, no breaking changes, incremental fixes) are respected by these recommendations.
