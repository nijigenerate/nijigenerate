#pragma once

#if !defined(_WIN32)
#error Windows crashdump state requires the actual Windows wchar ABI
#endif

#include <array>
#include <cstddef>
#include <cstdint>

namespace nijigenerate::utils::crashdump {
namespace detail {
inline constexpr std::size_t nativeCrashPathMax = 4096;
}

static_assert(sizeof(wchar_t) == 2);
extern std::array<wchar_t, detail::nativeCrashPathMax> nativeCrashDumpPathW;
extern std::size_t nativeCrashDumpPathWLen;
extern std::array<wchar_t, detail::nativeCrashPathMax> nativeCrashMiniDumpPathW;
extern std::size_t nativeCrashMiniDumpPathWLen;
extern std::int32_t nativeCrashHandlerActive;

} // namespace nijigenerate::utils::crashdump
