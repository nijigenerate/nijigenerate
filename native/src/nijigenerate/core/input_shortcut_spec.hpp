#pragma once

#include <nijigenerate/platform/shortcut_policy.hpp>
#include <array>
#include <cstddef>
#include <string_view>

namespace nijigenerate::core::input::detail {

template<std::size_t N> struct ShortcutLiteral {
    char bytes[N];
    consteval ShortcutLiteral(const char (&value)[N]) {
        for (std::size_t i = 0; i < N; ++i) bytes[i] = value[i];
    }
    constexpr std::string_view view() const { return {bytes, N - 1}; }
};

template<std::size_t N> struct ShortcutText {
    std::array<char, N> bytes{};
    std::size_t length{};
    constexpr std::string_view view() const { return {bytes.data(), length}; }
};

constexpr std::string_view shortcutToken(std::string_view token) {
    if constexpr (platform::shortcutAppleLabels) {
        if (token == "Super") return "\xEE\xAB\xA7";
        if (token == "Alt") return "\xEE\xAB\xA8";
        if (token == "Ctrl") return "\xEE\xAB\xA6";
    }
    return token;
}

template<ShortcutLiteral Spec> consteval auto formatShortcutSpec() {
    constexpr auto input = Spec.view();
    // Fixed capacity covers all platform labels without dynamic storage.
    ShortcutText<input.size() * 3 + 1> result;
    bool first = true;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= input.size(); ++i) {
        if (i == input.size() || input[i] == '-') {
            const auto token = shortcutToken(input.substr(start, i - start));
            if (!first) result.bytes[result.length++] = '+';
            for (char byte : token) result.bytes[result.length++] = byte;
            first = false;
            start = i + 1;
        }
    }
    return result;
}

inline constexpr auto ngTokSuper = formatShortcutSpec<"Super">();
inline constexpr auto ngTokAlt = formatShortcutSpec<"Alt">();
inline constexpr auto ngTokCtrl = formatShortcutSpec<"Ctrl">();
inline constexpr auto ngTokShift = formatShortcutSpec<"Shift">();

} // namespace nijigenerate::core::input::detail
