#include "crashdump_windows_internal.hpp"
#include "crashdump.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/platform/environment.hpp"
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/druntime_utf8.hpp>
#include <utility>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

namespace nijigenerate::utils::crashdump::detail {
namespace {
// Unlike toUTF16/byUTF, cross-width toUTF16z decodes foreach(dchar) strictly.
std::wstring messageToUTF16z(std::string_view bytes) {
    std::wstring result;
    compat::DruntimeUtf8Cursor cursor(bytes);
    while (!cursor.empty()) {
        auto scalar = static_cast<std::uint32_t>(cursor.take());
        if (scalar <= 0xffff) allocateCrashText([&] { result.push_back(static_cast<wchar_t>(scalar)); });
        else {
            scalar -= 0x10000;
            allocateCrashText([&] { result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10))); });
            allocateCrashText([&] { result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff))); });
        }
    }
    allocateCrashText([&] { result.push_back(0); });
    return result;
}

std::string desktopToUTF8(std::wstring_view text) {
    std::string result;
    for (std::size_t index = 0; index < text.size();) {
        auto scalar = static_cast<std::uint32_t>(text[index++]);
        if (scalar >= 0xd800 && scalar <= 0xdbff) {
            if (index == text.size()) scalar = 0xfffd;
            else {
                const auto next = static_cast<std::uint32_t>(text[index++]);
                scalar = next >= 0xdc00 && next <= 0xdfff ?
                    ((scalar - 0xd7c0) << 10) + (next - 0xdc00) : 0xfffd;
            }
        } else if (scalar >= 0xdc00 && scalar <= 0xdfff) scalar = 0xfffd;
        if (scalar <= 0x7f) allocateCrashText([&] { result.push_back(static_cast<char>(scalar)); });
        else if (scalar <= 0x7ff) {
            allocateCrashText([&] { result.push_back(static_cast<char>(0xc0 | (scalar >> 6))); });
            allocateCrashText([&] { result.push_back(static_cast<char>(0x80 | (scalar & 0x3f))); });
        } else if (scalar <= 0xffff) {
            allocateCrashText([&] { result.push_back(static_cast<char>(0xe0 | (scalar >> 12))); });
            allocateCrashText([&] { result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f))); });
            allocateCrashText([&] { result.push_back(static_cast<char>(0x80 | (scalar & 0x3f))); });
        } else {
            allocateCrashText([&] { result.push_back(static_cast<char>(0xf0 | (scalar >> 18))); });
            allocateCrashText([&] { result.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3f))); });
            allocateCrashText([&] { result.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3f))); });
            allocateCrashText([&] { result.push_back(static_cast<char>(0x80 | (scalar & 0x3f))); });
        }
    }
    return result;
}
} // namespace

compat::RetainedStringSlice getDesktopDir() {
    wchar_t desktop[MAX_PATH]{};
    if (::SHGetSpecialFolderPathW(HWND_DESKTOP, desktop, CSIDL_DESKTOP, nativeFalse)) {
        auto path = desktopToUTF8(std::wstring_view(desktop));
        if (!path.empty()) {
            if (compat::phobos::ngFileExists(path))
                return allocateCrashText([&] { return compat::RetainedString(std::move(path)); });
        }
    }
    // The nested fallback argument is evaluated even when TEMP is present.
    auto fallback = platform::ngGetEnvironmentVariableRetained("TMP");
    if (fallback.isNull()) fallback = compat::RetainedString::fromStaticLiteral(".");
    const auto temporary = platform::ngGetEnvironmentVariableRetained("TEMP");
    if (!temporary.isNull()) return temporary;
    return fallback;
}

void ShowMessageBox(std::string_view message, std::string_view title) {
    const auto encodedMessage = messageToUTF16z(message);
    const auto encodedTitle = messageToUTF16z(title);
    (void)::MessageBoxW(nullptr, encodedMessage.c_str(), encodedTitle.c_str(), 0);
}

void notifyNativeCrashUser() noexcept {
    static constexpr wchar_t title[] = L"nijigenerate Crashdump";
    static constexpr wchar_t message[] =
        L"The application has unexpectedly crashed.\n\n"
        L"Crash dump files were written to your desktop.\n"
        L"Please attach both the nijigenerate-native-crashdump text file and the .dmp file when filing an issue.";
    (void)::MessageBoxW(nullptr, message, title, nativeMessageBoxOK | nativeMessageBoxIconError);
}

} // namespace nijigenerate::utils::crashdump::detail

namespace nijigenerate::utils::crashdump {

void notifyCrashUser(compat::RetainedStringSlice) {
    const auto message = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "The application has unexpectedly crashed\n"
        "Please send the developers the nijigenerate-crashdump.txt which has been put on your desktop\n"
        "Via https://github.com/nijigenerate/nijigenerate/issues"));
    const auto title = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("nijigenerate Crashdump"));
    detail::ShowMessageBox(message.view(), title.view());
}

} // namespace nijigenerate::utils::crashdump
