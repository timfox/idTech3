/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2012-2020 Quake3e project

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// vm.c -- virtual machine

/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/

#include "vm_local.h"
#include "files_internal.h"
#include "crash_handler.h"
#include "files_validation.h"

#ifdef COMBINED_MONOLITH
	#ifndef _WIN32
		#include <dlfcn.h>  // For dlsym and RTLD_DEFAULT
	#else
		#include <windows.h>  // For GetModuleHandle and GetProcAddress
	#endif
#endif

opcode_info_t ops[ OP_MAX ] =
{
	// size, stack, nargs, flags
	{ .size = 0, .stack = 0, .nargs = 0, .flags = 0 }, // undef
	{ .size = 0, .stack = 0, .nargs = 0, .flags = 0 }, // ignore
	{ .size = 0, .stack = 0, .nargs = 0, .flags = 0 }, // break

	{ .size = 4, .stack = 0, .nargs = 0, .flags = 0 }, // enter
	{ .size = 4, .stack = -4, .nargs = 0, .flags = 0 }, // leave
	{ .size = 0, .stack = 0, .nargs = 1, .flags = 0 }, // call
	{ .size = 0, .stack = 4, .nargs = 0, .flags = 0 }, // push
	{ .size = 0, .stack = -4, .nargs = 1, .flags = 0 }, // pop

	{ .size = 4, .stack = 4, .nargs = 0, .flags = 0 }, // const
	{ .size = 4, .stack = 4, .nargs = 0, .flags = 0 }, // local
	{ .size = 0, .stack = -4, .nargs = 1, .flags = 0 }, // jump

	{ .size = 4, .stack = -8, .nargs = 2, .flags = JUMP }, // eq
	{ .size = 4, .stack = -8, .nargs = 2, .flags = JUMP }, // ne

	{ 4,-8, 2, JUMP }, // lti
	{ 4,-8, 2, JUMP }, // lei
	{ 4,-8, 2, JUMP }, // gti
	{ 4,-8, 2, JUMP }, // gei

	{ 4,-8, 2, JUMP }, // ltu
	{ 4,-8, 2, JUMP }, // leu
	{ 4,-8, 2, JUMP }, // gtu
	{ 4,-8, 2, JUMP }, // geu

	{ 4,-8, 2, JUMP|FPU }, // eqf
	{ 4,-8, 2, JUMP|FPU }, // nef

	{ 4,-8, 2, JUMP|FPU }, // ltf
	{ 4,-8, 2, JUMP|FPU }, // lef
	{ 4,-8, 2, JUMP|FPU }, // gtf
	{ 4,-8, 2, JUMP|FPU }, // gef

	{ 0, 0, 1, 0 }, // load1
	{ 0, 0, 1, 0 }, // load2
	{ 0, 0, 1, 0 }, // load4
	{ 0,-8, 2, 0 }, // store1
	{ 0,-8, 2, 0 }, // store2
	{ 0,-8, 2, 0 }, // store4
	{ 1,-4, 1, 0 }, // arg
	{ 4,-8, 2, 0 }, // bcopy

	{ 0, 0, 1, 0 }, // sex8
	{ 0, 0, 1, 0 }, // sex16

	{ 0, 0, 1, 0 }, // negi
	{ 0,-4, 3, 0 }, // add
	{ 0,-4, 3, 0 }, // sub
	{ 0,-4, 3, 0 }, // divi
	{ 0,-4, 3, 0 }, // divu
	{ 0,-4, 3, 0 }, // modi
	{ 0,-4, 3, 0 }, // modu
	{ 0,-4, 3, 0 }, // muli
	{ 0,-4, 3, 0 }, // mulu

	{ 0,-4, 3, 0 }, // band
	{ 0,-4, 3, 0 }, // bor
	{ 0,-4, 3, 0 }, // bxor
	{ 0, 0, 1, 0 }, // bcom

	{ 0,-4, 3, 0 }, // lsh
	{ 0,-4, 3, 0 }, // rshi
	{ 0,-4, 3, 0 }, // rshu

	{ 0, 0, 1, FPU }, // negf
	{ 0,-4, 3, FPU }, // addf
	{ 0,-4, 3, FPU }, // subf
	{ 0,-4, 3, FPU }, // divf
	{ 0,-4, 3, FPU }, // mulf

	{ 0, 0, 1, 0 },   // cvif
	{ 0, 0, 1, FPU }  // cvfi
};

const char *opname[ 256 ] = {
	"OP_UNDEF",

	"OP_IGNORE",

	"OP_BREAK",

	"OP_ENTER",
	"OP_LEAVE",
	"OP_CALL",
	"OP_PUSH",
	"OP_POP",

	"OP_CONST",

	"OP_LOCAL",

	"OP_JUMP",

	//-------------------

	"OP_EQ",
	"OP_NE",

	"OP_LTI",
	"OP_LEI",
	"OP_GTI",
	"OP_GEI",

	"OP_LTU",
	"OP_LEU",
	"OP_GTU",
	"OP_GEU",

	"OP_EQF",
	"OP_NEF",

	"OP_LTF",
	"OP_LEF",
	"OP_GTF",
	"OP_GEF",

	//-------------------

	"OP_LOAD1",
	"OP_LOAD2",
	"OP_LOAD4",
	"OP_STORE1",
	"OP_STORE2",
	"OP_STORE4",
	"OP_ARG",

	"OP_BLOCK_COPY",

	//-------------------

	"OP_SEX8",
	"OP_SEX16",

	"OP_NEGI",
	"OP_ADD",
	"OP_SUB",
	"OP_DIVI",
	"OP_DIVU",
	"OP_MODI",
	"OP_MODU",
	"OP_MULI",
	"OP_MULU",

	"OP_BAND",
	"OP_BOR",
	"OP_BXOR",
	"OP_BCOM",

	"OP_LSH",
	"OP_RSHI",
	"OP_RSHU",

	"OP_NEGF",
	"OP_ADDF",
	"OP_SUBF",
	"OP_DIVF",
	"OP_MULF",

	"OP_CVIF",
	"OP_CVFI"
};

cvar_t	*vm_rtChecks;
cvar_t	*vm_combined;

#ifdef DEBUG
int		vm_debugLevel;
#endif

// Monolithic build: function pointers for statically linked game modules
#ifdef COMBINED_MONOLITH
// Forward declarations for statically linked module entry points
// These will be resolved at link time from the static libraries
// Note: Must match QDECL calling convention
extern void QDECL dllEntry_game( dllSyscall_t syscallptr );
extern intptr_t QDECL vmMain_game( int command, int arg0, int arg1, int arg2 );

extern void QDECL dllEntry_cgame( dllSyscall_t syscallptr );
extern intptr_t QDECL vmMain_cgame( int command, int arg0, int arg1, int arg2 );

extern void QDECL dllEntry_ui( dllSyscall_t syscallptr );
extern intptr_t QDECL vmMain_ui( int command, int arg0, int arg1, int arg2 );

// Get entry points from statically linked modules using direct extern references
// Since modules are statically linked, we can reference them directly
static void *VM_GetCombinedEntryPoint( vmIndex_t index, vmMainFunc_t *entryPoint, dllSyscall_t dllSyscalls ) {
	const char *moduleName;
	dllEntry_t dllEntryFunc;
	
	// Map index to module-specific function pointers
	switch ( index ) {
		case VM_GAME:
			moduleName = "qagame";
			dllEntryFunc = dllEntry_game;
			*entryPoint = (vmMainFunc_t)vmMain_game;
			break;
		case VM_CGAME:
			moduleName = "cgame";
			dllEntryFunc = dllEntry_cgame;
			*entryPoint = (vmMainFunc_t)vmMain_cgame;
			break;
		case VM_UI:
			moduleName = "ui";
			dllEntryFunc = dllEntry_ui;
			*entryPoint = (vmMainFunc_t)vmMain_ui;
			break;
		default:
			return NULL;
	}
	
	if ( !dllEntryFunc || !*entryPoint ) {
		// Symbols not found - this shouldn't happen if modules are properly linked
		Com_Printf( "VM_GetCombinedEntryPoint: Failed to get function pointers for %s\n", moduleName );
		return NULL;
	}
	
	if ( com_developer && com_developer->integer ) {
		Com_Printf( "VM_GetCombinedEntryPoint(%s): dllSyscalls=%p entryPoint=%p\n",
			moduleName, (void*)dllSyscalls, (void*)*entryPoint );
	}
	
	// Call dllEntry to initialize the module
	dllEntryFunc( dllSyscalls );
	
	// Return a dummy handle (not NULL so VM knows it's loaded)
	return (void*)1;
}
#endif

// used by Com_Error to get rid of running vm's before longjmp
static int forced_unload;

// Type safety constants
#define VM_ERROR_BUFFER_SIZE 128
#define VM_LOCAL_ADDRESS_LIMIT 256
#define VM_CRC32_QAGAME 0x3E93FC1A
#define VM_INSTRUCTION_COUNT_QAGAME 123596
#define VM_DATA_LENGTH_QAGAME 2007536

struct vm_s vmTable[ VM_COUNT ];

const char *vmName[ VM_COUNT ] = {
	"qagame",
	"cgame",
	"ui"
};

static void VM_VmInfo_f( void );
static void VM_VmProfile_f( void );

#ifdef DEBUG
void VM_Debug( int level ) {
	vm_debugLevel = level;
}
#endif

/*
==============
VM_CheckBounds
==============
*/
void VM_CheckBounds( const vm_t *vm, unsigned int address, unsigned int length )
{
	//if ( !vm->entryPoint )
	{
		if ( (address | length) > vm->dataMask || (address + length) > vm->dataMask )
		{
			Com_Error( ERR_DROP, "program tried to bypass data segment bounds" );
		}
	}
}


/*
==============
VM_CheckBounds2
==============
*/
void VM_CheckBounds2( const vm_t *vm, unsigned int addr1, unsigned int addr2, unsigned int length )
{
	//if ( !vm->entryPoint )
	{
		if ( (addr1 | addr2 | length) > vm->dataMask || (addr1 + length) > vm->dataMask || (addr2+length) > vm->dataMask )
		{
			Com_Error( ERR_DROP, "program tried to bypass data segment bounds" );
		}
	}
}


/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
#ifndef DEDICATED
	Cvar_Get( "vm_ui", "2", CVAR_ARCHIVE | CVAR_PROTECTED );	// !@# SHIP WITH SET TO 2
	Cvar_Get( "vm_cgame", "2", CVAR_ARCHIVE | CVAR_PROTECTED );	// !@# SHIP WITH SET TO 2
