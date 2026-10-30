#include "crashdump_posix_state.hpp"
#include "crashdump_posix_internal.hpp"
#include <algorithm>
#include <cstring>

namespace nijigenerate::utils::crashdump {
namespace {
constexpr auto initialCrashPath() {
    std::array<std::uint8_t, nativeCrashPathMax> result{};
    result.fill(0xff);
    return result;
}
} // namespace

constinit std::array<std::uint8_t, nativeCrashPathMax> nativeCrashDumpPath = initialCrashPath();
constinit std::size_t nativeCrashDumpPathLen = 0;
constinit std::array<std::uint8_t, nativeCrashPathMax> nativeCrashExePath = initialCrashPath();
constinit std::size_t nativeCrashExePathLen = 0;
constinit std::int32_t nativeCrashHandlerActive = 0;
constinit thread_local void* nativeSignalStack = nullptr;

namespace detail {
void copyNativeCrashDumpPath(std::string_view path) {
    const auto count = std::min(path.size(), nativeCrashPathMax - 1);
    if (count != 0) std::memmove(nativeCrashDumpPath.data(), path.data(), count);
    nativeCrashDumpPath[count] = 0;
    nativeCrashDumpPathLen = count;
}
} // namespace detail
} // namespace nijigenerate::utils::crashdump
