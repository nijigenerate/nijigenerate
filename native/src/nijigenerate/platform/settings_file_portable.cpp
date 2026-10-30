// Used Phobos std.file read body, D Language Foundation 2007-2011.
// BSL-1.0: native/port/licenses/phobos-psd-file.txt.
#include "settings_file.hpp"
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/phobos_conv_exception.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>

namespace nijigenerate::platform {
bool ngSettingsFileExists(std::string_view name) {
    return compat::phobos::ngFileExists(name);
}
std::string ngSettingsReadFileBytes(std::string_view name) {
    const compat::phobos::TempCStringBuffer<char> path(name);
    const auto fail = [&] {
        const auto code = static_cast<std::uint32_t>(errno);
        throw compat::FileException(compat::RetainedString(name), code);
    };
    const auto descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor == -1) fail();
    std::string data;
    compat::ngRunWithScopeExit([&] {
        struct stat attributes;
        if (::fstat(descriptor, &attributes) != 0) fail();
        const auto reported = attributes.st_size;
        if (reported < -1 || reported == (std::numeric_limits<decltype(attributes.st_size)>::max)())
            throw compat::PhobosConvOverflowException();
        const auto maximumInitial = (std::numeric_limits<std::size_t>::max)() / 2;
        const auto initial = reported == 0 ? std::size_t{4096} : static_cast<std::size_t>(
            (std::min<std::uintmax_t>)(static_cast<std::uintmax_t>(reported + 1), maximumInitial));
        data.resize(initial);
        std::size_t used = 0;
        for (;;) {
            const auto actual = ::read(descriptor, data.data() + used, data.size() - used);
            if (actual == -1) fail();
            if (actual == 0) break;
            used += static_cast<std::size_t>(actual);
            if (used < data.size()) continue;
            constexpr std::size_t increment = 16384;
            data.append(increment, '\0');
        }
        data.resize(used);
    }, [&] { (void)::close(descriptor); });
    return data;
}
}
