#pragma once
#include "nijigenerate/widgets/category.hpp"
#include <compat/object_metadata.hpp>
#include <limits>
namespace nijigenerate::widgets::category::detail {
struct CategoryData {
    bool open = false;
    bool badColor = false;
    bool isDark = false;
    ImVec4 contentBounds = ImVec4(std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN());
    IncCategoryFlags flags = IncCategoryFlags::None;
};
bool& readOnlyCategoryContents();
void getCategoryColors(ImVec4 color, ImVec4& hoverColor, ImVec4& activeColor, ImVec4& bgColor,
                       ImVec4& shadowColor, ImVec4& textColor, bool& isDark);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::category::detail::CategoryData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.category.CategoryData)";
    inline static constexpr char unqualifiedArgument[] = "(CategoryData)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.category.CategoryData";
    inline static constexpr char unqualifiedElement[] = "CategoryData";
};
}
