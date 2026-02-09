# Console extensions ported from ioquake3

The ioquake3 project adds a rich console of cvars and commands beyond vanilla Quake III. To keep our fork aligned with that extended tooling, the table below mirrors the newly documented cvars/commands that ship with ioquake3 (see [https://github.com/ioquake/ioq3](https://github.com/ioquake/ioq3) for the upstream reference). You can use these as the basis for future copy/paste replacements that we may backport into `r_`, `cl_`, `com_`, etc. in this repo.

## Client cvars

| Cvar | Purpose |
| --- | --- |
| `cl_autoRecordDemo` | Start a new demo recording automatically whenever the map changes. |
| `cl_aviFrameRate` | Target framerate when capturing video via `video`. |
| `cl_aviMotionJpeg` | Use MJPEG when recording via `video`. |
| `cl_guidServerUniq` | Give each server connection a unique GUID. |
| `cl_cURLLib` | Path to the cURL library the engine will load on non‑Windows. |
| `cl_consoleKeys` | Characters/key names that toggle the console (space‑delimited). |
| `cl_mouseAccelStyle` | Set to `1` for QuakeLive style mouse acceleration, `0` for the original style. |
| `cl_mouseAccelOffset` | Adjust the acceleration curve to fine tune pointer response. |

## Console helpers

| Cvar | Purpose |
| --- | --- |
| `con_autochat` | Set to `0` to prevent console input being broadcast as chat when it lacks a leading slash. |
| `con_autoclear` | Set to `0` to avoid the console text clearing whenever the console closes. |
| `con_scale` | Scale the console output for legibility at high resolutions (`1` default, up to `4`, fractional values allowed). |
| `con_notifylines` | Set how many lines the notify area shows at the top of the console. |

## Input/joystick cvars

| Cvar | Purpose |
| --- | --- |
| `in_joystickUseAnalog` | Keep joystick axes as raw analog values instead of mapping them to keys. |
| `j_forward`, `j_side`, `j_up`, `j_pitch`, `j_yaw` | Analog stick scaling for each movement axis. |
| `j_*_axis` | Map physical joystick axes to logical actions (forward, side, up, pitch, yaw). |
| `in_joystickNo`, `in_availableJoysticks`, `in_keyboardDebug` | Query available joysticks and debug keyboard/joystick binding behavior. |

## Sound subsystem cvars

| Cvar | Purpose |
| --- | --- |
| `s_useOpenAL` | Toggle the OpenAL backend (requires restart) instead of the SDL fallback. |
| `s_alDopplerFactor`, `s_alDopplerSpeed`, `s_alRolloff`, `s_alMinDistance`, `s_alMaxDistance`, `s_alGraceDistance` | Fine-tune OpenAL attenuation/doppler/softening and the reference distance shown to listeners. |
| `s_alDevice`, `s_alInputDevice`, `s_alDriver`, `s_alAvailableDevices`, `s_alAvailableInputDevices`, `s_alSources`, `s_alPrecache` | Provide device/source hints, driver branding, and lists of available OpenAL playback/capture endpoints. |
| `s_alGain`, `s_sdl*` | Control overall volume or configure SDL audio (bit depth, sample rate, channel count, buffer overrides). |
| `s_backend` | Read-only string describing the active backend plus the driver set via `s_alDriver`. |
| `s_muteWhenMinimized`, `s_muteWhenUnfocused` | Mute audio when the window loses focus or is minimized. |
| `sv_dlRate` | Set the download bandwidth cap for server-side PK3 streaming (kbyte/s). |

## Console/system cvars

| Cvar | Purpose |
| --- | --- |
| `com_ansiColor`, `com_altivec` | Enable ANSI output in the terminal, enable Altivec on PowerPC. |
| `com_standalone`, `com_basegame`, `com_homepath` | Control standalone mode and where user data lives. |
| `com_legacyprotocol` | Force legacy 1.32c protocol numbers. |
| `com_maxfpsUnfocused`, `com_maxfpsMinimized` | Cap FPS when the window is unfocused/minimized. |
| `com_busyWait` | Busy loop waiting instead of sleeping (non‑zero means busy wait). |
| `com_pipefile` | Named pipe path for external control (POSIX only). |
| `com_gamename`, `com_protocol` | Override the gamename/protocol numbers the client advertises on the network. |

## Networking/server cvars

| Cvar | Purpose |
| --- | --- |
| `sv_dlURL`, `sv_banFile` | Where the server fetches downloadable PK3s from and where it stores ban lists. |
| `net_ip6`, `net_port6`, `net_enabled`, `net_mcast6addr`, `net_mcastiface` | IPv6 binding and multicast controls (bitmask to enable/disable IPv4, IPv6, etc.). |

## Renderer cvars

| Cvar | Purpose |
| --- | --- |
| `r_allowResize` | Allow the window to be resized at runtime. |
| `r_ext_texture_filter_anisotropic` | Enable anisotropic filtering. |
| `r_zProj`, `r_greyscale`, `r_stereoEnabled`, `r_anaglyphMode`, `r_stereoSeparation` | Stereo/anaglyph/tone controls. |
| `r_marksOnTriangleMeshes` | Expand impact mark support when rendering MD3s. |
| `r_sdlDriver`, `r_noborder`, `r_screenshotJpegQuality`, `r_aviMotionJpegQuality`, `r_mode` | SDL driver, window border, screenshot/video quality, and desktop-resolution mode. |

## Additional commands

| Command | Purpose |
| --- | --- |
| `video <filename>` | Begin video capture (use with `demo`). |
| `stopvideo`, `stopmusic` | Stop recording/displayed music. |
| `minimize` | Minimize the game window and show the desktop. |
| `togglemenu` | Sends an ESC key event; works even inside binds/UI. |
| `print`, `unset`, `cvar_modified [filter]` | Console utilities for inspecting/unsetting cvars. |
| `banaddr`, `exceptaddr`, `bandel`, `exceptdel`, `listbans`, `rehashbans`, `flushbans` | Banlist management helpers for servers. |
| `net_restart`, `game_restart <fs_game>` | Restart networking or hot-swap modifications. |
| `which <filename/path>` | Print the filesystem path used for a loaded asset. |
| `execq <filename>` | Quietly exec a config script (no console output). |
| `kicknum`, `kickall`, `kickbots` | Kick player(s) or bots from the server. |
| `tell <client num> <msg>` | Send a direct chat to a client. |
| `addbot random` | Add a random bot instead of a named one. |

More commands (e.g., `video`, `togglemenu`) might be available through future engine work; we can use this document as a checklist when porting those features.