#endif
	Cvar_Get( "vm_game", "2", CVAR_ARCHIVE | CVAR_PROTECTED );	// !@# SHIP WITH SET TO 2

	// Monolithic build detection
	#ifdef COMBINED_MONOLITH
		// Allow command-line override; keep INIT so it is set during startup.
		vm_combined = Cvar_Get( "vm_combined", "1", CVAR_INIT );
		Cvar_SetDescription( vm_combined, "Indicates that game modules are statically linked into the executable (monolithic build)" );
	#else
		vm_combined = Cvar_Get( "vm_combined", "0", CVAR_INIT | CVAR_ROM | CVAR_PROTECTED );
		Cvar_SetDescription( vm_combined, "Indicates that game modules are statically linked into the executable (monolithic build)" );
	#endif

	Cmd_AddCommand( "vmprofile", VM_VmProfile_f );
	Cmd_AddCommand( "vminfo", VM_VmInfo_f );

	Com_Memset( vmTable, 0, sizeof( vmTable ) );
}


/*
================
VM_Error

Emit a user-friendly error when a VM fails to load.
================
*/
void VM_Error( errorParm_t errorParm, vmIndex_t index ) {
	const char *module;
	char qvmPath[ MAX_QPATH ];
	char dllName[ MAX_QPATH ];

	if ( index < 0 || index >= VM_COUNT ) {
		Com_Error( errorParm, "VM_Create failed for invalid VM index %d", index );
	}

	module = vmName[ index ];

	Com_sprintf( qvmPath, sizeof( qvmPath ), "vm/%s.qvm", module );
	Com_sprintf( dllName, sizeof( dllName ), "%s." ARCH_STRING DLL_EXT, module );

	Com_Error(
		errorParm,
		"VM_Create on %s failed\n\n"
		"Checked for QVM '%s' and native module '%s' in the current mod folder.\n"
		"Ensure at least one of these exists and is built for your platform.",
		module,
		qvmPath,
		dllName
	);
}


/*
================
VM_SelectInterpret

Resolve the effective vmInterpret_t for a VM, honoring pure server
restrictions and clamping to the valid enum range.
================
*/
vmInterpret_t VM_SelectInterpret( const char *cvarName, vmInterpret_t requested, qboolean requireQvmOnly ) {
	int value = requested;

	// If a cvar name is provided, let it override the explicit value
	if ( cvarName && cvarName[0] ) {
		value = Cvar_VariableIntegerValue( cvarName );
	}

	// Clamp to known range
	if ( value < VMI_NATIVE ) {
		value = VMI_NATIVE;
	} else if ( value > VMI_BYTECODE ) {
		value = VMI_BYTECODE;
	}

	// On pure servers we only allow QVMs (compiled or bytecode)
	if ( requireQvmOnly && value == VMI_NATIVE ) {
		value = VMI_COMPILED;
	}

	return (vmInterpret_t)value;
}


/*
===============
VM_ValueToSymbol

Assumes a program counter value
===============
*/
const char *VM_ValueToSymbol( vm_t *vm, int value ) {
	vmSymbol_t	*sym;
	static char		text[MAX_TOKEN_CHARS];

	sym = vm->symbols;
	if ( !sym ) {
		return "NO SYMBOLS";
	}

	// find the symbol
	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	if ( value == sym->symValue ) {
		return sym->symName;
	}

	Com_sprintf( text, sizeof( text ), "%s+%i", sym->symName, value - sym->symValue );

	return text;
}


/*
===============
VM_ValueToFunctionSymbol

For profiling, find the symbol behind this value
===============
*/
vmSymbol_t *VM_ValueToFunctionSymbol( vm_t *vm, int value ) {
	vmSymbol_t	*sym;
	static vmSymbol_t	nullSym;

	sym = vm->symbols;
	if ( !sym ) {
		return &nullSym;
	}

	while ( sym->next && sym->next->symValue <= value ) {
		sym = sym->next;
	}

	return sym;
}


/*
===============
VM_SymbolToValue
===============
*/
int VM_SymbolToValue( vm_t *vm, const char *symbol ) {
	vmSymbol_t	*sym;

	for ( sym = vm->symbols ; sym ; sym = sym->next ) {
		if ( !strcmp( symbol, sym->symName ) ) {
			return sym->symValue;
		}
	}
	return 0;
}


/*
=====================
VM_SymbolForCompiledPointer
=====================
*/
#if 0 // 64bit!
const char *VM_SymbolForCompiledPointer( vm_t *vm, void *code ) {
	int			i;

	if ( code < (void *)vm->codeBase.ptr ) {
		return "Before code block";
	}
	if ( code >= (void *)(vm->codeBase.ptr + vm->codeLength) ) {
		return "After code block";
	}

	// find which original instruction it is after
	for ( i = 0 ; i < vm->codeLength ; i++ ) {
		if ( (void *)vm->instructionPointers[i] > code ) {
			break;
		}
	}
	i--;

	// now look up the bytecode instruction pointer
	return VM_ValueToSymbol( vm, i );
}
#endif


/*
===============
ParseHex
===============
*/
static int	ParseHex( const char *text ) {
	int		value;
	int		c;

	value = 0;
	while ( ( c = *text++ ) != 0 ) {
		if ( c >= '0' && c <= '9' ) {
			value = value * 16 + c - '0';
			continue;
		}
		if ( c >= 'a' && c <= 'f' ) {
			value = value * 16 + 10 + c - 'a';
			continue;
		}
		if ( c >= 'A' && c <= 'F' ) {
			value = value * 16 + 10 + c - 'A';
			continue;
		}
	}

	return value;
}


/*
===============
VM_LoadSymbols
===============
*/
static void VM_LoadSymbols( vm_t *vm ) {
	union {
		char	*c;
		void	*v;
	} mapfile;
	const char *text_p, *token;
	char	name[MAX_QPATH];
	char	symbols[MAX_QPATH];
	vmSymbol_t	**prev, *sym;
	int		count;
	int		value;
	int		chars;
	int		segment;
	int		numInstructions;

	// don't load symbols if not developer
	if ( !com_developer->integer ) {
		return;
	}

	COM_StripExtension(vm->name, name, sizeof(name));
	Com_sprintf( symbols, sizeof( symbols ), "vm/%s.map", name );
	FS_ReadFile( symbols, &mapfile.v );
	if ( !mapfile.c ) {
		Com_Printf( "Couldn't load symbol file: %s\n", symbols );
		return;
	}

	numInstructions = vm->instructionCount;

	// parse the symbols
	text_p = mapfile.c;
	prev = &vm->symbols;
	count = 0;

	while ( 1 ) {
		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			break;
		}
		segment = ParseHex( token );
		if ( segment ) {
			COM_Parse( &text_p );
			COM_Parse( &text_p );
			continue;		// only load code segment values
		}

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		value = ParseHex( token );

		token = COM_Parse( &text_p );
		if ( !token[0] ) {
			Com_Printf( "WARNING: incomplete line at end of file\n" );
			break;
		}
		chars = strlen( token );
		sym = Hunk_Alloc( sizeof( *sym ) + chars, h_high );
		*prev = sym;
		prev = &sym->next;
		sym->next = NULL;

		// convert value from an instruction number to a code offset
		if ( vm->instructionPointers && value >= 0 && value < numInstructions ) {
			value = vm->instructionPointers[value];
		}

		sym->symValue = value;
		Q_strncpyz( sym->symName, token, chars + 1 );

		count++;
	}

	vm->numSymbols = count;
	Com_Printf( "%i symbols parsed from %s\n", count, symbols );
	FS_FreeFile( mapfile.v );
}


/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get its args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.

============
*/
#if 0 // - disabled because now is different for each module
intptr_t QDECL VM_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
  // rcg010206 - see commentary above
  intptr_t	args[16];
  va_list	ap;
  int i;

  args[0] = arg;

  va_start( ap, arg );
  for (i = 1; i < ARRAY_LEN( args ); i++ )
    args[ i ] = va_arg( ap, intptr_t );
  va_end( ap );

  return currentVM->systemCall( args );
#else // original id code
	return currentVM->systemCall( &arg );
#endif
}
#endif


static void VM_SwapLongs( void *data, int length )
{
	(void)data;    // Suppress unused parameter warning
	(void)length;  // Suppress unused parameter warning
#ifndef Q3_LITTLE_ENDIAN
	int32_t *ptr;
	int i;
	ptr = (int32_t *) data;
	length /= sizeof( int32_t );
	for ( i = 0; i < length; i++ ) {
		ptr[ i ] = LittleLong( ptr[ i ] );
	}
#endif
}


static int Load_JTS( vm_t *vm, uint32_t crc32, void *data, int vmPakIndex ) {
	char		filename[MAX_QPATH];
	int			header[2];
	int			length;
	fileHandle_t fh;

	// load the image
	Com_sprintf( filename, sizeof(filename), "vm/%s.jts", vm->name );
	if ( data )
		Com_Printf( "Loading jts file %s...\n", filename );

	length = FS_FOpenFileRead( filename, &fh, qtrue );

	if ( fh == FS_INVALID_HANDLE ) {
		if ( data )
			Com_Printf( " not found.\n" );
		return -1;
	}

	if ( fs_lastPakIndex != vmPakIndex ) {
		Com_DPrintf( " invalid pak index %i (expecting %i) for %s.\n", fs_lastPakIndex, vmPakIndex, filename );
		FS_FCloseFile( fh );
		return -1;
	}

	if ( (size_t)length < sizeof( header ) ) {
		if ( data )
			Com_Printf( " bad filesize %i for %s.\n", length, filename );
		FS_FCloseFile( fh );
		return -1;
	}

	if ( FS_Read( header, sizeof( header ), fh ) != (int)sizeof( header ) ) {
		if ( data )
			Com_Printf( " error reading header of %s.\n", filename );
		FS_FCloseFile( fh );
		return -1;
	}

	// byte swap the header
	VM_SwapLongs( header, sizeof( header  ) );

	if ( (unsigned int)header[0] != crc32 ) {
		if ( data )
			Com_Printf( " crc32 mismatch: %08X <-> %08X.\n", header[0], crc32 );
		FS_FCloseFile( fh );
		return -1;
	}

	if ( header[1] < 0 || header[1] != (length - (int)sizeof( header ) ) ) {
		if ( data )
			Com_Printf( " bad file header.\n" );
		FS_FCloseFile( fh );
		return -1;
	}

	length -= sizeof( header ); // skip header and filesize

	// we need just filesize
	if ( !data ) {
		FS_FCloseFile( fh );
		return length;
	}

	FS_Read( data, length, fh );
	FS_FCloseFile( fh );

	// byte swap the data
	VM_SwapLongs( data, length );

	return length;
}


