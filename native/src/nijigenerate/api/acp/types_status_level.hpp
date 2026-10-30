#pragma once

#include <cstdint>
#include <array>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::api::acp::types {

enum class StatusLevel : std::int32_t {
    info,
    warning,
    error,
    progress
};

} // namespace nijigenerate::api::acp::types

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::api::acp::types::StatusLevel> {
    using Enum=::nijigenerate::api::acp::types::StatusLevel;
    inline static constexpr std::string_view name="StatusLevel";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::info,"info"},
        FormatEnumMember<Enum>{Enum::warning,"warning"},
        FormatEnumMember<Enum>{Enum::error,"error"},
        FormatEnumMember<Enum>{Enum::progress,"progress"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::api::acp::types::StatusLevel> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.api.acp.types.StatusLevel)"),
            RetainedString::fromStaticLiteral("(StatusLevel)"),
            RetainedString::fromStaticLiteral("nijigenerate.api.acp.types.StatusLevel"),
            RetainedString::fromStaticLiteral("StatusLevel")};
    }
};
} // namespace nicxlive::core::d_runtime
