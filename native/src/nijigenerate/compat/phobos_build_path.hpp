#pragma once
// Used finite char-string buildPath/chainPath bodies from Phobos std.path.
// Copyright the D Language Foundation and Phobos contributors. BSL-1.0;
// see port/licenses/phobos-crash-path.txt and installed std/path.d.
#include "retained_string.hpp"
#include "array_range_error.hpp"
#include <common/shared_array.hpp>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace nijigenerate::compat::phobos {
namespace build_path_detail {
inline bool separator(char value) noexcept {
#ifdef _WIN32
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}
#ifdef _WIN32
inline bool unc(std::string_view path) noexcept {
    return path.size() >= 3 && separator(path[0]) && separator(path[1]) && !separator(path[2]);
}
inline bool driveRoot(std::string_view path) noexcept {
    return path.size() >= 3 && path[1] == ':' && separator(path[2]);
}
inline std::size_t rootLength(std::string_view path) noexcept {
    if (path.empty()) return 0;
    if (separator(path[0])) {
        if (!unc(path)) return 1;
        std::size_t index = 3;
        while (index < path.size() && !separator(path[index])) ++index;
        if (index < path.size()) {
            auto share = index;
            do { ++share; } while (share < path.size() && separator(path[share]));
            if (share < path.size()) {
                do { ++share; } while (share < path.size() && !separator(path[share]));
                index = share;
            }
        }
        return index;
    }
    return driveRoot(path) ? 3 : 0;
}
#endif
inline void put(nicxlive::core::common::SharedArray<char>& buffer, std::size_t& index, char value) {
    if (index >= buffer.size()) throw ngStaticError<ArrayIndexError>(index, buffer.size(),
        RetainedString::fromStaticLiteral("std/path.d"), 1477);
    buffer.data()[index] = value;
    ++index;
}
} // namespace build_path_detail

inline RetainedString ngBuildPath(std::span<const RetainedString> segments) {
    if (segments.empty()) return {};
    std::size_t capacity = 0;
    for (const auto& segment : segments) capacity += segment.size() + 1;
    nicxlive::core::common::SharedArray<char> buffer(capacity, std::bit_cast<char>(std::uint8_t{0xff}));
    std::size_t position = 0;
    for (const auto& segment : segments) {
        if (segment.empty()) continue;
        if (position > buffer.size()) throw ngStaticError<ArraySliceError>(0, position, buffer.size(),
            RetainedString::fromStaticLiteral("std/path.d"), 1473);
        auto prefix = position;
        bool useSeparator = false;
        const auto text = segment.view();
        if (prefix) {
#ifdef _WIN32
            const bool absolute = build_path_detail::driveRoot(text) || build_path_detail::unc(text);
            const bool rooted = build_path_detail::separator(text[0]) || absolute;
            if (rooted) {
                prefix = absolute ? 0 : build_path_detail::rootLength({buffer.data(), position});
                if (prefix && build_path_detail::separator(buffer.data()[prefix - 1])) --prefix;
            }
#else
            const bool rooted = build_path_detail::separator(text[0]);
            if (rooted) prefix = 0;
#endif
            else if (!build_path_detail::separator(buffer.data()[prefix - 1])) useSeparator = true;
        }
        std::size_t index = 0;
        for (std::size_t i = 0; i < prefix; ++i)
            build_path_detail::put(buffer, index, buffer.data()[i]);
        if (useSeparator) {
#ifdef _WIN32
            build_path_detail::put(buffer, index, '\\');
#else
            build_path_detail::put(buffer, index, '/');
#endif
        }
        // Same-width byUTF!char passes the original code units through unchanged.
        for (const auto value : text) build_path_detail::put(buffer, index, value);
        position = index;
    }
    if (position > buffer.size()) throw ngStaticError<ArraySliceError>(0, position, buffer.size(),
        RetainedString::fromStaticLiteral("std/path.d"), 1483);
    if (buffer.isNull()) return {};
    return RetainedString::retainedView(buffer.retentionOwner(), buffer.data(), position);
}
} // namespace nijigenerate::compat::phobos
