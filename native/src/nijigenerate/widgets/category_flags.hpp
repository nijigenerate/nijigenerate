#pragma once

#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <compat/object_metadata.hpp>

namespace nijigenerate::widgets::category {

enum class IncCategoryFlags : std::int32_t {
    None = 0,
    NoCollapse = 1,
    DefaultClosed = 2,
};

constexpr IncCategoryFlags operator|(IncCategoryFlags left, IncCategoryFlags right) {
    return static_cast<IncCategoryFlags>(static_cast<std::int32_t>(left) | static_cast<std::int32_t>(right));
}
constexpr IncCategoryFlags operator&(IncCategoryFlags left, IncCategoryFlags right) {
    return static_cast<IncCategoryFlags>(static_cast<std::int32_t>(left) & static_cast<std::int32_t>(right));
}

} // namespace nijigenerate::widgets::category

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::widgets::category::IncCategoryFlags> {
    using Enum = ::nijigenerate::widgets::category::IncCategoryFlags;
    inline static constexpr std::string_view name = "IncCategoryFlags";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::None, "None"},
        FormatEnumMember<Enum>{Enum::NoCollapse, "NoCollapse"},
        FormatEnumMember<Enum>{Enum::DefaultClosed, "DefaultClosed"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::category::IncCategoryFlags> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.category.IncCategoryFlags)";
    inline static constexpr char unqualifiedArgument[] = "(IncCategoryFlags)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.category.IncCategoryFlags";
    inline static constexpr char unqualifiedElement[] = "IncCategoryFlags";
};
}
