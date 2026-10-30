#include "autosave_path_policy.hpp"
#include "path_policy.hpp"

namespace nijigenerate::platform {
namespace {
bool directorySeparator(char value) noexcept {
#if defined(_WIN32)
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}

bool nameSeparator(char value) noexcept {
#if defined(_WIN32)
    return directorySeparator(value) || value == ':';
#else
    return directorySeparator(value);
#endif
}

#if defined(_WIN32)
bool isUnc(std::string_view path) noexcept {
    return path.size() >= 3 && directorySeparator(path[0]) && directorySeparator(path[1]) &&
        !directorySeparator(path[2]);
}

std::size_t uncPrefixLength(std::string_view path) noexcept {
    std::size_t end = 3;
    while (end < path.size() && !directorySeparator(path[end])) ++end;
    if (end < path.size()) {
        auto share = end;
        while (share < path.size() && directorySeparator(path[share])) ++share;
        if (share < path.size()) {
            while (share < path.size() && !directorySeparator(path[share])) ++share;
            end = share;
        }
    }
    return end;
}
#endif

std::size_t rootLength(std::string_view path) noexcept {
    if (path.empty()) return 0;
#if defined(_WIN32)
    if (directorySeparator(path[0])) return isUnc(path) ? uncPrefixLength(path) : 1;
    if (path.size() >= 3 && path[1] == ':' && directorySeparator(path[2])) return 3;
#else
    if (directorySeparator(path[0])) return 1;
#endif
    return 0;
}
}

std::string_view ngAutosaveBaseName(std::string_view path) noexcept {
    auto remaining = path;
#if defined(_WIN32)
    if (path.size() >= 2 && path[1] == ':') remaining.remove_prefix(2);
    else if (isUnc(path)) remaining.remove_prefix(uncPrefixLength(path));
    if (remaining.empty()) return isUnc(path) ? path.substr(0, 1) : std::string_view{};
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

std::string ngAutosaveBuildDirectory(std::string_view config, std::string_view basename) {
    auto result = ngJoinRelativePath(std::string(config), "autosaves");
    if (basename.empty()) return result;
    if (directorySeparator(basename.front())) {
        // A rooted final component discards directories but keeps the Windows root.
        auto retained = rootLength(result);
        if (retained && directorySeparator(result[retained - 1])) --retained;
        result.resize(retained);
        result.append(basename);
        return result;
    }
    return ngJoinRelativePath(std::move(result), basename);
}

std::string_view ngAutosaveStripExtension(std::string_view path) noexcept {
    for (auto end = path.size(); end; ) {
        const auto index = --end;
        if (nameSeparator(path[index])) break;
        if (path[index] == '.' && index && !nameSeparator(path[index - 1]))
            return path.substr(0, index);
    }
    return path;
}
} // namespace nijigenerate::platform
