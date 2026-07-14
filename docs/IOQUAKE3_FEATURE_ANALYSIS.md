# ioquake3 Feature Analysis: Comparison with Our idTech3 Fork

**Date**: February 2026
**Scope**: Engine-level improvements only (not game logic)
**Methodology**: Web research of ioquake3 features + exhaustive grep/analysis of `/workspace` codebase

---

## Executive Summary

Our fork is based on **Quake3e** (ec-/Quake3e), which is itself derived from ioquake3-r1160. This means we already inherit many ioquake3 improvements, plus Quake3e's own performance and quality enhancements. Additionally, our fork has gone significantly further with **Vulkan 1.4 rendering**, **PBR materials**, **RTX ray tracing**, **Opus/FLAC/WebM codecs**, **OpenAL EFX/HRTF**, **Lua/Duktape scripting**, and **ImGui debug UI** — none of which exist in ioquake3.

However, several ioquake3 engine-level improvements are either **missing entirely** or **present but not enabled** in our build. This document provides a prioritized list.

---

## Feature Comparison Matrix

### Features We ALREADY HAVE (present and enabled)

| Feature | Status | Files/Evidence |
|---------|--------|----------------|
| SDL3 backend | ✅ Enabled | `CMakeLists.txt` USE_SDL=ON, `engine/platform/sdl/` |
| cURL HTTP/FTP downloads | ✅ Enabled | `CMakeLists.txt` USE_CURL=ON, `src/client/cl_curl.c` |
| OpenAL sound (surround 5.1/7.1) | ✅ Enabled | `CMakeLists.txt` USE_OPENAL=ON, `snd_backend_openal.c` |
| OpenAL EFX reverb/occlusion | ✅ Enabled | `src/audio/effects/snd_efx.c` |
| OpenAL HRTF support | ✅ Enabled | `snd_backend_openal.c` ALC_HRTF_SOFT |
| Ogg Vorbis audio codec | ✅ Enabled | `src/audio/codecs/snd_codec_ogg.c` |
| Opus audio codec | ✅ Enabled | `src/audio/codecs/snd_codec_opus.c` |
| FLAC audio codec | ✅ Enabled | `src/audio/codecs/snd_codec_flac.c` |
| MP3 audio codec | ✅ Enabled | `src/audio/codecs/snd_codec_mp3.c` |
| WebM audio codec | ✅ Enabled | `src/audio/codecs/snd_codec_webm.c` |
| PNG texture support | ✅ Enabled | `src/renderers/rendercommon/tr_image_png.c` |
| IQM model support | ✅ Enabled | `src/renderers/*/tr_model_iqm.c` |
| Persistent console history | ✅ Enabled | `src/qcommon/history.c` |
| GUID system | ✅ Enabled | `CL_UpdateGUID()` in `cl_main.c` |
| x86_64 JIT compiler | ✅ Enabled | `src/qcommon/vm_x86.c` |
| AArch64 JIT compiler | ✅ Enabled | `src/qcommon/vm_aarch64.c` |
| PowerPC JIT (ppc64) | ✅ Enabled | `src/qcommon/vm_powerpc.c` |
| ARMv7 JIT | ✅ Enabled | `src/qcommon/vm_armv7l.c` |
| VM bounds checking | ✅ Enabled | `VM_CheckBounds()`, `VM_CheckBounds2()` |
| OS-provided random bytes | ✅ Enabled | `Sys_RandomBytes()` → `/dev/urandom` or CryptoAPI |
| Multiuser Windows support | ✅ Enabled | `SHGetFolderPath(CSIDL_APPDATA)` |
| Colored terminal output | ✅ Enabled | `ttycon_ansicolor` cvar, `Sys_ANSIColorify()` |
| Download URL redirect | ✅ Enabled | `sv_dlURL`, `cl_dlURL` cvars |
| File extension security | ✅ Enabled | `FS_AllowedExtension()`, blocks .dll/.so/.dylib/.exe |
| Anaglyph stereo rendering | ✅ OpenGL only | `r_anaglyphMode` in OpenGL renderer (commented out in Vulkan) |
| msg.c maxbits overflow check | ✅ Partial | `msg->maxbits` field, checks in `MSG_WriteBits`/`MSG_ReadBits` |
| Improved command auto-completion | ✅ Enabled | `Field_AutoComplete` in `keys.c`, `cmd.c`, `cvar.c` |
| Vulkan renderer | ✅ OUR ADDITION | Not in ioq3 — our major enhancement |
| PBR materials | ✅ OUR ADDITION | Not in ioq3 |
| Ray tracing (RTX) | ✅ OUR ADDITION | Not in ioq3 |
| Lua/Duktape scripting | ✅ OUR ADDITION | Not in ioq3 |
| ImGui debug overlay | ✅ OUR ADDITION | Not in ioq3 |
| Video codecs (VP8, Theora, AV1, FFmpeg) | ✅ OUR ADDITION | `cl_cin_*.c` |