/*
=================
VM_ValidateHeader
=================
*/
static char *VM_ValidateHeader( vmHeader_t *header, int fileSize )
{
	static char errMsg[VM_ERROR_BUFFER_SIZE];
	int n;
	size_t fileSizeSz = (size_t)fileSize;
	
	// truncated
	if ( fileSizeSz < ( sizeof( vmHeader_t ) - sizeof( int32_t ) ) ) {
		Com_sprintf( errMsg, sizeof( errMsg ), "truncated image header (%i bytes long)", fileSize );
		return errMsg;
	}
	
	// bad magic
	if ( LittleLong( header->vmMagic ) != VM_MAGIC && LittleLong( header->vmMagic ) != VM_MAGIC_VER2 ) {
		Com_sprintf( errMsg, sizeof( errMsg ), "bad file magic %08x", LittleLong( header->vmMagic ) );
		return errMsg;
	}
	
	// truncated
	if ( fileSizeSz < sizeof( vmHeader_t ) && LittleLong( header->vmMagic ) != VM_MAGIC_VER2 ) {
		Com_sprintf( errMsg, sizeof( errMsg ), "truncated image header (%i bytes long)", fileSize );
		return errMsg;
	}
	
	if ( LittleLong( header->vmMagic ) == VM_MAGIC_VER2 )
		n = sizeof( vmHeader_t );
	else
		n = ( sizeof( vmHeader_t ) - sizeof( int32_t ) );
	
	// byte swap the header
	VM_SwapLongs( header, n );
	
	// bad code offset
	if ( (size_t)header->codeOffset >= fileSizeSz ) {
		Com_sprintf( errMsg, sizeof( errMsg ), "bad code segment offset %i", header->codeOffset );
		return errMsg;
	}
	
	// bad code length
	if ( header->codeLength <= 0 || (size_t)(header->codeOffset + header->codeLength) > fileSizeSz ) {
		Com_sprintf( errMsg, sizeof( errMsg ), "bad code segment length %i", header->codeLength );
		return errMsg;
	}
	
	// bad data offset
	if ( (size_t)header->dataOffset >= fileSizeSz || header->dataOffset != header->codeOffset + header->codeLength ) {
		Com_sprintf( errMsg, sizeof( errMsg ), "bad data segment offset %i", header->dataOffset );
		return errMsg;
	}
	
	// bad data length
	if ( header->dataOffset + header->dataLength > fileSize )  {
		Com_sprintf( errMsg, sizeof( errMsg ), "bad data segment length %i", header->dataLength );
		return errMsg;
	}
	
	if ( header->vmMagic == VM_MAGIC_VER2 ) {
		// bad lit/jtrg length
		if ( header->dataOffset + header->dataLength + header->litLength + header->jtrgLength != fileSize ) {
			Com_sprintf( errMsg, sizeof( errMsg ), "bad lit/jtrg segment length" );
			return errMsg;
		}
	}
	// bad lit length
	else if ( header->dataOffset + header->dataLength + header->litLength != fileSize ) {
		Com_sprintf( errMsg, sizeof( errMsg ), "bad lit segment length %i", header->litLength );
		return errMsg;
	}
	
	return NULL;
}


/*
=================
VM_LoadQVM

Load a .qvm file

if ( alloc )
 - Validate header, swap data
 - Alloc memory for data/instructions
 - Alloc memory for instructionPointers - NOT NEEDED
 - Load instructions
 - Clear/load data
else
 - Check for header changes
 - Clear/load data

=================
*/
static vmHeader_t *VM_LoadQVM( vm_t *vm, qboolean alloc ) {
	int					length;
	unsigned int		dataLength;
	unsigned int		dataAlloc;
	char				filename[MAX_QPATH], *errorMsg;
	unsigned int		crc32sum;
	qboolean			tryjts;
	vmHeader_t			*header;
	int					vmPakIndex;

	// load the image
	Com_sprintf( filename, sizeof(filename), "vm/%s.qvm", vm->name );
	Com_Printf( "Loading vm file %s...\n", filename );
	length = FS_ReadFile( filename, (void **)&header );
	if ( !header ) {
		Com_Printf( "Failed.\n" );
		VM_Free( vm );
		return NULL;
	}

	vmPakIndex = fs_lastPakIndex;

	crc32sum = crc32_buffer( (const byte*) header, length );

	// will also swap header
	errorMsg = VM_ValidateHeader( header, length );
	if ( errorMsg ) {
		VM_Free( vm );
		FS_FreeFile( header );
		Com_Printf( S_COLOR_RED "%s\n", errorMsg );
		return NULL;
	}

	vm->crc32sum = crc32sum;
	tryjts = qfalse;

	if( header->vmMagic == VM_MAGIC_VER2 ) {
		Com_Printf( "...which has vmMagic VM_MAGIC_VER2\n" );
	} else {
		tryjts = qtrue;
	}

	vm->exactDataLength = header->dataLength + header->litLength + header->bssLength;

	dataLength = vm->exactDataLength;
	if ( dataLength < PROGRAM_STACK_SIZE ) {
		dataLength = PROGRAM_STACK_SIZE;
	}

	vm->programStackExtra = PROGRAM_STACK_EXTRA;

	// if rounding difference is larger than extra space we need then reuse it
	if ( log2pad( dataLength, 1 ) - dataLength >= PROGRAM_STACK_EXTRA ) {
#ifdef _DEBUG
		// keep exact size for debug purposes
#else
		// reuse it all for release builds
		vm->programStackExtra = log2pad( dataLength, 1 ) - dataLength;
		// Com_DPrintf( S_COLOR_CYAN "%s: reuse %i bytes for pStack\n", vm->name, vm->programStackExtra );
#endif
	} else {
		dataLength += vm->programStackExtra;
	}

	vm->dataLength = dataLength;

	// round up to next power of 2 so all data operations can be mask protected
	dataLength = log2pad( dataLength, 1 );

	// reserve some space for effective LOCAL+LOAD* checks
	dataAlloc = dataLength + VM_DATA_GUARD_SIZE;

	if ( dataLength >= (1U<<31) || dataAlloc >= (1U<<31) ) {
		// dataLenth is negative int32
		VM_Free( vm );
		FS_FreeFile( header );
		Com_Printf( S_COLOR_RED "%s: data segment is too large\n", __func__ );
		return NULL;
	}

	if ( alloc ) {
		// allocate zero filled space for initialized and uninitialized data
		vm->dataBase = Hunk_Alloc( dataAlloc, h_high );
		vm->dataMask = dataLength - 1;
		vm->dataAlloc = dataAlloc;
	} else {
		// clear the data, but make sure we're not clearing more than allocated
		if ( vm->dataAlloc != dataAlloc ) {
			VM_Free( vm );
			FS_FreeFile( header );
			Com_Printf( S_COLOR_YELLOW "Warning: Data region size of %s not matching after"
					"VM_Restart()\n", filename );
			return NULL;
		}
		Com_Memset( vm->dataBase, 0x0, vm->dataAlloc );
	}

	// copy the intialized data
	Com_Memcpy( vm->dataBase, (byte *)header + header->dataOffset, header->dataLength + header->litLength );

	// byte swap the longs
	VM_SwapLongs( vm->dataBase, header->dataLength );

	if( header->vmMagic == VM_MAGIC_VER2 ) {
		int previousNumJumpTableTargets = vm->numJumpTableTargets;

		header->jtrgLength &= ~0x03;

		vm->numJumpTableTargets = header->jtrgLength >> 2;
		Com_Printf( "Loading %d jump table targets\n", vm->numJumpTableTargets );

		if ( alloc ) {
			vm->jumpTableTargets = (int32_t *) Hunk_Alloc( header->jtrgLength, h_high );
		} else {
			if ( vm->numJumpTableTargets != previousNumJumpTableTargets ) {
				VM_Free( vm );
				FS_FreeFile( header );

				Com_Printf( S_COLOR_YELLOW "Warning: Jump table size of %s not matching after "
					"VM_Restart()\n", filename );
				return NULL;
			}

			Com_Memset( vm->jumpTableTargets, 0, header->jtrgLength );
		}

		Com_Memcpy( vm->jumpTableTargets, (byte *)header + header->dataOffset +
				header->dataLength + header->litLength, header->jtrgLength );

		// byte swap the longs
		VM_SwapLongs( vm->jumpTableTargets, header->jtrgLength );
	}

	if ( tryjts == qtrue && (length = Load_JTS( vm, crc32sum, NULL, vmPakIndex )) >= 0 ) {
		// we are trying to load newer file?
		if ( vm->jumpTableTargets && vm->numJumpTableTargets != length >> 2 ) {
			Com_Printf( S_COLOR_YELLOW "Reload jts file\n" );
			vm->jumpTableTargets = NULL;
			alloc = qtrue;
		}
		vm->numJumpTableTargets = length >> 2;
		Com_Printf( "Loading %d external jump table targets\n", vm->numJumpTableTargets );
		if ( alloc == qtrue ) {
			vm->jumpTableTargets = (int32_t *) Hunk_Alloc( length, h_high );
		} else {
			Com_Memset( vm->jumpTableTargets, 0, length );
		}
		Load_JTS( vm, crc32sum, vm->jumpTableTargets, vmPakIndex );
	}

	return header;
}


static void VM_IgnoreInstructions( instruction_t *buf, const int count ) {
	int i;

	for ( i = 0; i < count; i++ ) {
		Com_Memset( buf + i, 0, sizeof( *buf ) );
		buf[i].op = OP_IGNORE;
	}

	buf[0].value = count > 0 ? count - 1 : 0;
}


static int InvertCondition( int op )
{
	switch ( op ) {
		case OP_EQ: return OP_NE;   // == -> !=
		case OP_NE: return OP_EQ;   // != -> ==

		case OP_LTI: return OP_GEI;	// <  -> >=
		case OP_LEI: return OP_GTI;	// <= -> >
		case OP_GTI: return OP_LEI; // >  -> <=
		case OP_GEI: return OP_LTI; // >= -> <

		case OP_LTU: return OP_GEU;
		case OP_LEU: return OP_GTU;
		case OP_GTU: return OP_LEU;
		case OP_GEU: return OP_LTU;

		case OP_EQF: return OP_NEF;
		case OP_NEF: return OP_EQF;

		case OP_LTF: return OP_GEF;
		case OP_LEF: return OP_GTF;
		case OP_GTF: return OP_LEF;
		case OP_GEF: return OP_LTF;

		default: 
			Com_Error( ERR_DROP, "incorrect condition opcode %i", op );
			return op;
	}
}


