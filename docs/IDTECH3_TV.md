# idTech3 TV live streaming

`idTech3-tv` is treated as a separate Owncast-compatible service. The engine can launch an external FFmpeg RTMP publisher from the console so players can stream gameplay to that service without embedding the Go server in the engine binary.

## Quick Start

Run the streaming service separately, then configure the client:

```cfg
seta cl_stream_enable 1
seta cl_stream_url "rtmp://127.0.0.1:1935/live"
seta cl_stream_key "YOUR_STREAM_KEY"
seta cl_stream_width 1280
seta cl_stream_height 720
seta cl_stream_fps 30
seta cl_stream_bitrate "3500k"
seta cl_stream_audio_bitrate "128k"
stream_start
```

Use `stream_status` to inspect configuration. Use `stream_stop` to mark the engine-side session inactive; if your FFmpeg process is still running, stop it through your OS process tools.

## Commands

| Command | Purpose |
|---------|---------|
| `stream_start` | Launch the configured external RTMP publisher. |
| `stream_stop` | Mark the stream inactive in the engine. |
| `stream_status` | Print current streaming configuration and last expanded command. |

## Cvars

| Cvar | Default | Notes |
|------|---------|-------|
| `cl_stream_enable` | `0` | Must be `1` before `stream_start` runs. |
| `cl_stream_url` | empty | RTMP ingest URL, for example `rtmp://host:1935/live`. |
| `cl_stream_key` | empty | Protected stream key from the service. |
| `cl_stream_ffmpeg` | `ffmpeg` | FFmpeg executable path. |
| `cl_stream_cmd` | empty | Optional command template override. |
| `cl_stream_width` / `cl_stream_height` | `1280` / `720` | Capture size used by the default FFmpeg template. |
| `cl_stream_fps` | `30` | Capture frame rate. |
| `cl_stream_bitrate` | `3500k` | Video bitrate. |
| `cl_stream_audio_bitrate` | `128k` | Audio bitrate. |
| `cl_stream_autoStart` | `0` | Automatically run `stream_start` during client init. |

`cl_stream_cmd` supports the shared engine template tokens: `%P` FFmpeg executable, `%U` RTMP URL, `%K` stream key, `%L` title, `%W` width, `%H` height, `%F` FPS, `%V` video bitrate, and `%Q` audio bitrate.

## Scope

The first integration launches a platform-specific FFmpeg desktop/audio capture command and pushes FLV/RTMP to idTech3-tv or any Owncast-compatible ingest. It does not yet capture Vulkan swapchain frames directly or mux in-engine audio buffers. Those can be added later behind the same `stream_*` command surface.
