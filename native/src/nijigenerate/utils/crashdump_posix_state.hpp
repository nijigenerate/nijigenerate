#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nijigenerate::utils::crashdump {

inline constexpr std::size_t nativeCrashPathMax = 4096;
inline constexpr std::int32_t nativeBacktraceMaxFrames = 128;
extern std::array<std::uint8_t, nativeCrashPathMax> nativeCrashDumpPath;
extern std::size_t nativeCrashDumpPathLen;
extern std::array<std::uint8_t, nativeCrashPathMax> nativeCrashExePath;
extern std::size_t nativeCrashExePathLen;
extern std::int32_t nativeCrashHandlerActive;
extern thread_local void* nativeSignalStack;

} // namespace nijigenerate::utils::crashdump
