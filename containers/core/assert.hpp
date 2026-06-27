#pragma once

#include"config.hpp"

#include<iostream>

namespace idx {

// ─────────────────────────────────────────────────────────────────────────────
// Helper macros that wrap a block of statements in do { ... } while(false).
// This makes multi-statement macros safe to use in if/else bodies without braces.
// ─────────────────────────────────────────────────────────────────────────────
#define idxMACRO_BLOCK_BEGIN do {
#define idxMACRO_BLOCK_END } while (false)

// Prints the assertion failure message together with the source file and line number.
// Uses compiler builtins (__builtin_FILE / __builtin_LINE) so the location reported
// is the call site, not this macro definition.
#define idxPRINT_DEBUG_MESSAGE(message) \
    std::cout << "ASSERTION FAILED" << std::endl  << message << std::endl << "file: " << __builtin_FILE() \
    << " line: " << __builtin_LINE() << std::endl;

// ─────────────────────────────────────────────────────────────────────────────
// Platform-specific debug break. Halts execution so a debugger can inspect the
// call stack at the exact point of failure.
// ─────────────────────────────────────────────────────────────────────────────
#if idxCOMPILER_MSVC
#define idxDEBUG_BREAK __debugbreak();
#elif idxCPU_ARM
#define idxDEBUG_BREAK __builtin_trap();
#elif idxCPU_X86 && (idxCOMPILER_GCC || idxCOMPILER_CLANG)
// Inline x86 software interrupt 3 – the classic "INT 3" breakpoint instruction.
#define idxDEBUG_BREAK __asm__ volatile("int $3");
#elif idxPLATFORM_EMSCRIPTEN
#define idxDEBUG_BREAK  \
    idxMACRO_BLOCK_BEGIN   \
    emscripten_log(EM_LOG_CONSOLE | EM_LOG_ERROR | EM_LOG_C_STACK | EM_LOG_JS_STACK, "debugBreak!");  \
    EM_ASM({ debugger; });  \
    idxMACRO_BLOCK_END
#else
// Fallback for unknown platforms: write to address 3, which triggers a segfault
// and forces a crash that can be caught by OS-level debuggers.
#define idxDEBUG_BREAK  \
    idxMACRO_BLOCK_BEGIN   \
    volatile int* int3 = (int*)3L;  \
    *int3 = 3;       \
    idxMACRO_BLOCK_END
#endif

// ─────────────────────────────────────────────────────────────────────────────
// idxASSERT(condition, message)
//
// In DEBUG builds   : prints the message and triggers a debug break (so you can
//                     inspect the call stack in a debugger).
// In RELEASE builds : still prints the message but then spins forever (infinite
//                     loop) instead of a debug break, ensuring a detectable hard
//                     stop without relying on debugger attachment.
//
// Usage:
//   idxASSERT(index < size, "index out of range");
// ─────────────────────────────────────────────────────────────────────────────
#if idxCONFIG_DEBUG
#define idxASSERT(condition, message)    \
	idxMACRO_BLOCK_BEGIN   \
		if ((condition)==false)  { \
            idxPRINT_DEBUG_MESSAGE(message) \
			idxDEBUG_BREAK     \
        } \
	idxMACRO_BLOCK_END
#else
#define idxASSERT(condition, message)   \
        if ((condition)==false) {   \
            idxPRINT_DEBUG_MESSAGE(message)   \
            while(true){}   \
        } ((void)0)
#endif

}
