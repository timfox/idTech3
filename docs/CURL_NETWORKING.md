# libcurl in idTech3 (HTTP downloads, patching, and beyond)

This document explains **how this engine uses libcurl today**, how to **configure servers and clients** for asset delivery, and how to **extend** the same stack for MOTDs, images, APIs, or music—without confusing “what ships in-tree” with “what you can build on top.”

---

## 1. What is wired in today?

| Area | Role |
|------|------|
| **Build** | CMake option `USE_CURL` (default **ON**). When **OFF**, `src/client/cl_curl.c` is not compiled and all features below disappear. |
| **Linking** | On Linux/macOS the **client** links against the system libcurl when `find_package(CURL)` (or Android NDK paths) succeeds. If curl is missing, the client still builds but **HTTP downloads are disabled** (CMake prints a warning). |
| **Runtime** | All curl usage lives in the **client** (`cl_curl.c` / `cl_main.c`). The dedicated server does **not** embed this download path. |
| **Protocols** | Allowed schemes are **http**, **https**, **ftp**, and **ftps** only (see `ALLOWED_PROTOCOLS` in `cl_curl.c`). There is no built-in WebSocket or generic “any URL” API exposed to game VMs. |

So: **curl = client-side HTTP(S)/FTP file transfer**, primarily for **`.pk3`** delivery and related map pack flows—not a full “HTTP client for mods” unless you add code.

---

## 2. Build and install libcurl

### Linux (typical)

Install development headers so CMake can find curl:

```bash
sudo apt-get install libcurl4-openssl-dev
```

(See [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) for the full prerequisite list.)

Configure/build as usual (`./scripts/compile_engine.sh vulkan` or your CMake flow). If curl is found you should see a CMake line like:

```text
-- libcurl: /usr/lib/x86_64-linux-gnu/libcurl.so
```

### Windows / macOS

- **Windows**: ensure a **libcurl** your linker can resolve when building the client (vendored headers exist under `src/external/src/curl/windows/include/` for some layouts; the exact link story matches your toolchain).
- **macOS**: install/link libcurl via your package manager or SDK so `find_package(CURL)` succeeds.

### Android

CMake searches the NDK sysroot for `curl/curl.h` and `libcurl`. If `USE_CURL` is ON but curl is missing, enable `USE_CURL=OFF` or add curl to the NDK/sysroot you target.

---

## 3. Two download mechanisms (both use curl)

### A. Server-driven redirect download (`CL_cURL_*`)

When you connect and the server sends a download list, the client may fetch missing files **from the server’s HTTP mirror** instead of UDP.

**Flow (simplified):**

1. Server advertises `sv_allowDownload` and optionally `sv_dlURL` in serverinfo (`cl_parse.c` copies these into `clc.sv_allowDownload` / `clc.sv_dlURL`).
2. Client builds `@remote@local@…` download queue and, if redirect is allowed, calls `CL_cURL_BeginDownload(localName, va("%s/%s", clc.sv_dlURL, remoteName))`.
3. Bytes stream through `CL_cURL_CallbackWrite`; first chunk must look like a **valid Quake pak signature** (`CL_ValidPakSignature`) before the temp file is opened—**random HTML or 404 pages fail fast**.
4. `CL_cURL_PerformDownload` runs from `CL_Frame` while `clc.downloadCURLM` is active; on success the temp file is renamed into place and the download queue advances.

**Server operator checklist:**

- Set **`sv_allowDownload`** so clients may download (bit **`DLF_ENABLE` = 1** is required for the autodownload path; see `qcommon.h` `DLF_*` flags).
- Set **`sv_dlURL`** to the **base URL** of your file mirror (no trailing slash is stripped if present). Example: `https://cdn.example.com/q3a/paks`.
- Serve **real `.pk3`** bytes at `sv_dlURL/<remotePakName>` (or whatever names the game’s download list uses).

**Client cvars / flags (`cl_allowDownload`, bitmask):**