/*
=================
VM_FindLocal

search for specified local variable until end of function
=================
*/
static qboolean VM_FindLocal( int32_t addr, const instruction_t *buf, const instruction_t *end, int32_t *back_addr ) {
	int32_t curr_addr = *back_addr;
	while ( buf < end ) {
		if ( buf->op == OP_LOCAL ) {
			if ( buf->value == addr ) {
				return qtrue;
			}
			++buf; continue;
		}
		if ( ops[ buf->op ].flags & JUMP ) {
			if ( buf->value < curr_addr ) {
				curr_addr = buf->value;
			}
			++buf; continue;
		}
		if ( buf->op == OP_JUMP ) {
			if ( buf->value && buf->value < curr_addr ) {
				curr_addr = buf->value;
			}
			++buf; continue;
		}
		if ( buf->op == OP_PUSH && (buf+1)->op == OP_LEAVE ) {
			break;
		}
		++buf;
	}
	*back_addr = curr_addr;
	return qfalse;
}


/*
=================
VM_Fixup

Do some corrections to fix known Q3LCC flaws
=================
*/
static void VM_Fixup( instruction_t *buf, int instructionCount )
{
	int n;
	instruction_t *i;

	i = buf;
	n = 0;

	while ( n < instructionCount )
	{
		if ( i->op == OP_LOCAL ) {

			// skip useless sequences
			if ( (i+1)->op == OP_LOCAL && (i+0)->value == (i+1)->value && (i+2)->op == OP_LOAD4 && (i+3)->op == OP_STORE4 ) {
				VM_IgnoreInstructions( i, 4 );
				i += 4; n += 4;
				continue;
			}

			// [0]OP_LOCAL + [1]OP_CONST + [2]OP_CALL + [3]OP_STORE4
			if ( (i+1)->op == OP_CONST && (i+2)->op == OP_CALL && (i+3)->op == OP_STORE4 && !(i+4)->jused ) {
				// [4]OP_CONST|OP_LOCAL (dest) + [5]OP_LOCAL(temp) + [6]OP_LOAD4 + [7]OP_STORE4
				if ( (i+4)->op == OP_CONST || (i+4)->op == OP_LOCAL ) {
					if ( (i+5)->op == OP_LOCAL && (i+5)->value == (i+0)->value && (i+6)->op == OP_LOAD4 && (i+7)->op == OP_STORE4 ) {
						int32_t back_addr = n;
						int32_t curr_addr = n;
						qboolean do_break = qfalse;

						// make sure that address of (potentially) temporary variable is not referenced further in this function
						if ( VM_FindLocal( i->value, i + 8, buf + instructionCount, &back_addr ) ) {
							i++; n++;
							continue;
						}

						// we have backward jumps in code then check for references before current position
						while ( back_addr < curr_addr ) {
							curr_addr = back_addr;
							if ( VM_FindLocal( i->value, buf + back_addr, i, &back_addr ) ) {
								do_break = qtrue;
								break;
							}
						}
						if ( do_break ) {
							i++; n++;
							continue;
						}

						(i+0)->op = (i+4)->op;
						(i+0)->value = (i+4)->value;
						VM_IgnoreInstructions( i + 4, 4 );
						i += 8;
						n += 8;
						continue;
					}
				}
			}
		}

		if ( i->op == OP_LEAVE && !i->endp ) {
			if ( !(i+1)->jused && (i+1)->op == OP_CONST && (i+2)->op == OP_JUMP ) {
				int v = (i+1)->value;
				if ( buf[ v ].op == OP_PUSH && buf[ v+1 ].op == OP_LEAVE && buf[ v+1 ].endp ) {
					VM_IgnoreInstructions( i + 1, 2 );
					i += 3;
					n += 3;
					continue;
				}
			}
		}

		//n + 0: if ( cond ) goto label1;
		//n + 2: goto label2;
		//n + 3: label1:
		// ...
		//n + x: label2:
		if ( ( ops[i->op].flags & (JUMP | FPU) ) == JUMP && !(i+1)->jused && (i+1)->op == OP_CONST && (i+2)->op == OP_JUMP ) {
			if ( i->value == n + 3 && (i+1)->value >= n + 3 ) {
				i->op = InvertCondition( i->op );
				i->value = ( i + 1 )->value;
				VM_IgnoreInstructions( i + 1, 2 );
				i += 3;
				n += 3;
				continue;
			}
		}
		i++;
		n++;
	}
}


/*
=================
VM_LoadInstructions

loads instructions in structured format
=================
*/
const char *VM_LoadInstructions( const byte *code_pos, int codeLength, int instructionCount, instruction_t *buf )
{
	static char errBuf[ 128 ];
	const byte *code_start, *code_end;
	int i, n, op0, op1, opStack;
	instruction_t *ci;

	code_start = code_pos; // for printing
	code_end = code_pos + codeLength;

	ci = buf;
	opStack = 0;
	op1 = OP_UNDEF;

	// load instructions and perform some initial calculations/checks
	for ( i = 0; i < instructionCount; i++, ci++, op1 = op0 ) {
		op0 = *code_pos;
		if ( op0 < 0 || op0 >= OP_MAX ) {
			Q_snprintf( errBuf, sizeof(errBuf), "bad opcode %02X at offset %d", op0, (int)(code_pos - code_start) );
			return errBuf;
		}
		n = ops[ op0 ].size;
		if ( code_pos + 1 + n  > code_end ) {
			Q_snprintf( errBuf, sizeof(errBuf), "code_pos > code_end" );
			return errBuf;
		}
		code_pos++;
		ci->op = op0;
		if ( n == 4 ) {
			ci->value = LittleLong( *((int32_t*)code_pos) );
			code_pos += 4;
		} else if ( n == 1 ) {
			ci->value = *((unsigned char*)code_pos);
			code_pos += 1;
		} else {
			ci->value = 0;
		}

		if ( ops[ op0 ].flags & FPU ) {
			ci->fpu = 1;
		}

		// setup jump value from previous const
		if ( op0 == OP_JUMP && op1 == OP_CONST ) {
			ci->value = (ci-1)->value;
		}

		ci->opStack = opStack;
		opStack += ops[ op0 ].stack;
	}

	return NULL;
}


static qboolean safe_address( instruction_t *ci, instruction_t *proc, int dataLength )
{
	if ( ci->op == OP_LOCAL ) {
		// local address can't exceed programStack frame plus VM_LOCAL_ADDRESS_LIMIT bytes of passed arguments
		if ( ci->value < 8 || ( proc && ci->value >= proc->value + VM_LOCAL_ADDRESS_LIMIT ) )
			return qfalse;
		return qtrue;
	}

	if ( ci->op == OP_CONST ) {
		// constant address can't exceed data segment
		if ( ci->value >= dataLength || ci->value < 0 )
			return qfalse;
		return qtrue;
	}

	return qfalse;
}


