// Native CRT boundary for the actual source/app.d implementation in app.cpp.
// BSD-2-Clause; see the repository LICENSE.
#include "app.hpp"
#define SDL_MAIN_HANDLED 1
#include <SDL3/SDL_main.h>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
template<class Character, class Convert>
int runEditor(int count, Character* const* values, Convert convert) {
    using namespace nijigenerate;
    SDL_SetMainReady();
    std::vector<compat::RetainedStringSlice> arguments;
    if (count > 0) arguments.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) arguments.emplace_back(convert(values[index]));
    return app::ngRunEditor(app::Arguments(std::move(arguments)));
}

#if defined(_WIN32)
nijigenerate::compat::RetainedString windowsArgument(const wchar_t* value) {
    static_assert(sizeof(wchar_t) == 2);
    using Cursor = nijigenerate::compat::phobos::detail::UnicodeCursor<std::wstring_view, wchar_t>;
    Cursor cursor{std::wstring_view(value)};
    std::string bytes;
    while (!cursor.empty()) {
        const auto scalar = static_cast<std::uint32_t>(cursor.front());
        if (scalar <= 0x7f) bytes.push_back(static_cast<char>(scalar));
        else if (scalar <= 0x7ff) {
            bytes.push_back(static_cast<char>(0xc0 | (scalar >> 6)));
            bytes.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        } else if (scalar <= 0xffff) {
            bytes.push_back(static_cast<char>(0xe0 | (scalar >> 12)));
            bytes.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            bytes.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        } else {
            bytes.push_back(static_cast<char>(0xf0 | (scalar >> 18)));
            bytes.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3f)));
            bytes.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f)));
            bytes.push_back(static_cast<char>(0x80 | (scalar & 0x3f)));
        }
        cursor.popFront();
    }
    return nijigenerate::compat::RetainedString(std::move(bytes));
}
#endif
}

#if defined(_WIN32)
int wmain(int count, wchar_t* values[]) {
    return runEditor(count, values, windowsArgument);
}
#else
int main(int count, char* values[]) {
    return runEditor(count, values, [](const char* value) {
        return nijigenerate::compat::RetainedString(std::string_view(value));
    });
}
#endif
