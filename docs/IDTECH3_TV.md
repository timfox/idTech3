# idTech3 TV live streaming

`idTech3-tv` is treated as a separate Owncast-compatible service. The engine can publish gameplay to its RTMP ingest endpoint without embedding the Go server in the engine binary.

The default `engine` backend captures rendered engine frames through the renderer capture path and muxes the engine audio mixer through the existing AVI pipe into FFmpeg. Pipe I/O runs on a dedicated streaming worker thread so the main client thread only enqueues capture chunks. The older desktop/audio grab command remains available as the `external` backend for compatibility.

## Quick Start

Run the streaming service separately, then configure the client:

```cfg
seta cl_stream_enable 1
seta cl_stream_backend "engine"
seta cl_stream_url "rtmp://127.0.0.1:1935/live"
seta cl_stream_key "YOUR_STREAM_KEY"
seta cl_stream_fps 30
seta cl_stream_bitrate "3500k"
seta cl_stream_audio_bitrate "128k"
stream_start
```

Use `stream_status` to inspect configuration. Use `stream_stop` to close the engine capture pipe. If you use `cl_stream_backend external`, `stream_stop` only marks the session inactive; stop the external FFmpeg process through your OS process tools if it is still running.

## Commands

| Command | Purpose |
|---------|---------|
| `stream_start` | Open the configured RTMP publisher. The default backend pipes engine frames and mixed audio to FFmpeg stdin. |
| `stream_stop` | Stop engine capture or mark an external capture inactive. |
| `stream_status` | Print current streaming configuration and last expanded command. |

## Cvars

| Cvar | Default | Notes |
|------|---------|-------|
| `cl_stream_enable` | `0` | Must be `1` before `stream_start` runs. |
| `cl_stream_url` | empty | RTMP ingest URL, for example `rtmp://host:1935/live`. |
| `cl_stream_key` | empty | Protected stream key from the service. |
| `cl_stream_backend` | `engine` | `engine` captures renderer frames and engine-mixed audio; `external` launches the desktop/audio grab template. |
| `cl_stream_ffmpeg` | `ffmpeg` | FFmpeg executable path. |
| `cl_stream_cmd` | empty | Optional command template override. For the `engine` backend the command must read AVI from stdin. |
| `cl_stream_width` / `cl_stream_height` | `1280` / `720` | Capture size used by the `external` desktop capture template. Engine capture uses the renderer capture size. |
| `cl_stream_fps` | `30` | Capture frame rate. |
| `cl_stream_bitrate` | `3500k` | Video bitrate. |
| `cl_stream_audio_bitrate` | `128k` | Audio bitrate. |
| `cl_stream_queueMegs` | `64` | Maximum queued engine stream pipe data before chunks are dropped to protect memory. |
| `cl_stream_autoStart` | `0` | Automatically run `stream_start` during client init. |

`cl_stream_cmd` supports the shared engine template tokens: `%P` FFmpeg executable, `%U` RTMP URL, `%K` stream key, `%L` title, `%W` width, `%H` height, `%F` FPS, `%V` video bitrate, and `%Q` audio bitrate.

## Backend Details

The `engine` backend opens a pipe equivalent to:

```text
ffmpeg -f avi -i - -threads 0 -y -c:v libx264 -preset veryfast -tune zerolatency -b:v <bitrate> -pix_fmt yuv420p -c:a aac -b:a <audio bitrate> -f flv <rtmp>/<key>
```

The input is not a screen grab. It is the engine's captured frame stream plus the mixed game audio buffer. This avoids compositor/window-capture fragility and keeps the stream source tied to actual rendered frames.

The streaming worker is intentionally scoped to live engine publishing. Legacy `video-pipe` demo export keeps its existing synchronous behavior.

If the encoder or network is slower than capture, the engine drops whole queued video/audio chunks after `cl_stream_queueMegs` instead of letting memory grow without bound. Drops are record-aligned so the pipe does not emit partial AVI chunks. `stream_status` reports queued data, peak queue, dropped chunks, dropped bytes, and pipe failure state.

The `external` backend is still useful on platforms where the renderer capture path is unavailable or when a player wants to stream overlays outside the engine window.
