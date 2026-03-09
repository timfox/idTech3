# Video Codec System

## Supported Formats

| Format | Extension | Codec | Library | License | Status |
|--------|-----------|-------|---------|---------|--------|
| ROQ | `.roq` | Id RoQ | Built-in | GPL2 | Always available |
| MP4 | `.mp4` | H.264/H.265 | FFmpeg | LGPL/GPL | Optional (`USE_FFMPEG`) |
| MKV | `.mkv` | Various | FFmpeg | LGPL/GPL | Optional (`USE_FFMPEG`) |
| WebM | `.webm` | VP8/VP9 | libvpx | BSD | Optional (`USE_VPX`) |
| AVI | `.avi` | Various | FFmpeg | LGPL/GPL | Optional (`USE_FFMPEG`) |
| MOV | `.mov` | Various | FFmpeg | LGPL/GPL | Optional (`USE_FFMPEG`) |
| Ogg Theora | `.ogv` | Theora | libtheora | BSD | Optional (`USE_THEORA`) |
| AV1 | `.av1` | AV1 | dav1d | BSD | Optional (`USE_DAV1D`) |

## Architecture

```
cl_cin.c           -- Legacy ROQ decoder + modern codec dispatcher
cl_cin_modern.c/h  -- Format detection, codec selection, fallback chain
cl_cin_ffmpeg.c    -- FFmpeg backend
cl_cin_dav1d.c     -- dav1d AV1 backend
cl_cin_vpx.c       -- libvpx VP8/VP9 backend
cl_cin_theora.c    -- Theora backend
```

## How It Works

1. `CIN_PlayCinematic()` opens the file
2. Checks ROQ magic bytes (`0x1084`) -- if ROQ, uses legacy decoder
3. If not ROQ, calls `CIN_DetectCodec()` to identify format by extension
4. Opens with the appropriate modern decoder backend
5. Falls back to FFmpeg if the specific codec isn't available
6. Frame decode feeds RGBA pixels to the renderer via `CIN_RunCinematic()`

## Enabling Codecs

```bash
cmake -DUSE_FFMPEG=ON -DUSE_DAV1D=ON -DUSE_VPX=ON -DUSE_THEORA=ON ..
```

Each codec is detected via pkg-config. Missing libraries produce warnings, not errors.

## Console Commands

```
ffmpeg codecs        -- list available codecs
ffmpeg info <file>   -- show media file information
ffmpeg play <file>   -- play a video file
```

## Image Formats

| Format | Extension | Library | HDR |
|--------|-----------|---------|-----|
| OpenEXR | `.exr` | tinyexr (BSD) | Yes |
| PNG | `.png` | libpng (vendored) | No |
| TGA | `.tga` | Built-in | No |
| JPEG | `.jpg` | libjpeg (vendored) | No |
| BMP | `.bmp` | Built-in | No |
| PCX | `.pcx` | Built-in | No |

### HDR Skybox

Load EXR panoramas as HDR skyboxes with IBL lighting:
```
r_skyboxHDR "textures/sky/forest.exr"
r_skyboxHDR_exposure 1.5
r_skyboxHDR_rotation 45
r_skyboxHDR_intensity 0.8
```

Supports: equirectangular, cubemap faces, vertical/horizontal cross, spherical mirror projections.
