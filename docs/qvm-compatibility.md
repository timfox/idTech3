# QVM Compatibility & Technical Debt

## Overview

This document tracks QVM (Quake Virtual Machine) compatibility concerns and technical debt related to maintaining backward compatibility with legacy QVM bytecode files.

## QVM System Architecture

The engine supports three execution modes for game modules:

1. **Native DLLs** (`VMI_NATIVE`) - Platform-specific shared libraries
2. **QVM Bytecode** (`VMI_BYTECODE`) - Interpreted bytecode
3. **Compiled QVM** (`VMI_COMPILED`) - JIT-compiled bytecode (x86/x64 only)

### VM Loading Priority

The engine attempts to load modules in this order:
1. Native DLL (`.so`, `.dll`, `.dylib`)
2. QVM bytecode (`.qvm`)
3. Falls back to QVM if native fails

**Key Point**: QVM support is **always maintained** as a fallback, ensuring legacy mods continue to work.

## Compatibility Guarantees

### ✅ Maintained Compatibility

- **Standard Syscalls**: All original Quake 3 syscalls remain unchanged
- **QVM Format**: Both `VM_MAGIC` and `VM_MAGIC_VER2` formats supported
- **Syscall Signatures**: Original function signatures preserved
- **Memory Layout**: VM data segment layout compatible
- **Stack Behavior**: Program stack behavior matches original

### ⚠️ Extension Syscalls (Q3E Extensions)

Some syscalls have been added that are **not** in standard Quake 3:

**Game Module Extensions:**
- `G_CVAR_SETDESCRIPTION` - Set CVar descriptions (Q3E extension)
- Various botlib extensions
- File system enhancements

**Detection:**
```c
// In sv_game.c - SV_GetValue()
if ( !Q_stricmp( key, "trap_Cvar_SetDescription_Q3E" ) )
{
    Com_sprintf( value, valueSize, "%i", G_CVAR_SETDESCRIPTION );
    return qtrue;
}
```

**Compatibility Strategy:**
- Extension syscalls are **optional** - legacy QVMs don't need them
- QVMs can query for extension availability via `SV_GetValue()` / `VM_Ext_GetKey()`
- Missing extensions fail gracefully (return 0 or no-op)

## Technical Debt Areas

### 1. Syscall Number Management

**Location**: `src/server/sv_game.c`, `src/client/cl_ui.c`, `src/client/cl_cgame.c`

**Issue**: Syscall numbers are hardcoded in switch statements. New syscalls must be added carefully to avoid conflicts.

**Debt:**
- No centralized syscall registry
- Syscall numbers not versioned
- Extension syscalls mixed with standard ones

**Recommendation:**
- Create `syscall_registry.h` with versioned syscall definitions
- Use feature detection rather than hardcoded numbers
- Document which syscalls are extensions vs. standard

### 2. VM Extension System

**Location**: `src/common/vm_ext.h`, `src/common/vm.c`

**Current State**: Basic extension system exists via `VM_Ext_GetKey()`

**Debt:**
- Extension system is underutilized
- No formal extension API versioning
- Extensions not documented for mod developers

**Recommendation:**
- Formalize extension API with version numbers
- Create extension registry
- Document extension availability per engine version

### 3. Native DLL Fallback Behavior

**Location**: `src/common/vm.c` - `VM_Create()`

**Current Behavior**: 
```c
// If QVM fails, try native DLL fallback
if ( header == NULL ) {
    vm->dllHandle = VM_LoadLib( name, &vm->entryPoint, dllSyscalls );
    // ...
}
```

**Debt:**
- Fallback order might confuse mod developers
- No clear error messages about why QVM failed
- Native DLLs bypass some QVM security checks

**Recommendation:**
- Add `com_vm_prefer` CVar (qvm/native/auto)
- Improve error messages
- Document security implications

### 4. Syscall Parameter Validation

**Location**: Throughout syscall handlers

**Current State**: Uses `VM_CHECKBOUNDS()` macro for bounds checking

**Debt:**
- Bounds checking is inconsistent
- Some syscalls don't validate all parameters
- Legacy QVMs might rely on lenient validation

**Example:**
```c
case G_FS_READ:
    if ( args[3] == 0 ) // UrT may pass this with args[2]=-1
        return 0;
    VM_CHECKBOUNDS( gvm, args[1], args[2] );
    return FS_VM_ReadFile( VMA(1), args[2], args[3], H_QAGAME );
```

**Recommendation:**
- Audit all syscalls for proper validation
- Add `com_vm_strict_bounds` CVar for strict mode
- Document which QVMs require lenient mode

