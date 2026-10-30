#include "crashdump_windows_internal.hpp"
#include "crashdump.hpp"
#include <algorithm>
#include <limits>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace nijigenerate::utils::crashdump::detail {
static_assert(sizeof(NativeDWord) == sizeof(::DWORD));
static_assert(sizeof(NativeBool) == sizeof(::BOOL));
static_assert(sizeof(NativeLong) == sizeof(::LONG));
static_assert(sizeof(NativeHandle) == sizeof(::HANDLE));

// Match Phobos byUTF replacement consumption before applying the path bound.
std::wstring toNativeCrashUTF16(std::string_view bytes) {
    std::wstring result;
    for (std::size_t offset = 0; offset < bytes.size();) {
        const auto first = static_cast<std::uint8_t>(bytes[offset]);
        std::uint32_t scalar = first;
        std::size_t consumed = 1;
        if (first >= 0x80) {
            scalar = 0xfffd;
            if ((first & 0xc0) == 0xc0) {
                auto control = static_cast<std::uint8_t>(first << 1);
                std::uint32_t decoded = first;
                for (std::size_t index = 1; index <= 3; ++index) {
                    if (offset + index == bytes.size()) { consumed = index; break; }
                    const auto next = static_cast<std::uint8_t>(bytes[offset + index]);
                    consumed = index + 1;
                    if ((next & 0xc0) != 0x80) break;
                    decoded = (decoded << 6) | (next & 0x3f);
                    control = static_cast<std::uint8_t>(control << 1);
                    if (!(control & 0x80)) {
                        constexpr std::uint32_t masks[] = {0x7f, 0x7ff, 0xffff, 0x1fffff};
                        decoded &= masks[index];
                        if ((decoded & ~masks[index - 1]) != 0 && decoded <= 0x10ffff &&
                            !(decoded >= 0xd800 && decoded <= 0xdfff)) scalar = decoded;
                        break;
                    }
                }
            }
        }
        offset += consumed;
        if (scalar <= 0xffff) allocateCrashText([&] { result.push_back(static_cast<wchar_t>(scalar)); });
        else {
            scalar -= 0x10000;
            allocateCrashText([&] { result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10))); });
            allocateCrashText([&] { result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff))); });
        }
    }
    return result;
}

void copyNativeCrashPath(std::array<wchar_t, nativeCrashPathMax>& target,
                         std::size_t& targetLength, std::string_view path) {
    const auto encoded = toNativeCrashUTF16(path);
    const auto count = std::min(encoded.size(), nativeCrashPathMax - 1);
    std::copy_n(encoded.begin(), count, target.begin());
    target[count] = 0;
    targetLength = count;
}

bool validNativeHandle(void* handle) noexcept {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

void writeAll(void* file, std::string_view text) noexcept {
    if (!validNativeHandle(file) || text.empty()) return;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto remaining = text.size() - pos;
        const auto chunk = static_cast<DWORD>(std::min(remaining,
            static_cast<std::size_t>(std::numeric_limits<NativeDWord>::max())));
        DWORD written = 0;
        if (!::WriteFile(file, text.data() + pos, chunk, &written, nullptr) || written == 0) return;
        pos += written;
    }
}

void writeInt(void* file, std::uint64_t value) noexcept {
    char buffer[32];
    std::size_t pos = sizeof(buffer);
    do {
        buffer[--pos] = static_cast<char>('0' + value % 10);
        value /= 10;
    } while (value);
    writeAll(file, std::string_view(buffer + pos, sizeof(buffer) - pos));
}

void writeHex(void* file, std::size_t value) noexcept {
    constexpr char digits[] = "0123456789ABCDEF";
    char buffer[2 + sizeof(std::size_t) * 2];
    buffer[0] = '0';
    buffer[1] = 'x';
    for (std::size_t index = 0; index < sizeof(std::size_t) * 2; ++index) {
        const auto shift = (sizeof(std::size_t) * 2 - 1 - index) * 4;
        buffer[2 + index] = digits[(value >> shift) & 0xf];
    }
    writeAll(file, std::string_view(buffer, sizeof(buffer)));
}

void* createCrashFile(const wchar_t* path) noexcept {
    return ::CreateFileW(path, nativeGenericWrite, nativeFileShareRead, nullptr,
                         nativeCreateAlways, nativeFileAttributeNormal, nullptr);
}

} // namespace nijigenerate::utils::crashdump::detail
