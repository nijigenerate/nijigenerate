#include "file_move.hpp"
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace nijigenerate::platform {
void ngRenameReplacingExisting(const std::filesystem::path& source, const std::filesystem::path& destination) {
#ifdef _WIN32
    // Match std.file.rename's existing-destination replacement on Windows.
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING))
        throw std::filesystem::filesystem_error("rename", source, destination,
            std::error_code(static_cast<int>(GetLastError()), std::system_category()));
#else
    std::filesystem::rename(source, destination);
#endif
}
}
