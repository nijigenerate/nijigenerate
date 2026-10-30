#include "browser.hpp"
#include <nijigenerate/compat/phobos_temp_cstring.hpp>
#include <cstdio>
#include <cstdlib>
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#else
#error Unsupported browser launch platform
#endif

namespace nijigenerate::platform {
void ngBrowse(std::string_view link) {
#if defined(_WIN32)
    const compat::phobos::TempCStringBuffer<wchar_t> wide(link);
    // Phobos intentionally ignores ShellExecuteW's result, including failures.
    (void)ShellExecuteW(nullptr, L"open", wide.c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
#else
    // A null descriptor stays null; a nonnull empty descriptor stays an empty C string.
    const compat::phobos::TempCStringBuffer<char> target(link);
    char* browser = nullptr;
    const char* command = std::getenv("BROWSER");
    if (command) {
        browser = ::strdup(command);
        command = browser;
    } else {
#if defined(__APPLE__)
        command = "open";
#else
        command = "xdg-open";
#endif
    }
    const char* arguments[] = {command, target.c_str(), nullptr};
    if (::fork() == 0) {
        ::execvp(arguments[0], const_cast<char* const*>(arguments));
        std::perror(arguments[0]);
        ::_exit(1);
    }
    if (browser) std::free(browser);
#endif
}
}
