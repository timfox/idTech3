# Steam Deck Integration

## Overview

This document describes the Steam Deck features integrated into the engine and mymod, following Steam's recommendations for Steam Deck compatibility.

## Features Implemented

### 1. Input System

#### Default Controller Configuration
- **Status**: ✅ Implemented
- **Location**: `mymod/controller_configs/default.vdf`
- **Description**: Default controller configuration that maps all Steam Deck inputs to keyboard/mouse equivalents, ensuring full game functionality with controller only.

#### Text Input
- **Status**: ✅ Implemented
- **Location**: `src/client/cl_steamdeck.c`, `mymod/gamesrc/ui/ui_mfield.c`
- **APIs Used**:
  - `ShowFloatingGamepadTextInput()` - For direct key input
  - `ShowGamepadTextInput()` - For callback-based input
- **Integration**: Automatically shows on-screen keyboard when text fields receive focus on Steam Deck.

#### Simultaneous Input Support
- **Status**: ✅ Implemented
- **Location**: `src/client/cl_input.c`
- **Description**: Supports simultaneous mouse-style (gyro/trackpad) and joystick-style camera movement without input lockout. Both input methods work together seamlessly.

### 2. Graphics

#### Vulkan API
- **Status**: ✅ Already Using
- **Description**: Engine uses Vulkan as primary graphics API, providing best performance and battery life on Steam Deck.

#### Video Codecs
- **Status**: ⚠️ Needs Review
- **Current**: Uses Theora and VPX codecs
- **Recommendation**: Ensure VP9/AV1 support for video playback
- **Location**: `src/client/cl_cin_*.c`

### 3. Game Features

#### Cloud Saves
- **Status**: ✅ Implemented
- **Location**: `src/client/cl_steamdeck.c`
- **API**: Steam Cloud (`ISteamRemoteStorage`)
- **Features**:
  - Automatic save game sync
  - Cross-device save transfer
  - Does NOT sync device-specific settings (resolution, etc.)

#### Offline Mode
- **Status**: ✅ Supported
- **Description**: All singleplayer content is accessible without Internet connection. Engine supports offline play by default.

#### Launchers
- **Status**: ✅ No External Launcher
- **Description**: All functionality is in-game. No external launcher required.

## Implementation Details

### Steam Deck Detection

The engine automatically detects Steam Deck hardware by checking:
1. `/sys/devices/virtual/dmi/id/product_name` for "Jupiter" or "Steam Deck"
2. `SteamDeck` or `STEAMDECK` environment variables
3. Steam runtime environment

### Steam Input Integration

Steam Input API is initialized automatically when Steam Deck is detected:
- Enables gyroscope support
- Enables trackpad support
- Enables back paddle support
- Provides on-screen keyboard

### Text Input Integration

Text fields in the UI automatically show Steam Input on-screen keyboard when:
- Field receives focus
- Controller button is pressed while field is focused
- Text input is enabled (`cl_steamdeck_textinput` CVar)

### Cloud Save Integration

Save games are automatically synced to Steam Cloud when:
- Steam Cloud is enabled (`cl_steamdeck_cloudsaves` CVar)
- Steam account has Cloud enabled
- App has Cloud enabled

Save files are stored in `saves/` directory in Steam Cloud.

## CVars

- `cl_steamdeck_enable` (default: 1) - Enable Steam Deck features
- `cl_steamdeck_textinput` (default: 1) - Enable Steam Input on-screen keyboard
- `cl_steamdeck_cloudsaves` (default: 1) - Enable Steam Cloud save sync
- `cl_steamdeck_simultaneous_input` (default: 1) - Enable simultaneous mouse/controller input

## Controller Configuration

Default controller configuration is provided in:
- `mymod/controller_configs/default.vdf`

This configuration maps:
- D-Pad → WASD movement
- Face buttons → Mouse clicks, Space, R, Escape
- Triggers → Mouse buttons
- Bumpers → Q/E keys
- Sticks → Movement and camera
- Gyro → Mouse camera (1:1 movement)
- Trackpads → Mouse movement and clicks
- Back paddles → Tab and Enter

## Building with Steamworks SDK

1. Download Steamworks SDK from https://partner.steamgames.com/doc/sdk
2. Extract to `third-party/steamworks/` or `libs/steamworks/`
3. Configure CMake with `-DUSE_STEAMWORKS=ON`
4. Build the project

## Testing

To test Steam Deck features:
1. Run on Steam Deck hardware
2. Or set `SteamDeck=1` environment variable
3. Or manually enable features with CVars

## Future Improvements

- [ ] Add VP9/AV1 codec support for video playback
- [ ] Enhanced controller configuration UI
- [ ] Steam Input action sets for different game modes
- [ ] Haptic feedback integration
- [ ] Performance profiles (Battery/Balanced/Performance)

## References

- [Steam Deck Compatibility Guide](https://partner.steamgames.com/doc/store/application/steamdeck)
- [Steamworks SDK Documentation](https://partner.steamgames.com/doc/sdk)
- [Steam Input API](https://partner.steamgames.com/doc/features/steam_controller/steam_input_api)

