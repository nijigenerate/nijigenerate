#pragma once
#include <nijigenerate/compat/retained_string.hpp>
#include <common/string_key_map.hpp>

namespace nijigenerate::panels::nodes::detail {
inline thread_local bool followSelection = false;
inline thread_local compat::RetainedString suffixName;
inline thread_local nicxlive::core::common::StringKeyMap<compat::RetainedString> actionIconMap = [] {
    nicxlive::core::common::StringKeyMap<compat::RetainedString> result;
    result.setRetained(compat::RetainedString::fromStaticLiteral("Add"),
        compat::RetainedString::fromStaticLiteral("\xee\x85\x85"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Insert"),
        compat::RetainedString::fromStaticLiteral("\xee\x85\x9e"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Edit Mesh"),
        compat::RetainedString::fromStaticLiteral("\xef\x82\x97"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Delete"),
        compat::RetainedString::fromStaticLiteral("\xee\xa4\xae"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Show"),
        compat::RetainedString::fromStaticLiteral("\xee\xa3\xb4"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Hide"),
        compat::RetainedString::fromStaticLiteral("\xee\xa3\xb5"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Copy"),
        compat::RetainedString::fromStaticLiteral("\xee\x85\x8d"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Paste"),
        compat::RetainedString::fromStaticLiteral("\xee\x85\x8f"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Reload"),
        compat::RetainedString::fromStaticLiteral("\xee\x97\x95"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("More Info"),
        compat::RetainedString::fromStaticLiteral("\xee\xa2\x8e"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Parameter Usage"),
        compat::RetainedString::fromStaticLiteral("\xee\x90\xa9"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Recalculate origin"),
        compat::RetainedString::fromStaticLiteral("\xee\x95\xbb"));
    result.setRetained(compat::RetainedString::fromStaticLiteral("Convert To..."),
        compat::RetainedString::fromStaticLiteral("\xee\x81\x83"));
    return result;
}();

template<bool icon> compat::RetainedString nodeActionToIcon(compat::RetainedString name) {
    if constexpr (icon) {
        if (actionIconMap.contains(name.view())) return actionIconMap.get(name.view());
        return compat::RetainedString::fromStaticLiteral("--");
    } else return name;
}

} // namespace nijigenerate::panels::nodes::detail
