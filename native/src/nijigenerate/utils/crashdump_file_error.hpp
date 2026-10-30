#pragma once

#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <cstdint>
#include <cstddef>

namespace nijigenerate::utils::crashdump::detail {
[[noreturn]] inline void failCrashFile(compat::RetainedStringSlice path,
    std::uint32_t code, std::size_t line) {
    compat::FileException error(path.retainedString(), code);
    error.file = compat::RetainedString::fromStaticLiteral("std/file.d");
    error.line = line;
    throw error;
}
} // namespace nijigenerate::utils::crashdump::detail
