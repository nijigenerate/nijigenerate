#include "depthmap_psd_names.hpp"
#include "detail/psd_depth_allocation.hpp"
#include <common/unicode_lowercase.hpp>
#include <nijigenerate/compat/array_range_error.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::io::depthmap_psd {
namespace {

// Standard-library compatibility below follows the installed Boost-licensed
// Phobos std.path, std.uni.toCase and std.utf replacement-decoder algorithms.
// The generated data header retains the upstream notice and full license.
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

bool uncPath(std::string_view path) noexcept {
    return path.size() >= 3 && directorySeparator(path[0]) &&
        directorySeparator(path[1]) && !directorySeparator(path[2]);
}

std::string_view baseName(std::string_view path) noexcept {
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

std::string_view stripExtension(std::string_view path) noexcept {
    for (auto end = path.size(); end; ) {
        const auto index = --end;
        if (nameSeparator(path[index])) break;
        if (path[index] == '.' && index && !nameSeparator(path[index - 1])) return path.substr(0, index);
    }
    return path;
}

compat::RetainedString phobosLower(compat::RetainedString input) {
    return ::nicxlive::core::common::unicode::toLower(std::move(input));
}

} // namespace

compat::RetainedString ngPsdDepthLowerString(compat::RetainedString text) {
    return phobosLower(std::move(text));
}

compat::RetainedStringSlice ngPsdDepthNormalizeLayerName(compat::RetainedStringSlice name) {
    const auto stem = stripExtension(baseName(name.view()));
    compat::RetainedString lowerInput;
    if (stem.data()) {
        const auto offset = static_cast<std::size_t>(stem.data() - name.data());
        lowerInput = name.retainedString().subslice(offset, offset + stem.size());
    }
    auto normalized = phobosLower(std::move(lowerInput));
    std::string_view bytes = normalized.view();
    if (!bytes.empty() && bytes.front() == '/') bytes.remove_prefix(1);
    std::string compact;
    bool previousSeparator = false;
    for (char value : bytes) {
        const bool separator = value == ' ' || value == '_' || value == '-';
        if (separator) {
            if (!previousSeparator && !compact.empty())
                detail::allocateImportBytes([&] { compact.push_back('-'); });
            previousSeparator = true;
            continue;
        }
        detail::allocateImportBytes([&] { compact.push_back(value); });
        previousSeparator = false;
    }
    if (!compact.empty() && compact.back() == '-') compact.pop_back();
    if (compact.empty()) return {};
    const auto retained = detail::allocateImportBytes([&] { return compat::RetainedString(std::move(compact)); });
    return compat::RetainedStringSlice(retained);
}

compat::RetainedStringSlice ngPsdDepthLayerIdentityKey(compat::RetainedStringSlice name,
        std::int32_t left, std::int32_t top, std::int32_t width, std::int32_t height) {
    const auto normalized = ngPsdDepthNormalizeLayerName(std::move(name));
    std::string result;
    if (!normalized.empty()) detail::allocateImportBytes([&] {
        result.assign(normalized.data(), normalized.size());
    });
    for (const auto value : std::array<std::int32_t, 4>{left, top, width, height}) {
        detail::allocateImportBytes([&] { result.push_back(':'); });
        // A signed32 decimal needs at most11 bytes, including the minus sign.
        char digits[12];
        const auto converted = std::to_chars(digits, digits + sizeof(digits), value);
        detail::allocateImportBytes([&] { result.append(digits, static_cast<std::size_t>(converted.ptr - digits)); });
    }
    const auto retained = detail::allocateImportBytes([&] { return compat::RetainedString(std::move(result)); });
    return compat::RetainedStringSlice(retained);
}

} // namespace nijigenerate::io::depthmap_psd