### 5. VM Memory Management

**Location**: `src/common/vm.c` - `VM_LoadQVM()`

**Current State**: 
- Data segment allocation with guard pages
- Stack size: `PROGRAM_STACK_SIZE` (64KB) + `PROGRAM_STACK_EXTRA` (32KB)
- Power-of-2 rounding for mask operations

**Debt:**
- Stack size is hardcoded (some mods might need more)
- Guard size (`VM_DATA_GUARD_SIZE`) is fixed
- Memory layout assumptions in QVM bytecode

**Recommendation:**
- Add `com_vm_stack_size` CVar (with max limit)
- Document memory requirements
- Test with various legacy QVMs

### 6. Floating-Point Behavior

**Location**: `src/common/vm_interpreted.c`

**Current State**: IEEE 754 compliance improvements

**Debt:**
- Some legacy QVMs might rely on non-standard FP behavior
- NaN handling might differ from original
- FPU state preservation

**Recommendation:**
- Add `com_vm_fp_mode` CVar (strict/legacy)
- Test FP edge cases with legacy QVMs
- Document FP behavior differences

### 7. File System Restrictions

**Location**: `src/common/files.c` - `FS_VM_OpenFile()`

**Current State**: QVMs have restricted file access

**Debt:**
- Restrictions might break some legacy mods
- No way to whitelist specific files
- Error messages don't explain restrictions

**Recommendation:**
- Add `com_vm_fs_restrict` CVar
- Create whitelist system for trusted mods
- Improve error messages

## Testing Legacy QVM Compatibility

### Test Cases

1. **Standard Quake 3 QVMs**
   - `baseq3/vm/cgame.qvm`
   - `baseq3/vm/ui.qvm`
   - `baseq3/vm/qagame.qvm`

2. **Popular Mod QVMs**
   - Urban Terror
   - Defrag
   - OpenArena
   - Quake 3 Legacy

3. **Edge Cases**
   - QVMs with large data segments
   - QVMs using all syscalls
   - QVMs with custom opcodes (if any)

### Compatibility Checklist

- [ ] All standard syscalls work
- [ ] VM loading succeeds
- [ ] Memory allocation correct
- [ ] Stack operations work
- [ ] File I/O restrictions respected
- [ ] Floating-point math correct
- [ ] String operations work
- [ ] Entity management works
- [ ] Network operations work
- [ ] Botlib integration works

## Recommendations for New Features

### Do's ✅

1. **Add Extension Syscalls**: Use extension system, don't modify standard syscalls
2. **Feature Detection**: Query for extension availability before use
3. **Backward Compatible**: New features should be optional
4. **Document Extensions**: Clearly mark Q3E-specific features
5. **Test Legacy QVMs**: Verify compatibility with popular mods

### Don'ts ❌

1. **Don't Change Standard Syscalls**: Modify signatures or behavior
2. **Don't Remove Syscalls**: Even deprecated ones
3. **Don't Break VM Format**: Maintain QVM file format compatibility
4. **Don't Assume Extensions**: Always check availability
5. **Don't Skip Validation**: Always validate VM parameters

## Migration Path for Mod Developers

### For Legacy QVM Mods

1. **No Changes Required**: Legacy QVMs continue to work
2. **Optional Enhancements**: Can use extensions if available
3. **Feature Detection**: Check for extensions before use

### For New Mods

1. **Prefer Native**: Use native DLLs for better performance
2. **QVM Fallback**: Provide QVM for compatibility
3. **Extension Support**: Use extensions for enhanced features

## Monitoring & Maintenance

### Regular Tasks

1. **Test Legacy QVMs**: Monthly compatibility testing
2. **Review Syscalls**: Quarterly review of syscall usage
3. **Update Documentation**: Keep compatibility docs current
4. **Community Feedback**: Monitor mod developer reports

### Metrics to Track

- Number of QVM loads vs. native DLL loads
- Syscall usage frequency
- Extension adoption rate
- Compatibility issue reports

## Related Files

- `src/common/vm.c` - VM loading and management
- `src/common/vm_interpreted.c` - QVM interpreter
- `src/common/vm_ext.h` - Extension system
- `src/server/sv_game.c` - Game module syscalls
- `src/client/cl_cgame.c` - CGame module syscalls
- `src/client/cl_ui.c` - UI module syscalls

## References

- [QVM File Format](core/virtual-machine)
- [Syscall Documentation](api/syscalls)
- [Mod Development Guide](../mymod/gamesrc/README.md)

