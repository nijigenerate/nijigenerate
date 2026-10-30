#include "crashdump_windows_state.hpp"

namespace nijigenerate::utils::crashdump {
namespace {
constexpr auto initialCrashPath() {
    std::array<wchar_t, detail::nativeCrashPathMax> result{};
    result.fill(static_cast<wchar_t>(0xffff));
    return result;
}
} // namespace

constinit std::array<wchar_t, detail::nativeCrashPathMax> nativeCrashDumpPathW = initialCrashPath();
constinit std::size_t nativeCrashDumpPathWLen = 0;
constinit std::array<wchar_t, detail::nativeCrashPathMax> nativeCrashMiniDumpPathW = initialCrashPath();
constinit std::size_t nativeCrashMiniDumpPathWLen = 0;
constinit std::int32_t nativeCrashHandlerActive = 0;

} // namespace nijigenerate::utils::crashdump
