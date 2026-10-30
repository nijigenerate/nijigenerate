// Derived from the independent POSIX crashdump.d ShowMessageBox. BSD-2-Clause.
#include "crashdump.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/platform/tinyfiledialogs.hpp>
#include <array>
#include <utility>

namespace nijigenerate::utils::crashdump::detail {

void ShowMessageBox(compat::RetainedStringSlice dumpPath) {
    using namespace core::translation;
    const auto title = ngTranslateCString("nijigenerate Crashdump");
    const auto pattern = ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "The application has unexpectedly crashed.\n\n"
        "If autosave is enabled, open File \xe2\x86\x92 Recent \xe2\x86\x92 Autosaves to recover your work.\n\n"
        "A crash report was written to:\n%s\n\nPlease attach it when filing an issue:\n"
        "https://github.com/nijigenerate/nijigenerate/issues"));
    const auto path = dumpPath.empty() ? getCrashDumpDir() : dumpPath;
    const std::array<TypedFormatArgument, 1> arguments{ngFormatArgument(path)};
    auto message = ngFormatTyped(pattern.view(), arguments);
    const auto input = allocateCrashText([&] { return compat::RetainedString(std::move(message)); });
    const compat::SourceCString text{compat::RetainedStringSlice(input)};
    compat::detail::ngSourceCStringPostconditions(input.view(), text.data());
    (void)platform::tinyfiledialogs::tinyfd_messageBox(title.data(), text.data(), "ok", "error", 1);
}

} // namespace nijigenerate::utils::crashdump::detail
