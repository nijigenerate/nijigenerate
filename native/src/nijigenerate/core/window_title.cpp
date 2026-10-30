// Derived from the independent core/window.d title bodies. BSD-2-Clause.
#include "window.hpp"
#include "window_state_internal.hpp"
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/io/save.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::core {

void ngRefreshWindowTitle() {
    auto& state = detail::windowState();
    // The selected native profile links mandatory SDL3. A missing shared SDK
    // prevents process entry; there is no separate D dynamic-loader flag.
    if (!state.window) return;

    if (!state.windowSubtitle.empty()) {
        const char* mark = io::save::incIsProjectModified() ? " *" : "";
        auto* window = state.window;
        // Read the current descriptor after the query, before concatenation.
        const auto subtitle = state.windowSubtitle;
        compat::RetainedString text;
        try {
            std::string bytes("nijigenerate - ");
            if (!subtitle.empty()) bytes.append(subtitle.data(), subtitle.size());
            bytes.append(mark);
            text = compat::RetainedString(std::move(bytes));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        const compat::SourceCString title{compat::RetainedStringSlice(text)};
        compat::detail::ngSourceCStringPostconditions(text.view(), title.data());
        SDL_SetWindowTitle(window, title.data());
    } else {
        SDL_SetWindowTitle(state.window, "nijigenerate");
    }
}

void ngSetWindowTitle(compat::RetainedStringSlice subtitle) {
    auto& state = detail::windowState();
    state.windowSubtitle = subtitle;
    state.lastWindowModified = io::save::incIsProjectModified();
    ngRefreshWindowTitle();
}

void ngUpdateWindowTitleTick() {
    const bool modified = io::save::incIsProjectModified();
    auto& state = detail::windowState();
    if (modified != state.lastWindowModified) {
        state.lastWindowModified = modified;
        ngRefreshWindowTitle();
    }
}

} // namespace nijigenerate::core
