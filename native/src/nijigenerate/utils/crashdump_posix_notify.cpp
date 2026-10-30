#include "crashdump.hpp"
#include "nijigenerate/core/translation.hpp"
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <string_view>
#include <utility>

namespace nijigenerate::utils::crashdump {
namespace detail {
// The canonical provider exists; its selected-library C-string contract is separate.
void ShowMessageBox(compat::RetainedStringSlice dumpPath);
} // namespace detail

namespace {
#if defined(__APPLE__) || defined(__linux__)
void writeCrashInstructions(std::string_view text) {
    providers::mcp::stdio::standardOutput().writeln(text);
}
#endif
} // namespace

void notifyCrashUser(compat::RetainedStringSlice dumpPath) {
#if defined(__APPLE__)
    const auto instructions = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "\n\n\n===   nijigenerate has crashed   ===\n"
        "Please send us the nijigenerate-crashdump.txt file in ~/Library/Logs\n"
        "Attach the file as a git issue @ https://github.com/nijigenerate/nijigenerate/issues"));
    writeCrashInstructions(instructions.view());
    detail::ShowMessageBox(std::move(dumpPath));
#elif defined(__linux__)
    const auto instructions = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "\n\n\n===   nijigenerate has crashed   ===\n"
        "Please send us the nijigenerate-crashdump.txt file in your log directory, XDG_STATE_HOME. "
        "For Flatpak, this is in ~/.var/app/nijigenerate.nijigenerate.\n"
        "Attach the file as a git issue @ https://github.com/nijigenerate/nijigenerate/issues"));
    writeCrashInstructions(instructions.view());
    detail::ShowMessageBox(std::move(dumpPath));
#else
    (void)dumpPath;
#endif
}

} // namespace nijigenerate::utils::crashdump