/*
===============================
VM_CheckInstructions

performs additional consistency and security checks
===============================
*/
const char *VM_CheckInstructions( instruction_t *buf,
								int instructionCount,
								const int32_t *jumpTableTargets,
								int numJumpTableTargets,
								int dataLength )
{
	static char errBuf[ VM_ERROR_BUFFER_SIZE ];
	instruction_t *opStackPtr[ PROC_OPSTACK_SIZE ];
	int i, m, n, v, op0, op1, opStack, pstack;
	instruction_t *ci, *proc;
	int startp, endp;
	int safe_stores;
	int unsafe_stores;

	ci = buf;
	opStack = 0;

	// opstack checks
	for ( i = 0; i < instructionCount; i++, ci++ ) {
		opStack += ops[ ci->op ].stack;
		if ( opStack < 0 ) {
			Q_snprintf( errBuf, sizeof(errBuf), "opStack underflow at %i", i );
			return errBuf;
		}
		if ( opStack >= PROC_OPSTACK_SIZE * 4 ) {
			Q_snprintf( errBuf, sizeof(errBuf), "opStack overflow at %i", i );
			return errBuf;
		}
	}

	ci = buf;
	pstack = 0;
	opStack = 0;
	safe_stores = 0;
	unsafe_stores = 0;
	op1 = OP_UNDEF;
	proc = NULL;
	Com_Memset( opStackPtr, 0, sizeof( opStackPtr ) );

	startp = 0;
	endp = instructionCount - 1;

	// Additional security checks

	for ( i = 0; i < instructionCount; i++, ci++, op1 = op0 ) {
		op0 = ci->op;

		m = ops[ ci->op ].stack;
		opStack += m;
		if ( m >= 0 ) {
			// do some FPU type promotion for more efficient loads
			if ( ci->fpu && ci->op != OP_CVIF ) {
				opStackPtr[ opStack / 4 ]->fpu = 1;
			}
			opStackPtr[ opStack >> 2 ] = ci;
		} else {
			if ( ci->fpu ) {
				if ( m <= -8 ) {
					opStackPtr[ opStack / 4 + 1 ]->fpu = 1;
					opStackPtr[ opStack / 4 + 2 ]->fpu = 1;
				} else {
					opStackPtr[ opStack / 4 + 0 ]->fpu = 1;
					opStackPtr[ opStack / 4 + 1 ]->fpu = 1;
				}
			} else {
				if ( m <= -8 ) {
					//
				} else {
					opStackPtr[ opStack / 4 + 0 ] = ci;
				}
			}
		}

		// function entry
		if ( op0 == OP_ENTER ) {
			// missing block end
			if ( proc || ( pstack && op1 != OP_LEAVE ) ) {
				Q_snprintf( errBuf, sizeof(errBuf), "missing proc end before %i", i );
				return errBuf;
			}
			if ( ci->opStack != 0 ) {
				v = ci->opStack;
				Q_snprintf( errBuf, sizeof(errBuf), "bad entry opstack %i at %i", v, i );
				return errBuf;
			}
			v = ci->value;
			if ( v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3) ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad entry programStack %i at %i", v, i );
				return errBuf;
			}

			pstack = ci->value;

			// mark jump target
			ci->jused = 1;
			proc = ci;
			startp = i + 1;

			// locate endproc
			for ( endp = 0, n = i+1 ; n < instructionCount; n++ ) {
				if ( buf[n].op == OP_PUSH && buf[n+1].op == OP_LEAVE ) {
					buf[n+1].endp = 1;
					endp = n;
					break;
				}
			}

			if ( endp == 0 ) {
				Q_snprintf( errBuf, sizeof(errBuf), "missing end proc for %i", i );
				return errBuf;
			}

			continue;
		}

		// proc opstack will carry max.possible opstack value
		if ( proc && ci->opStack > proc->opStack )
			proc->opStack = ci->opStack;

		// function return
		if ( op0 == OP_LEAVE ) {
			// bad return programStack
			if ( pstack != ci->value ) {
				v = ci->value;
				Q_snprintf( errBuf, sizeof(errBuf), "bad programStack %i at %i", v, i );
				return errBuf;
			}
			// bad opStack before return
			if ( ci->opStack != 4 ) {
				v = ci->opStack;
				Q_snprintf( errBuf, sizeof(errBuf), "bad opStack %i at %i", v, i );
				return errBuf;
			}
			v = ci->value;
			if ( v < 0 || v >= PROGRAM_STACK_SIZE || (v & 3) ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad return programStack %i at %i", v, i );
				return errBuf;
			}
			if ( op1 == OP_PUSH ) {
				if ( proc == NULL ) {
					Q_snprintf( errBuf, sizeof(errBuf), "unexpected proc end at %i", i );
					return errBuf;
				}
				proc = NULL;
				startp = i + 1; // next instruction
				endp = instructionCount - 1; // end of the image
			}
			continue;
		}

		// conditional jumps
		if ( ops[ ci->op ].flags & JUMP ) {
			v = ci->value;
			// conditional jumps should have opStack >= 8
			if ( ci->opStack < 8 ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad jump opStack %i at %i", ci->opStack, i );
				return errBuf;
			}
			//if ( v >= header->instructionCount ) {
			// allow only local proc jumps
			if ( v < startp || v > endp ) {
				Q_snprintf( errBuf, sizeof(errBuf), "jump target %i at %i is out of range (%i,%i)", v, i-1, startp, endp );
				return errBuf;
			}
			if ( buf[v].opStack != ci->opStack - 8 ) {
				n = buf[v].opStack;
				Q_snprintf( errBuf, sizeof(errBuf), "jump target %i has bad opStack %i", v, n );
				return errBuf;
			}
			// mark jump target
			buf[v].jused = 1;
			continue;
		}

		// unconditional jumps
		if ( op0 == OP_JUMP ) {
			// jumps should have opStack >= 4
			if ( ci->opStack < 4 ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad jump opStack %i at %i", ci->opStack, i );
				return errBuf;
			}
			if ( op1 == OP_CONST ) {
				v = buf[i-1].value;
				// allow only local jumps
				if ( v < startp || v > endp ) {
					Q_snprintf( errBuf, sizeof(errBuf), "jump target %i at %i is out of range (%i,%i)", v, i-1, startp, endp );
					return errBuf;
				}
				if ( buf[v].opStack != ci->opStack - 4 ) {
					n = buf[v].opStack;
					Q_snprintf( errBuf, sizeof(errBuf), "jump target %i has bad opStack %i", v, n );
					return errBuf;
				}
				if ( buf[v].op == OP_ENTER ) {
					n = buf[v].op;
					Q_snprintf( errBuf, sizeof(errBuf), "jump target %i has bad opcode %s", v, opname[ n ] );
					return errBuf;
				}
				if ( v == (i-1) ) {
					Q_snprintf( errBuf, sizeof(errBuf), "self loop at %i", v );
					return errBuf;
				}
				// mark jump target
				buf[v].jused = 1;
			} else {
				if ( proc )
					proc->swtch = 1;
				else
					ci->swtch = 1;
			}
			continue;
		}

		if ( op0 == OP_CALL ) {
			if ( ci->opStack < 4 ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad call opStack at %i", i );
				return errBuf;
			}
			if ( op1 == OP_CONST ) {
				v = buf[i-1].value;
				// analyse only local function calls
				if ( v >= 0 ) {
					if ( v >= instructionCount ) {
						Q_snprintf( errBuf, sizeof(errBuf), "call target %i is out of range", v );
						return errBuf;
					}
					if ( buf[v].op != OP_ENTER ) {
						n = buf[v].op;
						Q_snprintf( errBuf, sizeof(errBuf), "call target %i has bad opcode %s", v, opname[ n ] );
						return errBuf;
					}
					if ( v == 0 ) {
						Q_snprintf( errBuf, sizeof(errBuf), "explicit vmMain call inside VM at %i", i );
						return errBuf;
					}
					// mark jump target
					buf[v].jused = 1;
				}
			}
			continue;
		}

		if ( ci->op == OP_ARG ) {
			v = ci->value & 255;
			if ( proc == NULL ) {
				Q_snprintf( errBuf, sizeof(errBuf), "missing proc frame for %s %i at %i", opname[ ci->op ], v, i );
				return errBuf;
			}
			// argument can't exceed programStack frame
			if ( v < 8 || v > pstack - 4 || (v & 3) ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad argument address %i at %i", v, i );
				return errBuf;
			}
			continue;
		}

		if ( ci->op == OP_LOCAL ) {
			v = ci->value;
			if ( proc == NULL ) {
				Q_snprintf( errBuf, sizeof(errBuf), "missing proc frame for %s %i at %i", opname[ ci->op ], v, i );
				return errBuf;
			}
			if ( (ci+1)->op == OP_LOAD4 || (ci+1)->op == OP_LOAD2 || (ci+1)->op == OP_LOAD1 ) {
				if ( !safe_address( ci, proc, dataLength ) ) {
					Q_snprintf( errBuf, sizeof(errBuf), "bad %s address %i at %i", opname[ ci->op ], v, i );
					return errBuf;
				}
			}
			continue;
		}

		if ( ci->op == OP_LOAD4 && op1 == OP_CONST ) {
			v = (ci-1)->value;
			if ( v < 0 || v > dataLength - 4 ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad %s address %i at %i", opname[ ci->op ], v, i - 1 );
				return errBuf;
			}
			continue;
		}

		if ( ci->op == OP_LOAD2 && op1 == OP_CONST ) {
			v = (ci-1)->value;
			if ( v < 0 || v > dataLength - 2 ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad %s address %i at %i", opname[ ci->op ], v, i - 1 );
				return errBuf;
			}
			continue;
		}

		if ( ci->op == OP_LOAD1 && op1 == OP_CONST ) {
			v =  (ci-1)->value;
			if ( v < 0 || v > dataLength - 1 ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad %s address %i at %i", opname[ ci->op ], v, i - 1 );
				return errBuf;
			}
			continue;
		}

		if ( ci->op == OP_STORE4 || ci->op == OP_STORE2 || ci->op == OP_STORE1 ) {
			instruction_t *x = opStackPtr[ opStack / 4 + 1 ];
			if ( x->op == OP_CONST || x->op == OP_LOCAL ) {
				if ( safe_address( x, proc, dataLength ) ) {
					ci->safe = 1;
					safe_stores++;
					continue;
				} else {
					Q_snprintf( errBuf, sizeof(errBuf), "bad %s address %i at %i", opname[ ci->op ], x->value, (int)(x - buf) );
					return errBuf;
				}
			}
			unsafe_stores++;
			continue;
		}

		if ( ci->op == OP_BLOCK_COPY ) {
			instruction_t *src = opStackPtr[ opStack / 4 + 2 ];
			instruction_t *dst = opStackPtr[ opStack / 4 + 1 ];
			int safe = 0;
			v = ci->value;
			if ( v >= dataLength ) {
				Q_snprintf( errBuf, sizeof(errBuf), "bad count %i for block copy at %i", v, i - 1 );
				return errBuf;
			}
			if ( src->op == OP_LOCAL || src->op == OP_CONST ) {
				if ( !safe_address( src, proc, dataLength ) ) {
					Q_snprintf( errBuf, sizeof(errBuf), "bad src for block copy at %i", (int)(dst - buf) );
					return errBuf;
				}
				src->safe = 1;
				safe++;
			}
			if ( dst->op == OP_LOCAL || dst->op == OP_CONST ) {
				if ( !safe_address( dst, proc, dataLength ) ) {
					Q_snprintf( errBuf, sizeof(errBuf), "bad dst for block copy at %i", (int)(dst - buf) );
					return errBuf;
				}
				dst->safe = 1;
				safe++;
			}
			if ( safe == 2 ) {
				ci->safe = 1;
			}
		}
//		op1 = op0;
//		ci++;
	}

	if ( ( safe_stores + unsafe_stores ) > 0 ) {
		Com_DPrintf( "%s: safe stores - %i (%i%%)\n", __func__, safe_stores, safe_stores * 100 / ( safe_stores + unsafe_stores ) );
	}

	if ( op1 != OP_UNDEF && op1 != OP_LEAVE ) {
		Q_snprintf( errBuf, sizeof(errBuf), "missing return instruction at the end of the image" );
		return errBuf;
	}

	// ensure that the optimization pass knows about all the jump table targets
	if ( jumpTableTargets ) {
		// first pass - validate
		for( i = 0; i < numJumpTableTargets; i++ ) {
			n = jumpTableTargets[ i ];
			if ( n < 0 || n >= instructionCount ) {
				Com_DPrintf( "jump target %i set on instruction %i that is out of range [0..%i]\n",
					i, n, instructionCount - 1 );
				break;
			}
			if ( buf[n].opStack != 0 ) {
				// we may trap this on buggy VM_MAGIC_VER2 images
				// but we can safely optimize code even without JTRGSEG
				// so just switch to VM_MAGIC path here
				// Suppress warning - this is handled gracefully
				break;
			}
		}
		if ( i != numJumpTableTargets ) {
			// we may trap this on buggy VM_MAGIC_VER2 images
			// but we can safely optimize code even without JTRGSEG
			// so just switch to VM_MAGIC path here
			goto __noJTS;
		}
		// second pass - apply
		for( i = 0; i < numJumpTableTargets; i++ ) {
			n = jumpTableTargets[ i ];
			buf[ n ].jused = 1;
		}
	} else {
__noJTS:
		v = 0;
		// instructions with opStack > 0 can't be jump labels so it is safe to optimize/merge
		for ( i = 0, ci = buf; i < instructionCount; i++, ci++ ) {
			if ( ci->op == OP_ENTER ) {
				v = ci->swtch;
				continue;
			}
			// if there is a switch statement in function -
			// mark all potential jump labels
			if ( ci->swtch )
				v = ci->swtch;
			if ( ci->opStack > 0 )
				ci->jused = 0;
			else if ( v )
				ci->jused = 1;
		}
	}

	VM_Fixup( buf, instructionCount );

	return NULL;
}