### Features PRESENT IN CODE but NOT ENABLED in Build

| Feature | Status | Details |
|---------|--------|---------|
| **IPv6 networking** | ⚠️ Code exists, build doesn't enable | `USE_IPV6` guards in `net_ip.c`, `qcommon.h`, `sv_init.c`, `cl_main.c` — but `USE_IPV6` is never defined in `CMakeLists.txt` |
| **VoIP support** | ⚠️ Code exists, build doesn't enable | `USE_VOIP` guards in `cl_scrn.c`, `cl_parse.c`, `snd_backend_*.c` — protocol commands `svc_voipSpeex`/`svc_voipOpus` reserved in `qcommon.h` — but `USE_VOIP` never defined in `CMakeLists.txt`. VoIP client implementation (`CL_CaptureVoip`, `CL_ParseVoip`) appears to be missing from `cl_main.c`. |
| **Mumble positional audio** | ⚠️ Minimal reference only | Single `USE_MUMBLE` check in `snd_backend_sdl.c` — but no `libmumblelink.c` or actual Mumble integration code |

### Features MISSING ENTIRELY

| Feature | Priority | Details |
|---------|----------|---------|
| **Hardened Huffman decoding (CVE-2017-11721)** | 🔴 CRITICAL | Our `huffman.c` uses the old `Huff_Receive()` without maxoffset parameter. ioquake3 replaced this with `Huff_offsetReceive()` that accepts a maxoffset to prevent reading past buffer boundaries. Our partial mitigation (`bloc >> 3 > size` check between characters) does NOT protect during the Huffman tree walk itself. |
| **Autoupdater system** | 🟡 LOW | ioquake3's autoupdater uses RSA signature verification + SHA-256 checksums for secure binary updates. We have no equivalent. May not be relevant for our use case. |
| **SPARC JIT compiler** | 🟢 VERY LOW | ioquake3 has SPARC (sparc32/sparc64) JIT. Extremely niche platform. |

---

## Prioritized Recommendations

### Priority 1: CRITICAL — Security Fixes

#### 1.1 Harden Huffman Decoding (CVE-2017-11721)
- **Risk**: Remote denial-of-service or potential code execution via crafted network packets
- **File**: `src/qcommon/huffman.c`
- **What to do**: Port ioquake3 commit `d2b1d124d4055c2fcbe5126863487c52fd58cca1`
  - Replace `Huff_Receive()` with `Huff_offsetReceive()` that takes a `maxoffset` parameter
  - Replace `send()` with a version that checks offset bounds
  - Pass buffer size limits from `Huff_Decompress()` and `Huff_Compress()` into the tree walk functions
  - Remove the `FIXME` comment at line 343 of `huffman.c`
- **Current state**: We have a partial mitigation — `maxbits` field in `msg_t` struct catches overflows at the `MSG_ReadBits`/`MSG_WriteBits` level, and there's a check `if ( (bloc >> 3) > size )` between characters in `Huff_Decompress`. However, the actual `Huff_Receive` tree walk can still read past the buffer before this inter-character check fires.
- **Effort**: Small (~50 lines changed)
- **Impact**: Closes a known CVE that could be exploited in multiplayer

#### 1.2 Review Download File Type Restrictions
- **Status**: We already have `FS_AllowedExtension()` blocking `.dll`, `.so`, `.dylib`, `.exe`, `.qvm`, `.pk3`
- **Recommendation**: Verify our restrictions match or exceed ioquake3's (CVE-2017-6903 fix). Specifically check that auto-downloaded `.pk3` files cannot contain native code that gets loaded. Our `FS_CheckFilenameIsNotAllowed()` appears comprehensive.
- **Effort**: Audit only, no code changes likely needed

### Priority 2: HIGH — Enable Existing Code

#### 2.1 Enable IPv6 Networking (USE_IPV6)
- **What**: Add `OPTION(USE_IPV6 "Enable IPv6 networking support" ON)` to `CMakeLists.txt` and wire up `add_compile_definitions(USE_IPV6)` when enabled
- **Why**: The code is already written and tested (from Quake3e/ioquake3). IPv6-only networks are increasingly common. Enabling this is essentially free.
- **Current state**: All the code exists behind `#ifdef USE_IPV6` guards in `net_ip.c`, `qcommon.h`, `sv_init.c`, `sv_main.c`, `cl_main.c`, `vm_aarch64.c`, `md5.c`
- **Effort**: Minimal CMake change + testing
- **Risk**: Very low — well-tested code, just needs the define

