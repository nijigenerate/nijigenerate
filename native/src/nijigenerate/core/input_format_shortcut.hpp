#pragma once

#include "input_shortcut_spec.hpp"
#include "settings.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::core::input {

namespace detail {
// Call only around builtin byte storage, never around settings or caller code.
template<class Operation> inline auto allocateShortcutBytes(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Text> inline compat::RetainedString formatShortcut(bool ctrl, bool alt, bool shift, bool superKey,
    Text key) {
    if constexpr (platform::shortcutAppleLabels) {
        if (settings::incSettingsGet<bool>(compat::RetainedString::fromStaticLiteral("MacSwapCmdCtrl"), false))
            std::swap(ctrl, superKey);
    }
    std::string result;
    auto append = [&](std::string_view token) {
        allocateShortcutBytes([&] {
            if (!result.empty()) result.push_back('+');
            result.append(token);
        });
    };
    if (ctrl) append(detail::ngTokCtrl.view());
    if (alt) append(detail::ngTokAlt.view());
    if (shift) append(detail::ngTokShift.view());
    if (superKey) append(detail::ngTokSuper.view());
    if (!key.empty()) append(std::string_view(key.data(), key.size()));
    if (result.empty()) return {};
    return allocateShortcutBytes([&] { return compat::RetainedString(std::move(result)); });
}
} // namespace detail

inline compat::RetainedString ngFormatShortcut(bool ctrl, bool alt, bool shift, bool superKey,
    compat::RetainedStringSlice key) {
    return detail::formatShortcut(ctrl, alt, shift, superKey, key);
}

// Explicit native views borrow their key storage for the complete call.
inline compat::RetainedString ngFormatShortcut(bool ctrl, bool alt, bool shift, bool superKey,
    std::string_view key) {
    return detail::formatShortcut(ctrl, alt, shift, superKey, key);
}

} // namespace nijigenerate::core::input