| Bit | Name | Meaning |
|-----|------|---------|
| `1` | `DLF_ENABLE` | Master switch: allow downloads when joining. |
| `2` | `DLF_NO_REDIRECT` | If set, **do not use HTTP/FTP** (curl path disabled); UDP only if allowed. |
| `4` | `DLF_NO_UDP` | If set, **no UDP** file transfer—often used with “HTTP only” policies. |
| `8` | `DLF_NO_DISCONNECT` | Server-side hint: some setups skip disconnect-before-download when this is set (see `cl_curl.c`). |

**Client library name (`cl_cURLLib`):** only used when the tree is built with **`USE_CURL_DLOPEN`** (dynamic load of `libcurl.so` / `libcurl-3.dll`). The default static-link path on this fork uses linked libcurl and does not require shipping a separate `cl_cURLLib` file.

---

### B. Manual / mirror download (`Com_DL_*` and `download` / `dlmap`)

For **operator-initiated** or **auto map** downloads, the client uses the **`download_t`** helper (`Com_DL_Begin` / `Com_DL_Perform` / `Com_DL_Cleanup`).

**Key cvars:**

| Cvar | Default (this tree) | Role |
|------|----------------------|------|
| **`cl_dlURL`** | `http://ws.q3df.org/maps/download/%1` | URL template for `\download` / `\dlmap`. |
| **`cl_dlDirectory`** | `0` | `0` = save into **current game dir**; `1` = save into **base game dir** (`FS_GetBaseGameDir()`). |

**URL patterns:**

1. **Placeholder `%1`**: the pak base name is URL-escaped and substituted. Example: `https://files.example.com/mods/%1` + local `mymap.pk3` → `https://files.example.com/mods/mymap` (escaped).
2. **No `%1`**: the client appends `/<escapedName>` to the URL you gave (directory style).
3. **`Content-Disposition` filename** path: if `%1` appears in the URL string, the client sets `headerCheck` and expects the server to return a disposition header whose filename ends in **`.pk3`**; the name is validated (`Com_DL_HeaderCallback`). Use this when the CDN issues redirects or canonical names differ from the pak basename.

**Commands:**

- **`download <pakname>`** — fetch a pak using `cl_dlURL`. Use **`download -`** to cancel / clean up.
- **`dlmap <mapname>`** — same pipeline but first checks that `maps/<name>.bsp` is not already inside an existing pak (avoids redundant downloads).

**Frame pump:** `CL_Frame` calls `Com_DL_Perform(&download)` whenever `download.cURL` is non-NULL—your download advances **without blocking the main loop** (multi interface).

**After success:** `Com_DL_Perform` calls **`FS_Reload()`** so new paks appear in the VFS; map autodownload may send **`donedl`** to the server or **`vid_restart`** during demo playback.

---

## 4. “Patching” and content updates

There is **no built-in binary diff or incremental patch** format over HTTP. What you have is **whole-file replacement**:

1. Publish a **new `.pk3`** with a higher lexical sort name (e.g. `zzz_patch_2026_05_02.pk3`) or replace an existing name if you control the mirror and clients re-fetch.
2. Point **`cl_dlURL`** or **`sv_dlURL`** at that mirror.
3. Use **`download`** / autodownload / **`dlmap`** so the client pulls the archive and **`FS_Reload()`** picks it up.

For **small hotfixes**, common patterns are: ship a tiny pk3 that overrides a few assets, or use your game’s own **script/Lua** update channel (not curl-specific) if you add it.

---

## 5. Images, websites, MOTD, JSON, music—what exists vs. what to add

The stock client **does not** expose curl to QVM, cgame, UI, or Lua as a generic “`http.get`” API. **`Com_DL_*` is specialized for `.pk3`-style binary payloads** (signature check, rename into game dirs).

