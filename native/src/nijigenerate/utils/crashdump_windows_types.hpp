#pragma once

#if !defined(_WIN32)
#error Windows crashdump scalar definitions require the Windows target
#endif

#include <cstdint>

// The genuine SDK exception-pointer record is shared with the callback. Its
// complete declaration remains supplied by the actual Windows headers.
struct _EXCEPTION_POINTERS;

namespace nijigenerate::utils::crashdump::detail {

// Preserve source scalar types without shadowing SDK macros or enum typedefs.
using NativeDWord = std::uint32_t;
using NativeBool = std::int32_t;
using NativeLong = std::int32_t;
using NativeHandle = void*;
using NativeMiniDumpType = std::int32_t;

// Source's private record has natural alignment; it is not the packed4 SDK
// MINIDUMP_EXCEPTION_INFORMATION. Keep its real fields independently of the
// explicit SDK pointer adaptation at the operating-system call boundary.
struct SourceMiniDumpExceptionInformation {
    NativeDWord ThreadId{};
    ::_EXCEPTION_POINTERS* ExceptionPointers{};
    NativeBool ClientPointers{};
};

inline constexpr NativeDWord nativeGenericWrite = 0x40000000;
inline constexpr NativeDWord nativeFileShareRead = 0x00000001;
inline constexpr NativeDWord nativeCreateAlways = 2;
inline constexpr NativeDWord nativeFileAttributeNormal = 0x00000080;
inline constexpr std::uint32_t nativeMessageBoxOK = 0x00000000;
inline constexpr std::uint32_t nativeMessageBoxIconError = 0x00000010;
inline constexpr NativeBool nativeFalse = 0;
inline constexpr NativeLong nativeExceptionExecuteHandler = 1;
inline constexpr NativeMiniDumpType nativeMiniDumpNormal = 0x00000000;
inline constexpr NativeMiniDumpType nativeMiniDumpWithDataSegs = 0x00000001;
inline constexpr NativeMiniDumpType nativeMiniDumpWithHandleData = 0x00000004;
inline constexpr NativeMiniDumpType nativeMiniDumpWithIndirectlyReferencedMemory = 0x00000040;
inline constexpr NativeMiniDumpType nativeMiniDumpWithThreadInfo = 0x00001000;

} // namespace nijigenerate::utils::crashdump::detail
