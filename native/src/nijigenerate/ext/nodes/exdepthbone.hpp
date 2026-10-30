#pragma once

#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::ext::nodes {

enum class ExDepthTargetKind : std::int32_t {
    Grid = 0,
    Path = 1,
};

inline std::string_view ngDepthTargetKindName(ExDepthTargetKind kind) {
    switch (kind) {
    case ExDepthTargetKind::Grid: return "grid";
    case ExDepthTargetKind::Path: return "path";
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 24);
}

inline ExDepthTargetKind ngParseDepthTargetKind(std::string_view value) {
    if (value == "grid") return ExDepthTargetKind::Grid;
    if (value == "path") return ExDepthTargetKind::Path;
    constexpr std::string_view prefix = "unknown depth rig target kind: ";
    const auto message = [&] {
        try {
            std::string bytes;
            bytes.reserve(prefix.size() + value.size());
            bytes.append(prefix);
            if (!value.empty()) bytes.append(value);
            return compat::RetainedString(std::move(bytes));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 34);
}

inline float ngNormalizeDepthBoneSourceRotation(float rotation) {
    if (!std::isfinite(rotation)) return 0.0f;
    constexpr float pi = 0x1.921fb6p+1f;
    constexpr float twoPi = 0x1.921fb6p+2f;
    // Current LDC Windows widens the enum constant to this double in fmod(real,real).
    // Its surrounding addition/correction still materialize float32 values.
    constexpr double fmodTwoPi = 0x1.921fb54442d18p+2;
    float normalized = static_cast<float>(std::fmod(static_cast<double>(rotation + pi), fmodTwoPi));
    if (normalized < 0.0f) normalized += twoPi;
    return normalized - pi;
}

namespace detail {
inline compat::RetainedString copyDepthRigEnforceMessage(compat::RetainedString message) {
    try {
        return compat::RetainedString(std::string(message.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

struct DepthRigUuidHash {
    std::size_t operator()(std::uint64_t value) const noexcept {
        // core.internal.hash: an integral wider than size_t folds its high bits.
        if constexpr (sizeof(std::size_t) == 4)
            return static_cast<std::size_t>(value ^ (value >> 32));
        return static_cast<std::size_t>(value);
    }
};
using DepthRigUuidMap = nicxlive::core::common::ResourceKeyMap<std::uint64_t, std::uint64_t,
    DepthRigUuidHash, std::equal_to<std::uint64_t>>;

inline std::uint64_t remapDepthRigUuid(std::uint64_t uuid,
    DepthRigUuidMap uuidMap) {
    const auto remapped = uuidMap.find(uuid);
    return remapped == uuidMap.end() ? uuid : remapped->second;
}
} // namespace detail

} // namespace nijigenerate::ext::nodes

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::ext::nodes::ExDepthTargetKind> {
    using Enum = ::nijigenerate::ext::nodes::ExDepthTargetKind;
    inline static constexpr std::string_view name = "ExDepthTargetKind";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Grid, "Grid"},
        FormatEnumMember<Enum>{Enum::Path, "Path"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::ext::nodes::ExDepthTargetKind> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.ext.nodes.exdepthbone.ExDepthTargetKind)";
    inline static constexpr char unqualifiedArgument[] = "(ExDepthTargetKind)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.ext.nodes.exdepthbone.ExDepthTargetKind";
    inline static constexpr char unqualifiedElement[] = "ExDepthTargetKind";
};
}
