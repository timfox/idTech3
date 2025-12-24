# VM Hot Reload System

This document describes the QVM (Quake Virtual Machine) hot reloading system that enables rapid iteration during game development.

## Overview

The VM hot reload system automatically detects changes to QVM files and reloads the corresponding virtual machines without restarting the game. This dramatically improves the development workflow by allowing code changes to be tested immediately.

## Features

- **Automatic Detection**: Monitors QVM files for changes using content checksums
- **Instant Reloading**: Reloads VMs within milliseconds of file changes
- **Manual Control**: Console commands for manual reloading
- **Selective Reloading**: Reload individual VMs or all at once
- **Development Focused**: Only active during development builds

## Configuration

### CVARs

- `vm_hotReload` (default: 1)
  - Enable/disable automatic hot reloading
  - Set to 0 to disable automatic reloading

- `vm_hotReloadInterval` (default: 500)
  - Check interval in milliseconds
  - Lower values provide faster detection but may impact performance

- `vm_hotReloadVerbose` (default: 0)
  - Enable verbose logging
  - Shows detailed information about reload operations

## Console Commands

### Manual Reloading

```bash
# Reload a specific VM
vm_reload <vm_name>

# Examples:
vm_reload cgame    # Reload client game logic
vm_reload game     # Reload server game logic
vm_reload ui       # Reload user interface logic

# Reload all VMs
vm_reload_all

# Show hot reload status
vm_hotreload_status
```

### Available VM Names

The following VMs can be reloaded:
- `cgame` - Client game logic
- `game` - Server game logic
- `ui` - User interface logic

## Workflow

### Development Setup

1. **Build QVM files** with your changes
2. **Copy to mod directory**: Place `.qvm` files in `baseq3/vm/` or your mod's `vm/` directory
3. **Enable hot reload**: Ensure `vm_hotReload` is set to 1
4. **Start the game** with your mod loaded

### Automatic Reloading

Once enabled, the system will:
1. Monitor QVM files for changes every 500ms (configurable)
2. Calculate content checksums to detect modifications
3. Automatically reload changed VMs
4. Display reload confirmation in console

### Manual Reloading

For precise control:
1. Make code changes and rebuild QVMs
2. Use `vm_reload <name>` to reload specific VMs
3. Use `vm_reload_all` to reload everything at once

## Technical Details

### Detection Mechanism

The system uses content-based change detection:
- Calculates FNV-1a checksums of QVM file contents
- Compares against previously stored checksums
- Triggers reload when checksums differ

This approach is more reliable than file modification times and works across different file systems.

### Reload Process

1. **Detection**: File change detected via checksum comparison
2. **Validation**: Ensures VM is currently loaded
3. **Reload**: Calls `VM_Restart()` to reload VM with new code
4. **Notification**: Console output confirms successful reload
5. **Continuation**: Game continues running with new code

### Safety Features

- **Error Handling**: Failed reloads don't crash the game
- **State Preservation**: Reload maintains existing game state where possible
- **Selective**: Only reloads VMs that actually changed
- **Performance**: Minimal overhead when disabled

## Limitations

### Current Constraints

- **File System Dependent**: Relies on engine's file system for reading QVMs
- **State Loss**: Some VM-specific state may be lost during reload
- **Timing Sensitive**: Best used during development, not during critical gameplay
- **Memory Usage**: Maintains checksum cache in memory

### Future Improvements

- **Granular Reloading**: Reload individual functions rather than entire VMs
- **State Preservation**: Better handling of VM-internal state across reloads
- **Live Debugging**: Integration with debuggers for seamless stepping
- **Asset Reloading**: Extend to other asset types (shaders, models, etc.)

## Troubleshooting

### Common Issues

**Q: Hot reload not working**
A: Check that `vm_hotReload` is set to 1 and QVM files are in the correct location

**Q: Reloads taking too long**
A: Reduce `vm_hotReloadInterval` or disable verbose logging

**Q: Game crashes after reload**
A: Some game state may be incompatible with reloaded code. Consider restarting the map

**Q: False positive reloads**
A: The checksum system is very sensitive. Ensure build process is complete before testing

### Debug Information

Enable verbose logging to see detailed information:

```bash
set vm_hotReloadVerbose 1
vm_hotreload_status
```

This will show:
- Current hot reload settings
- Loaded VMs and their file paths
- Detailed reload operation logs

## Integration with Build System

### Recommended Build Setup

```bash
# Build script with hot reload support
#!/bin/bash

# Build QVMs
make qvms

# Copy to mod directory
cp build/release-linux-x86_64/baseq3/vm/*.qvm ~/ioquake3/baseq3/vm/

# Notify developer
echo "QVMs built and deployed. Hot reload active."
```

### CI/CD Considerations

For automated testing:
- Disable hot reload in CI: `+set vm_hotReload 0`
- Use fixed QVM builds to ensure consistent test results
- Enable verbose logging for debugging failed tests

## Performance Impact

### When Enabled
- **CPU**: Minimal (~0.1% during checks, occurs every 500ms)
- **Memory**: Small checksum cache (~100 bytes per VM)
- **Disk I/O**: File reads only when changes detected

### When Disabled
- **Zero overhead**: System completely inactive
- **No background monitoring**
- **No memory usage**

## Best Practices

### Development Workflow

1. **Enable hot reload** in development builds
2. **Use manual reload** for critical testing
3. **Disable for release** builds and performance testing
4. **Monitor console output** for reload confirmations

### Code Organization

- **Modular Code**: Design code to minimize state dependencies
- **Error Handling**: Robust error handling in VM code
- **State Management**: Clear separation between persistent and transient state
- **Testing**: Test reload scenarios during development

### Team Collaboration

- **Version Control**: Ensure QVM files are properly versioned
- **Build Scripts**: Standardized build and deployment process
- **Documentation**: Team awareness of hot reload capabilities
- **Conflict Resolution**: Clear process for handling reload conflicts

This hot reload system transforms the Quake 3 development experience by eliminating the need for frequent game restarts, enabling rapid iteration and more efficient debugging workflows.