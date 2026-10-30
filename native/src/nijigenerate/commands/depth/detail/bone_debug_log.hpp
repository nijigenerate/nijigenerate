#pragma once

#include "bone_debug_config.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <concepts>
#include <span>
#include <string_view>

namespace nijigenerate::commands::depth::bone::detail {

template<class Format> concept DepthBoneDebugFormat =
    std::same_as<Format, compat::RetainedString> ||
    std::same_as<Format, compat::RetainedStringSlice> ||
    std::same_as<Format, compat::Slice<char>> ||
    std::same_as<Format, std::span<const char>> ||
    std::same_as<Format, std::string_view>;

// The current source flag compiles out the complete body, not the call or its
// argument evaluation. Passing descriptors by value does not copy their bytes.
template<DepthBoneDebugFormat Format, class... Args>
void depthBoneDebugLog([[maybe_unused]] Format fmt, [[maybe_unused]] Args... args) {
    static_assert(!EnableDepthBoneDebugLog,
        "Enabling Depth Bone logging requires its real typed Phobos output provider");
}

// Literal/array transport avoids a C-string length scan or an owning byte copy.
template<std::size_t N, class... Args>
void depthBoneDebugLog([[maybe_unused]] const char (&fmt)[N], [[maybe_unused]] Args... args) {
    static_assert(!EnableDepthBoneDebugLog,
        "Enabling Depth Bone logging requires its real typed Phobos output provider");
}

} // namespace nijigenerate::commands::depth::bone::detail