/*
=================
VM_ReplaceInstructions
=================
*/
void VM_ReplaceInstructions( vm_t *vm, instruction_t *buf ) {
	instruction_t *ip;

	//Com_Printf( S_COLOR_GREEN "VMINFO [%s] crc: %08X, ic: %i, dl: %i\n", vm->name, vm->crc32sum, vm->instructionCount, vm->exactDataLength );

	if ( vm->index == VM_CGAME ) {
		if ( vm->crc32sum == VM_CRC32_QAGAME && vm->instructionCount == VM_INSTRUCTION_COUNT_QAGAME && vm->exactDataLength == VM_DATA_LENGTH_QAGAME ) {
			ip = buf + 110190;
			if ( ip->op == OP_ENTER && (ip+183)->op == OP_LEAVE && ip->value == (ip+183)->value ) {
				ip++;
				ip->op = OP_CONST;	ip->value = 110372; ip++;
				ip->op = OP_JUMP;	ip->value = 0; ip++;
				ip->op = OP_IGNORE; ip->value = 0;
			}
			if ( buf[4358].op == OP_LOCAL && buf[4358].value == 308 && buf[4359].op == OP_CONST && !buf[4359].value ) {
				buf[4359].value++;
			}
		} else
		if ( vm->crc32sum == 0xF0F1AE90 && vm->instructionCount == 123552 && vm->exactDataLength == 2007520 ) {
			ip = buf + 110177;
			if ( ip->op == OP_ENTER && (ip+183)->op == OP_LEAVE && ip->value == (ip+183)->value ) {
				ip++;
				ip->op = OP_CONST;	ip->value = 110359; ip++;
				ip->op = OP_JUMP;	ip->value = 0; ip++;
				ip->op = OP_IGNORE; ip->value = 0;
			}
			if ( buf[4358].op == OP_LOCAL && buf[4358].value == 308 && buf[4359].op == OP_CONST && !buf[4359].value ) {
				buf[4359].value++;
			}
		} else
		if ( vm->crc32sum == 0x051D4668 && vm->instructionCount == 267812 && vm->exactDataLength == 38064376 ) {
			ip = buf + 235;
			if ( ip->value == 70943 ) {
				VM_IgnoreInstructions( ip, 8 );
			}
		}
	}

	if ( vm->index == VM_GAME ) {
		if ( vm->crc32sum == 0x5AAE0ACC && vm->instructionCount == 251521 && vm->exactDataLength == 1872720 ) {
			vm->forceDataMask = qtrue; // OSP server doing some bad things with memory
		} else {
			vm->forceDataMask = qfalse;
		}
	}

	if ( vm->index == VM_UI ) {
		// fix OSP demo UI
		if ( vm->crc32sum == 0xCA84F31D && vm->instructionCount == 78585 && vm->exactDataLength == 542180 ) {
			if ( memcmp( vm->dataBase + 0x3D2E, "dm_67", 5 ) == 0 ) {
				memcpy( vm->dataBase + 0x3D2E, "dm_??", 5 );
			}
			if ( memcmp( vm->dataBase + 0x3D50, "\"%s.%s\"\n", 8 ) == 0 ) {
				memcpy( vm->dataBase + 0x3D50, "\"%s\"\n", 6 );
			}
		}
		// fix defrag-1.91.25 demo UI - masked Q_strupr() calls for directories and filenames
		if ( vm->crc32sum == 0x6E51985F && vm->instructionCount == 125942 && vm->exactDataLength == 1334788 ) {
			ip = buf + 60150;
			if ( ip[0].op == OP_LOCAL && ip[0].value == 28 && ip[1].op == OP_LOAD4 && ip[2].op == OP_ARG && ip[3].value == 124325 ) {
				VM_IgnoreInstructions( ip, 6 );
				ip = buf + 60438;
				VM_IgnoreInstructions( ip, 6 );
			}
		}
	}
}


/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
	vmHeader_t	*header;

	// DLL's can't be restarted in place
	if ( vm->dllHandle ) {
		syscall_t		systemCall;
		dllSyscall_t	dllSyscall;
		vmIndex_t		index;

		index = vm->index;
		systemCall = vm->systemCall;
		dllSyscall = vm->dllSyscall;

		VM_Free( vm );

		vm = VM_Create( index, systemCall, dllSyscall, VMI_NATIVE );
		return vm;
	}

	// load the image
	if ( ( header = VM_LoadQVM( vm, qfalse ) ) == NULL ) {
		// Provide a clearer error about which VM failed to restart
		VM_Error( ERR_DROP, vm->index );
		return NULL;
	}

	if ( com_developer && com_developer->integer ) {
		Com_Printf( "VM_Restart(%s)\n", vmName[ vm->index ] );
	}

	// free the original file
	FS_FreeFile( header );

	return vm;
}


/*
=================
VM_LoadLib

Used to load a development library (.so/.dll) instead of a virtual machine

TTimo: added some verbosity in debug
=================
*/
static void * QDECL VM_LoadLib( const char *name, vmMainFunc_t *entryPoint, dllSyscall_t systemcalls ) {

	const char	*gamedir = FS_GetCurrentGameDir();
	char		filename[ MAX_QPATH ];
	void		*libHandle;
	dllEntry_t	dllEntry;

	if ( !name || !name[0] ) {
		Com_Printf( "VM_LoadLib: Invalid library name (NULL or empty)\n" );
		return NULL;
	}

	Com_sprintf( filename, sizeof( filename ), "%s." ARCH_STRING DLL_EXT, name );
	Com_Printf( "VM_LoadLib: Looking for library '%s' in gamedir '%s'\n", filename, gamedir ? gamedir : "<unknown>" );

	libHandle = FS_LoadLibrary( filename );

	// Fallback for game module: many mods (like this project) build the server DLL as "game.x86_64.so"
	// instead of the stock "qagame.x86_64.so". If the primary load fails and we're asked for "qagame",
	// try "game" as a secondary name before giving up.
	if ( !libHandle && !Q_stricmp( name, "qagame" ) ) {
		char altFilename[ MAX_QPATH ];
		Com_sprintf( altFilename, sizeof( altFilename ), "game." ARCH_STRING DLL_EXT );
		Com_Printf( "VM_LoadLib: Primary '%s' failed, trying fallback '%s' for game module\n",
		            filename, altFilename );
		libHandle = FS_LoadLibrary( altFilename );
		if ( libHandle ) {
			Com_Printf( "VM_LoadLib: Fallback '%s' loaded successfully for game module\n", altFilename );
			Q_strncpyz( filename, altFilename, sizeof( filename ) );
		} else {
			Com_Printf( "VM_LoadLib: Fallback '%s' also failed for game module\n", altFilename );
		}
	}

	if ( !libHandle ) {
		Com_Printf( "VM_LoadLib '%s' failed (searched in gamedir: %s)\n", filename, gamedir ? gamedir : "<unknown>" );
		return NULL;
	}

	Com_Printf( "VM_LoadLib '%s' loaded successfully, handle=%p\n", filename, libHandle );

	Com_Printf( "VM_LoadLib: Loading symbols dllEntry and vmMain\n" );
	{
		void *dllEntrySym = Sys_LoadFunction( libHandle, "dllEntry" );
		void *vmMainSym   = Sys_LoadFunction( libHandle, "vmMain" );
		dllEntry = (dllEntry_t)(intptr_t)dllEntrySym;
		*entryPoint = (vmMainFunc_t)(intptr_t)vmMainSym;
	}
	if ( !*entryPoint || !dllEntry ) {
		const char *missing = !*entryPoint ? "vmMain" : "dllEntry";
		Com_Printf( "VM_LoadLib '%s' failed: missing required symbol '%s'\n", filename, missing );
		Sys_UnloadLibrary( libHandle );
		return NULL;
	}

	Com_Printf( "VM_LoadLib(%s) found vmMain at %p, dllEntry at %p\n", name, (void*)(intptr_t)*entryPoint, (void*)(intptr_t)dllEntry );
	Com_Printf( "VM_LoadLib: Calling dllEntry with systemcalls=%p\n", (void*)(intptr_t)systemcalls );
	dllEntry( systemcalls );
	Com_Printf( "VM_LoadLib(%s) dllEntry completed successfully!\n", name );

	return libHandle;
}


