#pragma once
// Independent installed Phobos std.path compatibility; same implementation as depthmap_psd_names.
#include <string_view>
namespace nijigenerate::windows::kramerge::detail {
inline bool directorySeparator(char value) noexcept {
#if defined(_WIN32)
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}

inline bool nameSeparator(char value) noexcept {
#if defined(_WIN32)
    return directorySeparator(value) || value == ':';
#else
    return directorySeparator(value);
#endif
}

inline bool uncPath(std::string_view path) noexcept {
    return path.size() >= 3 && directorySeparator(path[0]) &&
        directorySeparator(path[1]) && !directorySeparator(path[2]);
}

inline std::string_view baseName(std::string_view path) noexcept {
    auto remaining = path;
#if defined(_WIN32)
    if (path.size() >= 2 && path[1] == ':') remaining.remove_prefix(2);
    else if (uncPath(path)) {
        std::size_t end = 3;
        while (end < path.size() && !directorySeparator(path[end])) ++end;
        if (end < path.size()) {
            auto share = end;
            do { ++share; } while (share < path.size() && directorySeparator(path[share]));
            if (share < path.size()) {
                do { ++share; } while (share < path.size() && !directorySeparator(path[share]));
                end = share;
            }
        }
        remaining.remove_prefix(end);
    }
    if (remaining.empty()) return uncPath(path) ? path.substr(0, 1) : std::string_view{};
#else
    if (remaining.empty()) return {};
#endif
    auto end = remaining.size();
    while (end && directorySeparator(remaining[end - 1])) --end;
    if (!end) return remaining.substr(0, 1);
    auto start = end;
    while (start && !nameSeparator(remaining[start - 1])) --start;
    return remaining.substr(start, end - start);
}

}
