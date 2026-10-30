// Phobos std.internal.cstring tempCString, BSL-1.0.
// Copyright Denis Shelomovskij 2013-2014.
// See port/licenses/phobos-source-c-string.txt.
#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/phobos_temp_cstring.hpp>

namespace nijigenerate::utils::crashdump::detail {
// Preserve existing caller spelling while consuming the one actual canonical
// tempCString provider, including its allocation errors and spill-copy policy.
template<class Char> class CrashFileName {
public:
    explicit CrashFileName(compat::RetainedStringSlice source) : native_(source.view()) {}
    CrashFileName(const CrashFileName&) = delete;
    CrashFileName& operator=(const CrashFileName&) = delete;
    const Char* c_str() const noexcept { return native_.c_str(); }

private:
    compat::phobos::TempCStringBuffer<Char> native_;
};
} // namespace nijigenerate::utils::crashdump::detail
