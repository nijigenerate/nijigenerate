// Phobos time/path behavior: see native/port/licenses/phobos-crash-path.txt.
#include "crashdump.hpp"
#include "crashdump_internal.hpp"
#include <nijigenerate/compat/phobos_local_timestamp.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::utils::crashdump {
namespace detail {
std::string ngCurrentCrashTimestamp() {
    const auto timestamp = compat::phobos::ngLocalIsoTimestamp();
    return allocateCrashText([&] { return std::string(timestamp.view()); });
}
std::string ngJoinCrashPath(std::string_view directory, std::string_view leaf) {
    // The byte adapter borrows only for this call; buildPath owns its new result.
    const std::array<compat::RetainedString, 2> segments{
        compat::RetainedString::fromBorrowedView(directory), compat::RetainedString::fromBorrowedView(leaf)};
    const auto result = compat::phobos::ngBuildPath(segments);
    return allocateCrashText([&] { return std::string(result.view()); });
}
}

compat::RetainedStringSlice genCrashDumpPath(compat::RetainedStringSlice filename) {
    auto timestamp = compat::phobos::ngLocalIsoTimestamp();
#if defined(_WIN32)
    auto translated = detail::allocateCrashText([&] { return std::string(timestamp.view()); });
    for (auto& character : translated) {
        if (std::string_view("<>:\"/\\|?*").find(character) != std::string_view::npos) character = '-';
    }
    timestamp = detail::allocateCrashText([&] { return compat::RetainedString(std::move(translated)); });
#endif
    const auto directory = getCrashDumpDir();
    auto leaf = detail::allocateCrashText([&] {
        std::string bytes(filename.view());
        bytes += '-';
        bytes.append(timestamp.view());
        bytes += ".txt";
        return compat::RetainedString(std::move(bytes));
    });
    const std::array<compat::RetainedString, 2> segments{
        directory.retainedString(), std::move(leaf)};
    return compat::phobos::ngBuildPath(segments);
}

} // namespace nijigenerate::utils::crashdump
