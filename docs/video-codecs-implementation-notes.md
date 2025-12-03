# Video Codec Implementation Notes

## Build Integration

The new codec source files are automatically included via `AUX_SOURCE_DIRECTORY(src/client CLIENT_SRCS)` in CMakeLists.txt:
- `cl_cin_codec.c` - Codec detection system
- `cl_cin_theora.c` - Theora decoder (when USE_THEORA is enabled)
- `cl_cin_vpx.c` - VP8/VP9 decoder (when USE_VPX is enabled)

## Codec Detection

### ROQ
- Magic number: `0x1084` (first 2 bytes)
- Always enabled

### Theora
- Magic number: `OggS` (first 4 bytes)
- Note: OggS header indicates Ogg container, but actual Theora detection happens during initialization when parsing Theora identification headers
- Falls back to extension-based detection (`.ogv`, `.ogg`)

### VP8/VP9
- Magic number: EBML header `0x1A45DFA3` (first 4 bytes)
- Falls back to extension-based detection (`.webm`, `.vp8`, `.vp9`)

## Error Handling

### Initialization Failures
- All codecs check for successful initialization
- Proper cleanup on failure (shutdown functions called)
- File handles closed and codec set to CODEC_NONE
- User-friendly error messages

### Runtime Errors
- Theora: Handles TH_EFAULT (bad packets), TH_DUPFRAME (duplicate frames)
- VPX: Handles VPX_CODEC_OK and other VPX error codes
- End-of-file detection with proper looping support

## Memory Management

### Buffer Allocation
- Frame buffers allocated with `Z_Malloc` using `TAG_TEMP`
- Error checking for allocation failures
- Proper cleanup in shutdown functions

### Codec-Specific Data
- Theora: `theora_data_t` structure
- VPX: `vpx_data_t` structure
- Allocated per-handle, freed in shutdown

## Reset and Looping

### Reset Functions
- **Theora**: Reinitializes Ogg sync/stream, re-reads headers, recreates decoder
- **VPX**: Resets file position, destroys and recreates decoder
- Both maintain file handle and codec state

### Looping Support
- Detected in `CIN_RunCinematic` when status is FMV_EOF
- Calls reset function if looping is enabled
- Sets status to FMV_LOOPED, which is converted to FMV_PLAY

## Frame Timing

### Theora
- Uses FPS from Theora info header (`fps_numerator / fps_denominator`)
- Defaults to 30 FPS if not specified
- Frame delay calculated as `1000 / fps` milliseconds

### VPX
- Defaults to 30 FPS (can be enhanced with WebM container parsing)
- Frame delay calculated as `1000 / fps` milliseconds

## YUV to RGB Conversion

Both codecs use similar YUV to RGB conversion:
- YUV 4:2:0 format (chroma subsampled)
- Standard ITU-R BT.601 conversion coefficients
- Clamps RGB values to 0-255 range
- Outputs RGBA format (alpha always 255)

## Known Limitations

### Theora
- Audio tracks in Ogg files are ignored (video-only playback)
- Multiple video streams not supported (uses first Theora stream found)

### VP8/VP9
- WebM container parsing is simplified (reads raw VPX data)
- Full WebM parsing would require EBML library for proper frame extraction
- Audio tracks not supported
- Frame boundaries may not be perfectly aligned without proper container parsing

## Future Enhancements

1. **WebM Container Parsing**: Implement proper EBML parsing for VP8/VP9
2. **Audio Support**: Extract and play audio tracks from Theora/VPX files
3. **Multiple Streams**: Support files with multiple video streams
4. **Seeking**: Add support for seeking to specific time positions
5. **Hardware Acceleration**: Use GPU for YUV to RGB conversion if available