#### 2.2 Enable VoIP Support (USE_VOIP)
- **What**: Add `OPTION(USE_VOIP "Enable in-engine VoIP support" ON)` and required definitions
- **Why**: In-engine voice chat using Opus codec is a significant multiplayer feature
- **Current state**: 
  - Protocol messages reserved (`svc_voipSpeex`, `svc_voipOpus`, `clc_voipSpeex`, `clc_voipOpus`)
  - Audio backend capture code exists in both OpenAL and SDL backends
  - `cl_parse.c` has VoIP packet handling  
  - OpenAL backend has full spatial VoIP rendering (`al_voip_channel_t`)
  - **MISSING**: Core VoIP client logic (`CL_CaptureVoip`, voip cvar setup) appears absent from `cl_main.c`. Server-side VoIP relay code appears absent from `sv_client.c`. This would need to be ported from ioquake3.
- **Effort**: Medium — some client/server VoIP logic needs porting
- **Dependencies**: Opus library (already vendored in `src/external/src/opus/`)

#### 2.3 Add Mumble Positional Audio Integration
- **What**: Port `libmumblelink.c` from ioquake3 and add proper `USE_MUMBLE` build option
- **Why**: Allows positional audio in Mumble voice chat based on in-game player position
- **Current state**: Single `USE_MUMBLE` reference in SDL audio capture code, but no actual Mumble link library
- **Effort**: Small — ioquake3's `libmumblelink.c` is ~100 lines of shared memory IPC
- **Risk**: Low — completely optional feature behind a cvar

### Priority 3: MEDIUM — Nice-to-Have Improvements

#### 3.1 Anaglyph Stereo for Vulkan Renderer
- **What**: Port the anaglyph stereo rendering from OpenGL renderer to Vulkan
- **Current state**: OpenGL renderer has full `r_anaglyphMode` support. Vulkan renderer has it commented out (`//r_anaglyphMode` in `tr_init.c`)
- **Effort**: Medium — needs Vulkan render pass modifications

#### 3.2 Autoupdater System
- **What**: Implement a secure binary update mechanism
- **Design**: ioquake3 uses RSA-signed manifests + SHA-256 file checksums + libtomcrypt
- **Why**: Convenient for end-user security patches
- **Effort**: Large — new subsystem
- **Priority**: Lower because our distribution model may differ

### Priority 4: LOW — Minimal Value

#### 4.1 SPARC JIT Compiler
- **What**: JIT compiler for SPARC/SPARC64 architectures
- **Why**: Extremely niche. We already have x86_64, AArch64, ARMv7, and PowerPC JIT
- **Recommendation**: Skip unless there is specific demand

---

## Summary of Our Advantages Over ioquake3

Our fork significantly **exceeds** ioquake3 in several areas:

| Area | Our Fork | ioquake3 |
|------|----------|----------|
| Rendering | Vulkan 1.4 + OpenGL, PBR, RTX ray tracing | OpenGL only |
| Audio codecs | Opus, FLAC, OGG, MP3, WebM | OGG only |
| Audio effects | OpenAL EFX reverb, HRTF, occlusion | Basic OpenAL |
| Video codecs | VP8, Theora, AV1, FFmpeg | RoQ only |
| Scripting | Lua + Duktape (JavaScript) | None |
| Debug tools | ImGui overlay | None |
| JIT targets | x86_64, AArch64, ARMv7, PPC64 | x86_64, PPC64, SPARC |
| VM security | Bounds checking, runtime checks | Basic bounds checking |
| Build system | CMake 3.24+ with C23/C++23 | CMake/Make |

---

## Action Items (Ordered by Priority)

1. **🔴 CRITICAL**: Port ioquake3 Huffman hardening (CVE-2017-11721 fix) → `huffman.c`
2. **🟠 HIGH**: Enable `USE_IPV6` in `CMakeLists.txt` (code already exists)
3. **🟠 HIGH**: Port missing VoIP client/server logic from ioquake3, enable `USE_VOIP`
4. **🟡 MEDIUM**: Port `libmumblelink.c` from ioquake3, add `USE_MUMBLE` build option
5. **🟡 MEDIUM**: Port anaglyph stereo to Vulkan renderer
6. **🔵 LOW**: Evaluate autoupdater need for our distribution model
7. **⚪ SKIP**: SPARC JIT (no demand)

---

## References

- ioquake3 improvements list: https://ioquake3.org/improvements/
- ioquake3 GitHub: https://github.com/ioquake/ioq3
- Huffman fix commit: https://github.com/ioquake/ioq3/commit/d2b1d124d4055c2fcbe5126863487c52fd58cca1
- CVE-2017-11721: Buffer overflow in MSG_ReadBits
- CVE-2017-6903: Auto-download sandbox escape
- Quake3e (our upstream): https://github.com/ec-/Quake3e