/*
================
VM_Create

If image ends in .qvm it will be interpreted, otherwise
it will attempt to load as a system dll
================
*/
vm_t *VM_Create( vmIndex_t index, syscall_t systemCalls, dllSyscall_t dllSyscalls, vmInterpret_t interpret ) {
	int			remaining;
	const char	*name;
	vmHeader_t	*header;
	vm_t		*vm;

	if ( !systemCalls ) {
		Com_Error( ERR_FATAL, "VM_Create: bad parms" );
	}

	if ( (unsigned)index >= VM_COUNT ) {
		Com_Error( ERR_DROP, "VM_Create: bad vm index %i", index );
	}

	remaining = Hunk_MemoryRemaining();

	vm = &vmTable[ index ];

	/* For monolithic builds, force native interpretation so we prefer the
	 * statically-linked entry points instead of attempting to load QVM/DLL.
	 * If the static symbols are missing, we still fall through to the normal
	 * QVM/DLL path below.
	 */
#ifdef COMBINED_MONOLITH
	if ( vm_combined && vm_combined->integer ) {
		interpret = VMI_NATIVE;
	}
#endif

	// see if we already have the VM
	if ( vm->name ) {
		if ( vm->index != index ) {
			Com_Error( ERR_DROP, "VM_Create: bad allocated vm index %i", vm->index );
			return NULL;
		}
		// VM already created; reuse existing instance
		return vm;
	}

	name = vmName[ index ];

	// Mod safety: Validate VM loading context
	{
		const char *gameDir = Cvar_VariableString("fs_game");
		if ( gameDir && gameDir[0] && Q_stricmp(gameDir, BASEGAME) != 0 ) {
			// We're loading a VM from a mod - validate the mod first
			Crash_SetModLoadingContext( gameDir, va( "VM_%s_load", name ) );
			if ( !Mod_ValidateBeforeLoad( gameDir ) ) {
				Com_Printf( S_COLOR_RED "Mod validation failed for %s VM load: %s\n", name, gameDir );
				Crash_ReportModLoad( gameDir, va( "VM_%s_validation_failed", name ) );
				Crash_ClearModLoadingContext();
				return NULL;
			}
			Crash_ClearModLoadingContext();
		}
	}

	vm->name = name;
	vm->index = index;
	vm->systemCall = systemCalls;
	vm->dllSyscall = dllSyscalls;
	vm->privateFlag = CVAR_PRIVATE;

	// never allow dll loading with a demo
	if ( interpret == VMI_NATIVE ) {
		if ( Cvar_VariableIntegerValue( "fs_restrict" ) ) {
			interpret = VMI_COMPILED;
		}
	}

	if ( interpret == VMI_NATIVE ) {
		// Check if this is a monolithic build with combined modules
		if ( vm_combined && vm_combined->integer ) {
			// Monolithic build: modules are statically linked, skip library loading
			if ( com_developer && com_developer->integer ) {
				Com_Printf( "VM_Create: Monolithic build detected (vm_combined=1), skipping library load for %s\n", name );
			}
			
			// For monolithic builds, we need to get entry points from statically linked modules
			// This will be handled by the linker resolving symbols from static libraries
			#ifdef COMBINED_MONOLITH
				// Try to get entry point from statically linked module
				// The linker will resolve these symbols from the static libraries we linked
				void *combinedHandle = VM_GetCombinedEntryPoint( index, &vm->entryPoint, dllSyscalls );
				if ( combinedHandle && vm->entryPoint ) {
					vm->dllHandle = NULL; // statically linked; no dlclose
					vm->privateFlag = 0; // allow reading private cvars
					vm->dataAlloc = ~0U;
					vm->dataMask = ~0U;
					vm->dataBase = 0;
					Com_Printf( "VM_Create: Using statically linked %s, entryPoint=%p\n", name, (void*)vm->entryPoint );
					// Defensive: ensure dllEntry was invoked so the module's syscall pointer is initialized.
					switch ( index ) {
						case VM_GAME:
							if ( dllEntry_game ) dllEntry_game( dllSyscalls );
							break;
						case VM_CGAME:
							if ( dllEntry_cgame ) dllEntry_cgame( dllSyscalls );
							break;
						case VM_UI:
							if ( dllEntry_ui ) dllEntry_ui( dllSyscalls );
							break;
						default:
							break;
					}
					// Debug: verify function pointer
					if ( index == VM_GAME ) {
						extern intptr_t QDECL vmMain_game( int command, int arg0, int arg1, int arg2 );
						Com_Printf( "VM_Create: vmMain_game address=%p, vm->entryPoint=%p\n", (void*)vmMain_game, (void*)vm->entryPoint );
						Com_Printf( "VM_Create: Calling dllEntry_game to initialize syscall pointer\n" );
					}
					return vm;
				} else {
					Com_Error( ERR_DROP, "VM_Create: monolithic build expected statically linked %s but entry point was missing", name );
				}
			#else
				// vm_combined is set but COMBINED_MONOLITH not defined - shouldn't happen
				Com_Printf( "VM_Create: WARNING - vm_combined set but COMBINED_MONOLITH not defined\n" );
				interpret = VMI_COMPILED;
			#endif
		} else {
			// Normal build: try to load as a system library (.so/.dll)
			if ( com_developer && com_developer->integer ) {
				Com_Printf( "VM_Create: Loading library file %s (index=%d)\n", name, index );
			}
			
			// Preload game.x86_64.so if loading UI module, since UI depends on it
			// This ensures the dependency is available when dlopen resolves ../vm/game.x86_64.so
			// NOTE: We do NOT call dllEntry on the preloaded handle - it will be called when
			// the game module is properly loaded for the server with the correct syscall pointer
			if ( index == VM_UI ) {
				char preloadName[ MAX_QPATH ];
				Com_sprintf( preloadName, sizeof( preloadName ), "game." ARCH_STRING DLL_EXT );
				if ( com_developer && com_developer->integer ) {
					Com_Printf( "VM_Create: Preloading %s for UI dependency\n", preloadName );
				}
				void *gameHandle = FS_LoadLibrary( preloadName );
				if ( gameHandle ) {
					if ( com_developer && com_developer->integer ) {
						Com_Printf( "VM_Create: Preloaded %s for UI dependency (not initializing syscall pointer)\n", preloadName );
					}
					// Don't unload it - UI needs it
					// Don't call dllEntry - it will be called when game module is loaded for server
				} else {
					if ( com_developer && com_developer->integer ) {
						Com_Printf( "VM_Create: WARNING - Failed to preload %s (this is normal for monolithic builds)\n", preloadName );
					}
				}
			}
			
			if ( com_developer && com_developer->integer ) {
				Com_Printf( "VM_Create: Calling VM_LoadLib for %s\n", name );
			}
			vm->dllHandle = VM_LoadLib( name, &vm->entryPoint, dllSyscalls );
			if ( vm->dllHandle ) {
				if ( com_developer && com_developer->integer ) {
					Com_Printf( "VM_Create: VM_LoadLib succeeded for %s, entryPoint=%p\n", name, (void*)(intptr_t)vm->entryPoint );
					Com_Printf( "VM_Create: VM_LoadLib succeeded for %s, entryPoint=%p\n", name, (void*)(intptr_t)vm->entryPoint );
				}
				vm->privateFlag = 0; // allow reading private cvars
				vm->dataAlloc = ~0U;
				vm->dataMask = ~0U;
				vm->dataBase = 0;
				if ( com_developer && com_developer->integer ) {
					Com_Printf( "VM_Create: Returning VM for %s\n", name );
				}
				return vm;
			}

			Com_Printf( "VM_Create: Failed to load dll for %s, looking for qvm.\n", name );
			interpret = VMI_COMPILED;
		}
	}

	// load the image
	vm->name = name; // Set name before loading QVM
	header = VM_LoadQVM( vm, qtrue );
	if ( header == NULL ) {
		// QVM bytecode not found or failed to load.
		// As a fallback, try to load a native DLL for this VM if possible.
		//
		// This is especially useful for mods that ship only native modules
		// (e.g. cgame.x86_64.so) without providing vm/cgame.qvm.
		Com_Printf( "VM_Create: VM_LoadQVM failed for %s, attempting native DLL fallback\n", name );

		// Avoid recursion: only attempt the fallback if we haven't already
		// tried native loading for this VM.
		if ( interpret != VMI_NATIVE ) {
			vm->dllHandle = VM_LoadLib( name, &vm->entryPoint, dllSyscalls );
			if ( vm->dllHandle ) {
				Com_Printf( "VM_Create: DLL fallback succeeded for %s, entryPoint=%p\n",
				            name, (void*)(intptr_t)vm->entryPoint );
				vm->privateFlag = 0; // allow reading private cvars
				vm->dataAlloc = ~0U;
				vm->dataMask = ~0U;
				vm->dataBase = 0;
				return vm;
			}
			Com_Printf( "VM_Create: DLL fallback also failed for %s\n", name );
		}

		return NULL;
	}

	// Check QVM format and adjust interpretation mode
	// Old VM_MAGIC format QVMs should use interpretation, not compilation
	if ( LittleLong( header->vmMagic ) == VM_MAGIC && interpret >= VMI_COMPILED ) {
		Com_Printf( "VM_Create: Old QVM format detected, falling back to interpretation for %s\n", name );
		interpret = VMI_BYTECODE;
	}

	// allocate space for the jump targets, which will be filled in by the compile/prep functions
	vm->instructionCount = header->instructionCount;
	//vm->instructionPointers = Hunk_Alloc(vm->instructionCount * sizeof(*vm->instructionPointers), h_high);
	vm->instructionPointers = NULL;

	// copy or compile the instructions
	vm->codeLength = header->codeLength;

	// the stack is implicitly at the end of the image
	vm->programStack = vm->dataMask + 1;
	vm->stackBottom = vm->programStack - PROGRAM_STACK_SIZE - vm->programStackExtra;

	vm->compiled = qfalse;

#ifdef NO_VM_COMPILED
	if ( interpret >= VMI_COMPILED ) {
		Com_Printf( "Architecture doesn't have a bytecode compiler, using interpreter\n" );
		interpret = VMI_BYTECODE;
	}
#else
	if ( interpret >= VMI_COMPILED ) {
		if ( VM_Compile( vm, header ) ) {
			vm->compiled = qtrue;
		}
	}
#endif
	// VM_Compile may have reset vm->compiled if compilation failed
	if ( !vm->compiled ) {
		if ( !VM_PrepareInterpreter2( vm, header ) ) {
			FS_FreeFile( header );	// free the original file
			VM_Free( vm );
			return NULL;
		}
	}

	// free the original file
	FS_FreeFile( header );

	// load the map file
	VM_LoadSymbols( vm );

	Com_Printf( "%s loaded in %d bytes on the hunk\n", vm->name, remaining - Hunk_MemoryRemaining() );

	return vm;
}


/*
==============
VM_ValidatePointer

Validates a pointer for basic sanity checks
==============
*/
static qboolean VM_ValidatePointer( void *ptr, const char *context ) {
	if ( !ptr ) {
		Com_DPrintf( "VM_ValidatePointer: NULL pointer in context '%s'\n",
		           context ? context : "unknown" );
		return qfalse;
	}

	uintptr_t addr = (uintptr_t)ptr;

	// Avoid obviously invalid addresses (NULL, very low addresses, kernel space)
	if ( addr < 0x1000 || addr >= 0x7FFFFFFFFFFF ) {
		Com_Printf( "VM_ValidatePointer: Pointer %p appears invalid (out of range) in context '%s'\n",
		           ptr, context ? context : "unknown" );
		return qfalse;
	}

	// Check for suspicious patterns that might indicate corrupted pointers
	// Avoid pointers that look like they might be uninitialized or corrupted
	if ( (addr & 0xFFFF) == 0xAAAA || (addr & 0xFFFF) == 0xCCCC || (addr & 0xFFFF) == 0xCDCD ) {
		Com_Printf( "VM_ValidatePointer: Pointer %p appears corrupted (suspicious pattern) in context '%s'\n",
		           ptr, context ? context : "unknown" );
		return qfalse;
	}

	// Basic user address space check (64-bit Linux)
	// Check that pointer is in user space (not kernel space)
	if ( addr >= 0x800000000000ULL ) {  // Kernel space starts around 0x800000000000 on x86_64
		Com_Printf( "VM_ValidatePointer: Pointer %p appears to be in kernel space in context '%s'\n",
		           ptr, context ? context : "unknown" );
		return qfalse;
	}

	return qtrue;
}

