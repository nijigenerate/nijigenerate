#pragma once
// Phobos std.path relativePath/asRelativePath used char-string bodies, BSL-1.0.
// Copyright the D Language Foundation and Phobos contributors.
// See port/licenses/phobos-crash-path.txt and installed std/path.d.
#include "phobos_build_path.hpp"
#include "phobos_unicode_cursor.hpp"
#include "phobos_file_directory.hpp"
#include "throwable.hpp"
#include "out_of_memory_error.hpp"
#include <common/detail/phobos_lowercase_data.hpp>
#include <functional>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace nijigenerate::compat::phobos {
namespace relative_path_detail {
inline bool absolute(std::string_view path) noexcept {
#ifdef _WIN32
    return build_path_detail::driveRoot(path) || build_path_detail::unc(path);
#else
    return !path.empty() && build_path_detail::separator(path.front());
#endif
}

// Forward-only used pathSplitter closure: roots are one element; separators
// are trimmed between elements, while literal '.' and '..' remain elements.
class Splitter {
    std::string_view path_;
    std::size_t first_ = 0, last_ = 0, next_ = 0;
    bool empty_ = true;
    void trim() {
        while (next_ < path_.size() && build_path_detail::separator(path_[next_])) ++next_;
    }
public:
    explicit Splitter(std::string_view path) : path_(path), empty_(path.empty()) {
        if (empty_) return;
#ifdef _WIN32
        last_ = build_path_detail::rootLength(path);
#else
        last_ = build_path_detail::separator(path[0]) ? 1 : 0;
#endif
        if (!last_) while (last_ < path.size() && !build_path_detail::separator(path[last_])) ++last_;
        next_ = last_;
        trim();
    }
    bool empty() const noexcept { return empty_; }
    std::string_view front() const { return path_.substr(first_, last_ - first_); }
    void popFront() {
        if (next_ == path_.size()) { empty_ = true; return; }
        first_ = next_;
        last_ = first_;
        while (last_ < path_.size() && !build_path_detail::separator(path_[last_])) ++last_;
        next_ = last_;
        trim();
    }
};

#if defined(_WIN32) || defined(__APPLE__)
inline std::uint32_t lower(std::uint32_t scalar) {
    if (scalar < 0xaa) return scalar >= 'A' && scalar <= 'Z' ? scalar + 32 : scalar;
    // The canonical Phobos scalar lowercase trie, not full-string case folding.
    namespace data = ::nicxlive::core::common::unicode::detail::phobos_lowercase_data;
    const auto packed = [](std::uint64_t offset, std::size_t index, unsigned bits) {
        const auto word = data::trieWords[offset + index / (64u / bits)];
        return static_cast<std::uint32_t>((word >> (bits * (index % (64u / bits)))) & ((1ULL << bits) - 1));
    };
    auto index = packed(data::trieOffsets[0], scalar >> 13, 8);
    index = packed(data::trieOffsets[1], (index << 7) + ((scalar >> 6) & 0x7f), 16);
    index = packed(data::trieOffsets[2], (index << 6) + (scalar & 0x3f), 16);
    return index == 0xffff ? scalar : data::lowerValues[index] & 0xffffff;
}
#endif
inline bool equal(std::string_view left, std::string_view right) {
#if defined(_WIN32) || defined(__APPLE__)
    detail::UnicodeCursor<std::string_view, char> a(left), b(right);
    while (!a.empty() && !b.empty()) {
        auto ac = a.front(), bc = b.front();
        const bool separators = ac < 128 && bc < 128 &&
            build_path_detail::separator(static_cast<char>(ac)) &&
            build_path_detail::separator(static_cast<char>(bc));
        if (!separators && lower(ac) != lower(bc)) return false;
        a.popFront(); b.popFront();
    }
    return a.empty() && b.empty();
#else
    return left == right;
#endif
}
template<class Operation> decltype(auto) storage(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
}
inline RetainedString absoluteRelative(RetainedString path, RetainedString base) {
    Splitter bases(base.view()), paths(path.view());
    const bool choosePath = !equal(bases.front(), paths.front());
    bases.popFront(); paths.popFront();
    // mismatch is evaluated even when differing roots ultimately choose path.
    while (!bases.empty() && !paths.empty() && equal(bases.front(), paths.front())) {
        bases.popFront(); paths.popFront();
    }
    if (choosePath) return storage([&] { return RetainedString(path.view()); });
    std::string output;
#ifdef _WIN32
    constexpr char separator = '\\';
#else
    constexpr char separator = '/';
#endif
    const bool dot = bases.empty() && paths.empty();
    while (!bases.empty()) {
        storage([&] { if (!output.empty()) output.push_back(separator); output.append(".."); });
        bases.popFront();
    }
    if (dot) storage([&] { output.push_back('.'); });
    while (!paths.empty()) {
        const auto part = paths.front();
        // joiner's narrow-string elements decode before byChar re-encodes.
        // Keep decoder exceptions outside the primitive allocation boundary.
        detail::UnicodeCursor<std::string_view, char> characters(part);
        if (!output.empty()) storage([&] { output.push_back(separator); });
        std::size_t offset = 0;
        while (!characters.empty()) {
            const auto scalar = characters.front();
            const std::size_t width = scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
            storage([&] { output.append(part.substr(offset, width)); });
            offset += width;
            characters.popFront();
        }
        paths.popFront();
    }
    return storage([&] { return RetainedString(std::move(output)); });
}
}

// The default CaseSensitive.osDefault specialization on native char strings.
// The producer is invoked exactly once, only for an absolute input descriptor.
template<class BaseProducer>
    requires std::is_invocable_r_v<RetainedString, BaseProducer&>
RetainedString ngRelativePath(RetainedString path, BaseProducer&& base) {
    if (!relative_path_detail::absolute(path.view())) return path;
    RetainedString actualBase = std::invoke(base);
    if (!relative_path_detail::absolute(actualBase.view()))
        throw ThrowableException(RetainedString::fromStaticLiteral("Base directory must be absolute"),
            RetainedString::fromStaticLiteral("std/path.d"), 2916);
    return relative_path_detail::absoluteRelative(std::move(path), std::move(actualBase));
}
inline RetainedString ngRelativePath(RetainedString path, RetainedString base) {
    return ngRelativePath(std::move(path), [&] { return base; });
}
inline RetainedString ngRelativePath(RetainedString path) {
    return ngRelativePath(std::move(path), [] {
        auto directory = ngGetCurrentDirectory();
        return relative_path_detail::storage([&] { return RetainedString(std::move(directory)); });
    });
}
}
