# Gamepack Path Token Expansion

## Overview

The Radiant editor supports path token expansion in gamepack configurations, allowing gamepacks to define engine paths, game base directories, and user directories using environment variables and platform-specific paths.

## Supported Token Formats

### Windows-Style Tokens
- `%VAR%` - Expands to the value of environment variable `VAR`
- Example: `%ProgramFiles%\Quake3\` → `C:\Program Files\Quake3\`

### Unix-Style Tokens
- `${VAR}` - Expands to the value of environment variable `VAR`
- `$VAR` - Simple form (variable name must be valid identifier)
- Example: `${HOME}/.quake3/` → `/home/user/.quake3/`

### Home Directory
- `~` - Expands to user's home directory
- `~/path` - Expands to `$HOME/path`
- Example: `~/.radiant/` → `/home/user/.radiant/`

### Platform-Specific Paths

#### Windows
- `%ProgramFiles%` - Program Files directory (usually `C:\Program Files`)
- `%ProgramFiles(x86)%` or `%ProgramFilesX86%` - 32-bit Program Files
- `%AppData%` - Roaming Application Data (`%USERPROFILE%\AppData\Roaming`)
- `%LocalAppData%` - Local Application Data (`%USERPROFILE%\AppData\Local`)
- `%Documents%` - My Documents folder

#### Linux
- `${XDG_CONFIG_HOME}` - Config directory (default: `~/.config`)
- `${XDG_DATA_HOME}` - Data directory (default: `~/.local/share`)
- `${XDG_CACHE_HOME}` - Cache directory (default: `~/.cache`)

#### macOS
- `${ApplicationSupport}` - Application Support directory (`~/Library/Application Support`)

## Usage in Gamepack Configurations

Gamepack configuration files can use these tokens in path fields:

```json
{
  "enginePath": "%ProgramFiles%\\Quake3\\",
  "gameBasePath": "${HOME}/.quake3/baseq3/",
  "userPath": "${XDG_CONFIG_HOME}/qtradiant/quake3/"
}
```

## Path Normalization

All expanded paths are automatically normalized:
- Resolves `..` (parent directory references)
- Resolves `.` (current directory references)
- Normalizes path separators (forward/backward slashes) to platform-appropriate format
- Preserves absolute path indicators

### Examples

- `~/games/../tools/./editor` → `~/tools/editor`
- `C:\Games\..\Tools\.\Editor` → `C:\Tools\Editor`

## Fallback Behavior

If an environment variable is not found:
- The token is left **unchanged** in the path
- A warning is logged to help diagnose configuration issues
- This allows gamepacks to specify optional paths that may not exist on all systems

### Example

If `%QUAKE3_PATH%` is not set:
- Input: `%QUAKE3_PATH%\baseq3\`
- Output: `%QUAKE3_PATH%\baseq3\` (unchanged, with warning logged)

## Implementation

The path expansion is implemented in:
- `radiant/qt/path_expander.h` - PathExpander class interface
- `radiant/qt/path_expander.cpp` - Implementation
- `radiant/qt/qt_env.cpp` - Integration via `ExpandGamepackPath()` function

## API Usage

### C++ Code

```cpp
#include "qt_env.h"

// Expand a path with tokens
QString enginePath = ExpandGamepackPath("%ProgramFiles%\\Quake3\\");
QString gamePath = ExpandGamepackPath("${HOME}/.quake3/baseq3/");
```

### Direct PathExpander Usage

```cpp
#include "path_expander.h"

QString expanded = PathExpander::expandPath("%VAR%/path");
QString normalized = PathExpander::normalizePath("~/games/../tools/");
```

## Testing

To test path expansion:

1. Set environment variables:
   ```bash
   export TEST_PATH=/tmp/test
   ```

2. Use in gamepack config:
   ```json
   {
     "testPath": "${TEST_PATH}/game"
   }
   ```

3. Verify expansion:
   - Path should expand to `/tmp/test/game`
   - Check console logs for expansion messages

## Troubleshooting

### Token Not Expanding

1. Check environment variable is set:
   ```bash
   echo $VAR  # Unix
   echo %VAR% # Windows CMD
   ```

2. Verify token syntax:
   - Windows: `%VAR%` (with percent signs)
   - Unix: `${VAR}` or `$VAR`

3. Check console logs for warnings about missing variables

### Path Normalization Issues

- Ensure paths use forward slashes or backslashes consistently
- Avoid excessive `..` references that go beyond root directory
- Check that absolute paths start with `/` (Unix) or `C:\` (Windows)

## Future Enhancements

- Support for `~username` expansion (currently warns)
- Additional platform-specific paths as needed
- Path validation and existence checking
- Relative path resolution from gamepack file location
