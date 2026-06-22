# x86/x86_64 assembly manifest (snd_mix etc.). Unix 64-bit only; Windows uses C fallbacks.

idtech3_require_layout()

macro(idtech3_init_asm_sources)
	set(ASM_SRCS "")
	if(UNIX AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86|i.86|x86_64|X86_64")
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			idtech3_glob_src_rel(ASM_SRCS
				"src/asm/*.S"
				"src/asm/*.s"
				"engine/asm/*.S"
				"engine/asm/*.s"
			)
		endif()
	endif()
endmacro()
