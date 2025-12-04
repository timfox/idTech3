# Syscall Registry System

## Overview

The syscall registry system provides centralized management of syscall numbers and extensions, improving maintainability and ensuring QVM compatibility.

## Architecture

### Centralized Registry

**Location**: `src/qcommon/syscall_registry.h`, `src/qcommon/syscall_registry.c`

The registry system provides:
- **Unified extension lookup** across all VM modules (game, cgame, ui)
- **Version tracking** for extension API compatibility
- **Documentation** of extension syscalls
- **Backward compatibility** with legacy extension system

### Extension Registration

Extensions are registered in `syscall_registry.c`:

```c
static syscall_extension_t g_game_extensions[] = {
    { "SVF_SELF_PORTAL2_Q3E", SVF_SELF_PORTAL2, SYSCALL_EXT_API_VERSION, "Portal rendering flag extension" },
    { "trap_Cvar_SetDescription_Q3E", G_CVAR_SETDESCRIPTION, SYSCALL_EXT_API_VERSION, "CVar description extension" },
    { NULL, -1, 0, NULL }
};
```

### Integration Points

**Server (Game Module)**:
- `src/server/sv_game.c` - `SV_GetValue()` uses centralized registry
- Maintains legacy `VM_Ext` system for compatibility

**Client (CGame Module)**:
- `src/client/cl_cgame.c` - `CL_GetValue()` uses centralized registry
- Falls back to legacy hardcoded extensions

**Client (UI Module)**:
- `src/client/cl_ui.c` - `UI_GetValue()` uses centralized registry
- Falls back to legacy hardcoded extensions

## API

### `Syscall_GetValue()`

Unified extension lookup function:

```c
qboolean Syscall_GetValue( vmIndex_t vm_index, char *value, int valueSize, const char *key );
```

**Parameters**:
- `vm_index`: VM module (VM_GAME, VM_CGAME, VM_UI)
- `value`: Output buffer for syscall number (as string)
- `valueSize`: Size of value buffer
- `key`: Extension key name (e.g., "trap_Cvar_SetDescription_Q3E")

**Returns**: `qtrue` if extension found, `qfalse` otherwise

### `Syscall_ExtensionAvailable()`

Check if an extension is available:

```c
qboolean Syscall_ExtensionAvailable( const char *key_name, vmIndex_t vm_index );
```

### `Syscall_GetExtensionNumber()`

Get syscall number directly:

```c
int Syscall_GetExtensionNumber( const char *key_name, vmIndex_t vm_index );
```

## Migration Path

### For New Extensions

1. **Add to registry**: Register in `syscall_registry.c` extension arrays
2. **Update VM_Ext**: Add to `ext_trap_keys_t` arrays for tracking
3. **Document**: Add description in registry entry

### For Legacy Extensions

Legacy hardcoded extensions in `SV_GetValue()`, `CL_GetValue()`, `UI_GetValue()` are maintained for compatibility but should be migrated to the registry.

## Benefits

1. **Centralized Management**: All extensions in one place
2. **Version Tracking**: Extension API versioning
3. **Documentation**: Built-in descriptions
4. **Maintainability**: Easier to add/modify extensions
5. **Compatibility**: Backward compatible with existing QVMs

## Future Improvements

1. **Syscall Metadata**: Add full syscall info (parameters, return types)
2. **Validation**: Runtime validation of syscall parameters
3. **Documentation Generation**: Auto-generate API docs from registry
4. **Version Checking**: Check QVM API version compatibility
5. **Deprecation System**: Mark deprecated syscalls with removal dates

## Related Files

- `src/qcommon/syscall_registry.h` - Registry header
- `src/qcommon/syscall_registry.c` - Registry implementation
- `src/qcommon/vm_ext.h` - Legacy extension system
- `src/server/sv_game.c` - Game module syscalls
- `src/client/cl_cgame.c` - CGame module syscalls
- `src/client/cl_ui.c` - UI module syscalls
- `docs/QVM_COMPATIBILITY.md` - QVM compatibility documentation

