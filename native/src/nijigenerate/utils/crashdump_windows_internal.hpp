#pragma once

#include "crashdump_windows_state.hpp"
#include "crashdump_windows_types.hpp"
#include "nijigenerate/compat/retained_string_slice.hpp"
#include <string>
#include <string_view>

namespace nijigenerate::utils::crashdump::detail {

std::wstring toNativeCrashUTF16(std::string_view bytes);
compat::RetainedStringSlice getDesktopDir();
void ShowMessageBox(std::string_view message, std::string_view title);
void notifyNativeCrashUser() noexcept;
void copyNativeCrashPath(std::array<wchar_t, nativeCrashPathMax>& target,
                         std::size_t& targetLength, std::string_view path);
bool validNativeHandle(NativeHandle handle) noexcept;
void writeAll(NativeHandle file, std::string_view text) noexcept;
void writeInt(NativeHandle file, std::uint64_t value) noexcept;
void writeHex(NativeHandle file, std::size_t value) noexcept;
NativeHandle createCrashFile(const wchar_t* path) noexcept;

} // namespace nijigenerate::utils::crashdump::detail