| Use case | In engine today | Practical approach |
|----------|-----------------|--------------------|
| **Banner / MOTD / news** | Not built-in over curl | Serve a **text URL** via `cl_motdString`-style cvars (existing Q3 pattern) or add a small **C client** module that uses `curl_easy_*` into a ring buffer and feeds the UI VM. |
| **Web page in UI** | No embedded browser | Ship **plain text or image URLs** and render with your UI (or open the system browser externally—platform-specific). |
| **REST / JSON telemetry** | Not built-in | Add a **worker thread + curl** (or use `libcurl` multi + `curl_multi_wait`) from `qcommon` or `client`; **never** block `CL_Frame` on network I/O. |
| **Streaming music** | Codecs exist; HTTP radio not wired through this curl path | Use **static music files** in paks or extend the audio layer with a **dedicated stream reader** (curl read callback → decoder queue). |
| **User avatars / HTTP images** | Renderer loads from **FS** or pak | **Download to `fs_homepath`** with curl in new code, then **`re.RegisterShader`** / reload path—see renderer docs for hot reload. |

When you extend, **reuse the security patterns already in `cl_curl.c`:**

- Restrict **`CURLOPT_PROTOCOLS`** to HTTP(S)/FTP(S) (already done).
- Cap redirects (`CURLOPT_MAXREDIRS`).
- Validate **content types** and sizes before writing outside intended directories.
- Do **not** trust `Content-Disposition` filenames without the same validation as `Com_DL_HeaderCallback`.

---

## 6. Security and abuse considerations

1. **Only binary paks for the main download callbacks** — the first buffer must pass **`CL_ValidPakSignature`**; HTML error pages abort the transfer.
2. **HTTPS** is supported when libcurl is built with TLS (OpenSSL, etc.).
3. **User-agent / Referer** — server downloads set Referer to `ioQ3://<serveraddr>` and User-Agent to **`Q3_VERSION`** (identifies the client build).
4. **Operator trust** — whoever controls **`sv_dlURL`** / **`cl_dlURL`** controls what binaries the client will install into the game directory. Treat mirrors like **executable supply chain**.
5. **Rate and size** — there is no built-in global throttle; add CDN limits or extend **`Com_DL_CallbackProgress`** to enforce caps if you expose public mirrors.

---

## 7. Quick troubleshooting

| Symptom | Things to check |
|---------|------------------|
| “cURL library” / init warnings | libcurl dev package missing at **build** time, or `cl_cURLLib` wrong for **dlopen** builds. |
| HTTP disabled unexpectedly | `cl_allowDownload` includes **`DLF_NO_REDIRECT` (2)**. |
| UDP disabled errors | `cl_allowDownload` includes **`DLF_NO_UDP` (4)** but HTTP failed—adjust flags or fix URL. |
| Server never uses HTTP | **`sv_dlURL` empty** or **`sv_allowDownload`** missing redirect permission. |
| `cl_dlURL` “not set” | Empty **`cl_dlURL`** string when running **`download`**. |
| Download completes but map missing | Wrong **`cl_dlDirectory`** / wrong game dir; run **`fs_restart`** if you add files outside the downloader. |

---

## 8. Reference map (source files)

- **`src/client/cl_curl.c`** — `CL_cURL_*`, `Com_DL_*`, protocol allowlist, write/progress callbacks.
- **`src/client/cl_curl.h`** — `download_t`, curl typedefs / `qcurl_*` macros.
- **`src/client/cl_main.c`** — `CL_Frame` pump, `CL_Download` / `download` & `dlmap` commands, cvar registration.
- **`src/client/cl_parse.c`** — reads `sv_allowDownload`, `sv_dlURL` from serverinfo.
- **`src/qcommon/qcommon.h`** — `DLF_*` bitmask definitions.
- **`CMakeLists.txt`** — `USE_CURL`, `find_package(CURL)`, client link lines.

---

## 9. Summary

- **Today**, libcurl is the **client’s HTTPS/FTP download engine** for **`.pk3`**-style content, integrated with **autodownload**, **`sv_dlURL`**, and the **`download` / `dlmap`** commands.
- **Patching** is “ship a new pak and reload the filesystem,” not a built-in incremental patcher.
- **MOTD, websites, JSON, music streaming** are **design targets**: implement them with **additional client (or common) code** on top of the same libcurl dependency, following the non-blocking and validation patterns already used in `cl_curl.c`.

For build prerequisites, keep [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) as the source of truth for package names.
