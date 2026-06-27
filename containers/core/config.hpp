#pragma once

namespace idx {

#if NDEBUG
#define idxCONFIG_DEBUG 0
#else
#define idxCONFIG_DEBUG 1
#endif

#if defined(__clang__)
// clang defines __GNUC__ or _MSC_VER
#undef  idxCOMPILER_CLANG
#define idxCOMPILER_CLANG (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#if defined(__clang_analyzer__)
#undef  idxCOMPILER_CLANG_ANALYZER
#define idxCOMPILER_CLANG_ANALYZER 1
#endif
#elif defined(_MSC_VER)
#undef  idxCOMPILER_MSVC
#define idxCOMPILER_MSVC _MSC_VER
#elif defined(__GNUC__)
#undef  idxCOMPILER_GCC
#define idxCOMPILER_GCC (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#error "idxCOMPILER_* is not defined!"
#endif

#if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
#undef  idxCPU_ARM
#define idxCPU_ARM 1
#define idxCACHE_LINE_SIZE 64
#elif defined(__MIPSEL__) || defined(__mips_isa_rev) || defined(__mips64)
#undef  idxCPU_MIPS
#define idxCPU_MIPS 1
#define idxCACHE_LINE_SIZE 64
#elif defined(_M_PPC) || defined(__powerpc__) || defined(__powerpc64__)
#undef  idxCPU_PPC
#define idxCPU_PPC 1
#define idxCACHE_LINE_SIZE 128
#elif defined(__riscv) || defined(__riscv__) || defined(RISCVEL)
#undef  idxCPU_RISCV
#define idxCPU_RISCV 1
#define idxCACHE_LINE_SIZE 64
#elif defined(_M_IX86)  ||defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#undef  idxCPU_X86
#define idxCPU_X86 1
#define idxCACHE_LINE_SIZE 64
#else
#undef  idxCPU_JIT
#define idxCPU_JIT 1
#define idxCACHE_LINE_SIZE 64
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__) || defined(__64BIT__) || defined(__mips64) || defined(__powerpc64__) || defined(__ppc64__) || defined(__LP64__)
#undef  idxARCH_64BIT
#define idxARCH_64BIT 64
#else
#undef  idxARCH_32BIT
#define idxARCH_32BIT 32
#endif

#if idxCPU_PPC
// __BIG_ENDIAN__ is gcc predefined macro
#if defined(__BIG_ENDIAN__)
#undef  idxCPU_ENDIAN_BIG
#define idxCPU_ENDIAN_BIG 1
#else
#undef  idxCPU_ENDIAN_LITTLE
#define idxCPU_ENDIAN_LITTLE 1
#endif
#else
#undef  idxCPU_ENDIAN_LITTLE
#define idxCPU_ENDIAN_LITTLE 1
#endif

#if defined(_DURANGO) || defined(_XBOX_ONE)
#undef  idxPLATFORM_XBOXONE
#define idxPLATFORM_XBOXONE 1
#elif defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX
// If _USING_V110_SDK71_ is defined it means we are using the v110_xp or v120_xp toolset.
#if defined(_MSC_VER) && (_MSC_VER >= 1700) && !defined(_USING_V110_SDK71_)
#include <winapifamily.h>
#endif // defined(_MSC_VER) && (_MSC_VER >= 1700) && (!_USING_V110_SDK71_)
#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#undef  idxPLATFORM_WINDOWS
#if !defined(WINVER) && !defined(_WIN32_WINNT)
#if idxARCH_64BIT
// When building 64-bit target Win7 and above.
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#else
// Windows Server 2003 with SP1, Windows XP with SP2 and above
#define WINVER 0x0502
#define _WIN32_WINNT 0x0502
#endif // idxARCH_64BIT
#endif // !defined(WINVER) && !defined(_WIN32_WINNT)
#define idxPLATFORM_WINDOWS _WIN32_WINNT
#else
#undef  idxPLATFORM_WINRT
#define idxPLATFORM_WINRT 1
#endif
#elif defined(__ANDROID__)
// Android compiler defines __linux__
#include <sys/cdefs.h> // Defines __BIONIC__ and includes android/api-level.h
#undef  idxPLATFORM_ANDROID
#define idxPLATFORM_ANDROID __ANDROID_API__
#elif defined(__VCCOREVER__)
// RaspberryPi compiler defines __linux__
#undef  idxPLATFORM_RPI
#define idxPLATFORM_RPI 1
#elif  defined(__linux__)
#undef  idxPLATFORM_LINUX
#define idxPLATFORM_LINUX 1
#elif  defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) || defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__)
#undef  idxPLATFORM_IOS
#define idxPLATFORM_IOS 1
#elif defined(__has_builtin) && __has_builtin(__is_target_os) && __is_target_os(xros)
#undef  idxPLATFORM_VISIONOS
#define idxPLATFORM_VISIONOS 1
#elif defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
#undef  idxPLATFORM_OSX
#define idxPLATFORM_OSX __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__
#elif defined(__EMSCRIPTEN__)
#include <emscripten/version.h>
#undef  idxPLATFORM_EMSCRIPTEN
#define idxPLATFORM_EMSCRIPTEN (__EMSCRIPTEN_MAJOR__ * 10000 + __EMSCRIPTEN_MINOR__ * 100 + __EMSCRIPTEN_TINY__)
#elif defined(__ORBIS__)
#undef  idxPLATFORM_PS4
#define idxPLATFORM_PS4 1
#elif defined(__PROSPERO__)
#undef  idxPLATFORM_PS5
#define idxPLATFORM_PS5 1
#elif  defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#undef  idxPLATFORM_BSD
#define idxPLATFORM_BSD 1
#elif defined(__GNU__)
#undef  idxPLATFORM_HURD
#define idxPLATFORM_HURD 1
#elif defined(__NX__)
#undef  idxPLATFORM_NX
#define idxPLATFORM_NX 1
#elif defined(__HAIKU__)
#undef  idxPLATFORM_HAIKU
#define idxPLATFORM_HAIKU 1
#endif
}