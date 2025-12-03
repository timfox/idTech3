# Video Codec Support

This document describes the video codec support system added to id Tech 3.

## Overview

The engine now supports multiple GPL 2 compliant video codecs for cinematic playback:
- **ROQ** - Original Quake 3 codec (always enabled)
- **Theora** - Ogg Theora codec (optional, GPL 2 compatible)
- **VP8/VP9** - WebM codecs (optional, BSD license, GPL 2 compatible)

## Architecture

The codec system is modular and extensible:

1. **Codec Detection** (`cl_cin_codec.h`, `cl_cin_codec.c`)
   - Detects video format by magic number and file extension
   - Routes to appropriate decoder

2. **Main Cinematic System** (`cl_cin.c`)
   - Refactored to support multiple codecs
   - Routes codec-specific operations to appropriate handlers

3. **Codec Implementations** (to be implemented)
   - `cl_cin_theora.c` - Theora decoder
   - `cl_cin_vpx.c` - VP8/VP9 decoder

## Building

### Dependencies

**Theora:**
```bash
sudo apt-get install libtheora-dev
```

**VPX (VP8/VP9):**
```bash
sudo apt-get install libvpx-dev
```

### CMake Options

- `USE_THEORA` - Enable Theora support (default: ON)
- `USE_VPX` - Enable VP8/VP9 support (default: ON)

The build system will automatically detect available libraries and enable/disable support accordingly.

## Usage

The cinematic system automatically detects the codec based on file magic numbers or extensions:

- `.roq` files → ROQ codec
- `.ogv`, `.ogg` files → Theora codec (if enabled)
- `.webm`, `.vp8`, `.vp9` files → VP8/VP9 codec (if enabled)

Example:
```
/cinematic intro.roq
/cinematic intro.ogv
/cinematic intro.webm
```

## Implementation Status

### Completed
- ✅ Codec detection system
- ✅ Modular codec architecture
- ✅ CMake build system integration
- ✅ ROQ codec (existing, now integrated into new system)
- ✅ Theora decoder (`cl_cin_theora.c`)
- ✅ VP8/VP9 decoder (`cl_cin_vpx.c`)

### Notes
- Theora decoder fully implements Ogg Theora video playback
- VP8/VP9 decoder implements basic VP8/VP9 decoding (WebM container parsing can be enhanced)

## Codec Details

### ROQ
- **Status**: Fully implemented (original Quake 3 codec)
- **License**: GPL 2
- **File Extensions**: `.roq`
- **Magic Number**: `0x1084`

### Theora
- **Status**: Fully implemented
- **License**: BSD-style (GPL 2 compatible)
- **File Extensions**: `.ogv`, `.ogg`
- **Magic Number**: `OggS` header
- **Library**: libtheora
- **Features**: Full Ogg Theora video decoding with YUV to RGB conversion

### VP8/VP9
- **Status**: Basic implementation (WebM container parsing can be enhanced)
- **License**: BSD-style (GPL 2 compatible)
- **File Extensions**: `.webm`, `.vp8`, `.vp9`
- **Magic Number**: EBML header (`0x1A45DFA3`)
- **Library**: libvpx
- **Features**: VP8/VP9 video decoding with YUV to RGB conversion

## Adding New Codecs

To add a new codec:

1. Add codec type to `video_codec_t` enum in `cl_cin_codec.h`
2. Add detection function in `cl_cin_codec.c`
3. Add codec info entry in `codec_info[]` array
4. Implement decoder in new file (e.g., `cl_cin_decoder.c`)
5. Add codec-specific handlers in `cl_cin.c` switch statements
6. Update CMakeLists.txt to find and link library

## License Compatibility

All codecs are GPL 2 compatible:
- **ROQ**: Original Quake 3 codec (GPL 2)
- **Theora**: BSD-style license (compatible with GPL 2)
- **VP8/VP9**: BSD-style license (compatible with GPL 2)