/*
==============
VM_ValidateVMState

Validates VM structure integrity before destruction
==============
*/
static qboolean VM_ValidateVMState( vm_t *vm ) {
	if ( !vm ) {
		Com_DPrintf( "VM_ValidateVMState: NULL VM pointer\n" );
		return qfalse;
	}

	// Check for obviously corrupted VM structure
	if ( vm->index < 0 || vm->index >= VM_COUNT ) {
		Com_Printf( "VM_ValidateVMState: Invalid VM index %d\n", vm->index );
		return qfalse;
	}

	if ( !vm->name || !vm->name[0] ) {
		Com_Printf( "VM_ValidateVMState: VM has invalid name\n" );
		return qfalse;
	}

	// Validate function pointers if they exist (cast to void* for validation)
	if ( vm->destroy ) {
		union { void (*func)(void); void *ptr; } convert = { .func = vm->destroy };
		if ( !VM_ValidatePointer( convert.ptr, "vm_destroy" ) ) {
			Com_Printf( "VM_ValidateVMState: VM destroy function pointer appears corrupted\n" );
			return qfalse;
		}
	}

	// Validate DLL handle if it exists
	if ( vm->dllHandle && !VM_ValidatePointer( vm->dllHandle, "vm_dllHandle" ) ) {
		Com_Printf( "VM_ValidateVMState: VM DLL handle appears corrupted\n" );
		return qfalse;
	}

	return qtrue;
}

/*
==============
VM_SafeDestroy

Safely calls VM destroy function with error handling
==============
*/
static void VM_SafeDestroy( vm_t *vm ) {
	if ( !vm || !vm->destroy ) {
		return;
	}

	// Additional validation before calling destroy
	if ( !VM_ValidateVMState(vm) ) {
		Com_Printf( "VM_SafeDestroy: VM state validation failed, skipping destroy for %s\n",
		           vm->name ? vm->name : "unknown" );
		return;
	}

	Com_DPrintf( "VM_SafeDestroy: Calling destroy function for VM %s\n",
	           vm->name ? vm->name : "unknown" );

	// Call destroy with basic error trapping
	// Note: We can't use setjmp/longjmp here as it's not portable across DLL boundaries
	vm->destroy( vm );
}

/*
==============
VM_SafeUnloadLibrary

Safely unloads VM DLL with error handling
==============
*/
static void VM_SafeUnloadLibrary( void *dllHandle ) {
	if ( !dllHandle ) {
		return;
	}

	Com_DPrintf( "VM_SafeUnloadLibrary: Unloading DLL %p\n", dllHandle );

	// Basic validation - ensure dllHandle looks reasonable
	if ( (uintptr_t)dllHandle < 0x1000 || (uintptr_t)dllHandle >= 0x7FFFFFFFFFFF ) {
		Com_Printf( "VM_SafeUnloadLibrary: DLL handle appears corrupted: %p, skipping unload\n", dllHandle );
		return;
	}

	Sys_UnloadLibrary( dllHandle );
}

/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {

	if( !vm ) {
		return;
	}

	// Enhanced validation before any operations
	if ( !VM_ValidateVMState(vm) ) {
		Com_Printf( "VM_Free: VM state validation failed for %s, performing minimal cleanup\n",
		           vm->name ? vm->name : "unknown" );
		goto cleanup;
	}

	if ( vm->callLevel ) {
		if ( !forced_unload ) {
			Com_Error( ERR_FATAL, "VM_Free(%s) on running vm", vm->name );
			return;
		} else {
			Com_Printf( "forcefully unloading %s vm\n", vm->name );
		}
	}

	// Safe destroy call
	VM_SafeDestroy( vm );

	// Safe DLL unloading
	if ( vm->dllHandle ) {
#ifdef COMBINED_MONOLITH
		// Statically linked modules shouldn't be unloaded via dlclose.
		if ( vm_combined && vm_combined->integer ) {
			// Skip unload; handle is NULL for monolith entries.
		} else
#endif
		{
			VM_SafeUnloadLibrary( vm->dllHandle );
		}
	}

#if 0	// now automatically freed by hunk
	if ( vm->codeBase.ptr ) {
		Z_Free( vm->codeBase.ptr );
	}
	if ( vm->dataBase ) {
		Z_Free( vm->dataBase );
	}
	if ( vm->instructionPointers ) {
		Z_Free( vm->instructionPointers );
	}
#endif

cleanup:
	Com_Memset( vm, 0, sizeof( *vm ) );
}


void VM_Clear( void ) {
	int i;
	for ( i = 0; i < VM_COUNT; i++ ) {
		VM_Free( &vmTable[ i ] );
	}
}


void VM_Forced_Unload_Start(void) {
	forced_unload = 1;
}


void VM_Forced_Unload_Done(void) {
	forced_unload = 0;
}


/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/

intptr_t QDECL VM_Call( vm_t *vm, int nargs, int callnum, ... )
{
	//vm_t	*oldVM;
	intptr_t r;
	int i;

	if ( !vm ) {
		// Don't crash - return 0 for safety when UI is not available
		// This allows the engine to continue running without UI
		Com_Printf( "WARNING: VM_Call called with NULL vm (callnum=%d, nargs=%d)\n", callnum, nargs );
		return 0;
	}

#ifdef DEBUG
	if ( vm_debugLevel ) {
	  Com_Printf( "VM_Call( %d )\n", callnum );
	}

	if ( nargs >= MAX_VMMAIN_CALL_ARGS ) {
		Com_Error( ERR_DROP, "VM_Call: nargs >= MAX_VMMAIN_CALL_ARGS" );
	}
#endif

	++vm->callLevel;
	// if we have a dll loaded, call it directly
	if ( vm->entryPoint )
	{
		//rcg010207 -  see dissertation at top of VM_DllSyscall() in this file.
		intptr_t args[MAX_VMMAIN_CALL_ARGS-1];
		va_list ap;
		va_start( ap, callnum );
		for ( i = 0; i < nargs; i++ ) {
			args[i] = va_arg( ap, intptr_t );
		}
		va_end( ap );
		
		// Ensure unused arguments are zero-initialized
		for ( i = nargs; i < MAX_VMMAIN_CALL_ARGS-1; i++ ) {
			args[i] = 0;
		}

		// add more arguments if you're changed MAX_VMMAIN_CALL_ARGS:
		r = vm->entryPoint( callnum, args[0], args[1], args[2] );
	} else {
#if id386 && !defined __clang__ // calling convention doesn't need conversion in some cases
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, nargs+1, (int32_t*)&callnum );
		else
#endif
			r = VM_CallInterpreted2( vm, nargs+1, (int32_t*)&callnum );
#else
		int32_t args[MAX_VMMAIN_CALL_ARGS];
		va_list ap;

		args[0] = callnum;
		va_start( ap, callnum );
		for ( i = 0; i < nargs; i++ ) {
			args[i+1] = va_arg( ap, int32_t );
		}
		va_end(ap);
#ifndef NO_VM_COMPILED
		if ( vm->compiled )
			r = VM_CallCompiled( vm, nargs+1, &args[0] );
		else
#endif
			r = VM_CallInterpreted2( vm, nargs+1, &args[0] );
#endif
	}
	--vm->callLevel;

	return r;
}


//=================================================================

static int QDECL VM_ProfileSort( const void *a, const void *b ) {
	vmSymbol_t	*sa, *sb;

	sa = *(vmSymbol_t **)a;
	sb = *(vmSymbol_t **)b;

	if ( sa->profileCount < sb->profileCount ) {
		return -1;
	}
	if ( sa->profileCount > sb->profileCount ) {
		return 1;
	}
	return 0;
}


/*
==============
VM_NameToVM
==============
*/
static vm_t *VM_NameToVM( const char *name )
{
	vmIndex_t index;

	if ( !Q_stricmp( name, "game" ) )
		index = VM_GAME;
	else if ( !Q_stricmp( name, "cgame" ) )
		index = VM_CGAME;
	else if ( !Q_stricmp( name, "ui" ) )
		index = VM_UI;
	else {
		Com_Printf( " unknown VM name '%s'\n", name );
		return NULL;
	}

	if ( !vmTable[ index ].name ) {
		Com_Printf( " %s is not running.\n", name );
		return NULL;
	}

	return &vmTable[ index ];
}


/*
==============
VM_VmProfile_f

==============
*/
static void VM_VmProfile_f( void ) {
	vm_t		*vm;
	vmSymbol_t	**sorted, *sym;
	int			i;
	double		total;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: %s <game|cgame|ui>\n", Cmd_Argv( 0 ) );
		return;
	}

	vm = VM_NameToVM( Cmd_Argv( 1 ) );
	if ( vm == NULL ) {
		return;
	}

	if ( !vm->numSymbols ) {
		return;
	}

	sorted = Z_Malloc( vm->numSymbols * sizeof( *sorted ) );
	sorted[0] = vm->symbols;
	total = sorted[0]->profileCount;
	for ( i = 1 ; i < vm->numSymbols ; i++ ) {
		sorted[i] = sorted[i-1]->next;
		total += sorted[i]->profileCount;
	}

	qsort( sorted, vm->numSymbols, sizeof( *sorted ), VM_ProfileSort );

	for ( i = 0 ; i < vm->numSymbols ; i++ ) {
		int		perc;

		sym = sorted[i];

		perc = 100 * (float) sym->profileCount / total;
		Com_Printf( "%2i%% %9i %s\n", perc, sym->profileCount, sym->symName );
		sym->profileCount = 0;
	}

	Com_Printf("    %9.0f total\n", total );

	Z_Free( sorted );
}


/*
==============
VM_VmInfo_f
==============
*/
static void VM_VmInfo_f( void ) {
	const vm_t	*vm;
	int		i;

	Com_Printf( "Registered virtual machines:\n" );
	for ( i = 0 ; i < VM_COUNT ; i++ ) {
		vm = &vmTable[i];
		if ( !vm->name ) {
			continue;
		}
		Com_Printf( "%s : ", vm->name );
		if ( vm->dllHandle ) {
			Com_Printf( "native\n" );
			continue;
		}
		if ( vm->compiled ) {
			Com_Printf( "compiled on load\n" );
		} else {
			Com_Printf( "interpreted\n" );
		}
		Com_Printf( "    code length : %7i\n", vm->codeLength );
		Com_Printf( "    table length: %7i\n", vm->instructionCount*4 );
		Com_Printf( "    data length : %7i\n", vm->dataMask + 1 );
	}
}


/*
===============
VM_LogSyscalls

Insert calls to this while debugging the vm compiler
===============
*/
void VM_LogSyscalls( int *args ) {
	(void)args;  // Suppress unused parameter warning
#if 0
	static	int		callnum;
	static	FILE	*f;

	if ( !f ) {
		f = Sys_FOpen( "syscalls.log", "w" );
		if ( !f ) {
			return;
		}
	}
	callnum++;
	fprintf( f, "%i: %p (%i) = %i %i %i %i\n", callnum, (void*)(args - (int *)currentVM->dataBase),
		args[0], args[1], args[2], args[3], args[4] );
#endif
}
