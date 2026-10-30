#include "crashdump.hpp"
#include "crashdump_internal.hpp"
#include "crashdump_file_error.hpp"
#include <nijigenerate/compat/phobos_temp_cstring.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <pwd.h>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace nijigenerate::utils::crashdump {
namespace {
compat::RetainedStringSlice combineHome(const char* home, compat::RetainedStringSlice path,
                                        std::size_t suffix) {
#ifndef NDEBUG
    if (home == nullptr) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString{}, compat::RetainedString::fromStaticLiteral("std/path.d"), 4000);
    if (path.empty()) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString{}, compat::RetainedString::fromStaticLiteral("std/path.d"), 4001);
#endif
    auto length = std::strlen(home);
    const auto text = path.view();
    const bool trailing = length != 0 && home[length - 1] == '/';
    if (suffix < text.size()) {
        if (trailing && (length > 1 || text[suffix] == '/')) --length;
        return detail::allocateCrashText([&] {
            std::string result(home, length);
            result.append(text.substr(suffix));
            return compat::RetainedString(std::move(result));
        });
    }
    if (trailing && length > 1) --length;
    if (length == 0) return {};
    return detail::allocateCrashText([&] { return compat::RetainedString(std::string(home, length)); });
}

// Preserve the installed Phobos expandTilde policy, including unchanged aliases.
compat::RetainedStringSlice expandCrashDumpTilde(compat::RetainedStringSlice path) {
    const auto text = path.view();
    if (text.empty() || text.front() != '~') return path;
    if (text.size() == 1 || text[1] == '/') {
        const auto home = std::getenv("HOME");
        return home ? combineHome(home, path, 1) : path;
    }
#if defined(__BIONIC__)
    return path;
#else
    const auto slash = text.find('/');
    const auto usernameEnd = slash == std::string_view::npos ? text.size() : slash;
    const auto username = detail::allocateCrashText([&] { return std::string(text.substr(1, usernameEnd - 1)); });
    const auto suffix = slash == std::string_view::npos ? text.size() + 1 : slash;
    std::vector<char> buffer;
    std::uint32_t increment = 5 * 1024;
    passwd entry{};
    for (;;) {
        detail::allocateCrashText([&] {
            buffer.resize(buffer.size() + increment, std::bit_cast<char>(std::uint8_t{0xff}));
        });
        passwd* found = nullptr;
        errno = 0;
        const auto result = ::getpwnam_r(username.c_str(), &entry, buffer.data(), buffer.size(), &found);
        if (result == 0) {
            if (found == &entry) return combineHome(entry.pw_dir, path, suffix);
            return path;
        }
        // The source deliberately examines errno, rather than the return code.
        switch (errno) {
        case ERANGE:
        case 0:
            break;
        case ENOENT:
        case ESRCH:
        case EBADF:
        case EPERM:
            return path;
        default:
            throw compat::ngStaticError<compat::OutOfMemoryError>(
                compat::RetainedString::fromStaticLiteral("std/path.d"), 4130);
        }
        const bool overflow = increment > std::numeric_limits<std::uint32_t>::max() / 2;
        increment *= 2;
        // The source uses constant-false assert, which remains unreachable in release.
        if (overflow) compat::ngAssertUnreachable(nullptr, "std/path.d", 4137);
    }
#endif
}
} // namespace

compat::RetainedStringSlice getCrashDumpDir() {
    if (!detail::crashDumpDirOverride.empty()) return detail::crashDumpDirOverride;
#if defined(__APPLE__)
    return expandCrashDumpTilde(compat::RetainedString::fromStaticLiteral("~/Library/Logs/"));
#elif defined(__linux__)
    const auto state = linuxStateHome();
    const auto path = detail::allocateCrashText([&] {
        std::string bytes(state.view());
        bytes += '/';
        return compat::RetainedString(std::move(bytes));
    });
    return expandCrashDumpTilde(path);
#else
    return expandCrashDumpTilde(compat::RetainedString::fromStaticLiteral("~"));
#endif
}

void mkdirCrashDumpDir() {
    const auto directory = getCrashDumpDir();
    if (compat::phobos::ngFileExists(directory.view())) return;
    {
        const compat::phobos::TempCStringBuffer<char> path{directory.view()};
        if (::mkdir(path.c_str(), 0777) != 0) {
            const auto error = errno;
            detail::failCrashFile(directory.empty() ? compat::RetainedStringSlice{} : directory,
                static_cast<std::uint32_t>(error), 2985);
        }
    }
#if defined(__linux__)
    const compat::phobos::TempCStringBuffer<char> path{directory.view()};
    if (::chmod(path.c_str(), 0700) != 0) {
        const auto error = errno;
        detail::failCrashFile(directory.empty() ? compat::RetainedStringSlice{} : directory,
            static_cast<std::uint32_t>(error), 2271);
    }
#endif
}

} // namespace nijigenerate::utils::crashdump
