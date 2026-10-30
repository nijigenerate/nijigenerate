#include "nijigenerate/core/window.hpp"
#include "nijigenerate/core/window_diagnostic_internal.hpp"
#include "nijigenerate/compat/phobos_stdio.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <string_view>

namespace nijigenerate::core {

namespace {
#if defined(NIJIGENERATE_WINDOW_DEBUG_IMGUI)
std::int32_t addIndent(std::int32_t value, std::int32_t increment) {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)
        + static_cast<std::uint32_t>(increment));
}
#endif
}

#if defined(NIJIGENERATE_WINDOW_ORDINARY_DEBUG)
namespace detail {
void writeWindowGLInformation(const std::array<std::string_view, 4>& values, std::uint32_t contextVersion) {
    providers::mcp::stdio::standardOutput().withTextWriter(
        [&](providers::mcp::stdio::FileTextWriter& writer) {
            writer.put(std::string_view("GLInfo:\n\t"));
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i) writer.put(std::string_view("\n\t"));
                writer.put(values[i]);
            }
            // This is the actual selected SDL3 context diagnostic, not a loader enum.
            writer.put(std::string_view("\n\tgls=gl"));
            std::array<char, 10> digits;
            const auto converted = std::to_chars(digits.data(), digits.data() + digits.size(), contextVersion / 10);
            writer.put(std::string_view(digits.data(), static_cast<std::size_t>(converted.ptr - digits.data())));
            writer.put(static_cast<unsigned char>('\n'));
        });
}
}
#endif

void ngDebugImGuiState(compat::RetainedStringSlice message, int indent) {
#if defined(NIJIGENERATE_WINDOW_DEBUG_IMGUI)
    static_assert(sizeof(int) == sizeof(std::int32_t));
    thread_local std::int32_t currentIndent = 0;
    const char* flag = "  ";
    if (indent > 0) {
        currentIndent = addIndent(currentIndent, indent);
        flag = ">>";
    } else if (indent < 0) {
        flag = "<<";
    }

    auto* window = ImGui::GetCurrentWindow();
    // The source evaluates to!string(win.Name) before entering writefln.
    const auto* name = compat::ngRequireObject(window)->Name;
    compat::RetainedString windowName;
    if (name && *name) windowName = detail::retainWindowText(std::string_view(name));
    const auto spaces = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(currentIndent) * 2u);
    providers::mcp::stdio::standardOutput().withTextWriter(
        [&](providers::mcp::stdio::FileTextWriter& writer) {
            // Source repeat(char) uses unchecked character put, not bulk put.
            for (auto remaining = static_cast<std::size_t>(spaces); remaining != 0; --remaining)
                writer.put(static_cast<unsigned char>(' '));
            writer.put(std::string_view(flag));
            if (!message.empty()) writer.put(std::string_view(message.data(), message.size()));
            writer.put(std::string_view(" ["));
            writer.put(windowName.view());
            writer.put(std::string_view("]"));
            writer.put(static_cast<unsigned char>('\n'));
        });

    if (indent < 0) {
        currentIndent = addIndent(currentIndent, indent);
        if (currentIndent < 0) {
#if defined(NIJIGENERATE_WINDOW_ORDINARY_DEBUG)
            providers::mcp::stdio::standardOutput().writeln("ERROR: dedented too far!");
#endif
            currentIndent = 0;
        }
    }
#else
    (void)message;
    (void)indent;
#endif
}

} // namespace nijigenerate::core
