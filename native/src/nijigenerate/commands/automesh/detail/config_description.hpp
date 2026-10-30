#pragma once

#include <compat/switch_error.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta_reflection.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta_source_grid.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta_source_contours.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta_source_optimum.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta_source_skeletonize.hpp>
#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::commands::automesh::config::detail {
namespace description_detail {
namespace meta=viewport::vertex::automesh::meta;

template<class T> inline constexpr bool isEnumAttribute=false;
template<> inline constexpr bool isEnumAttribute<meta::AMEnum> = true;
template<std::size_t N>
inline constexpr bool isEnumAttribute<meta::source_binding::AMEnumDeclaration<N>> = true;

template<class Attributes> constexpr bool hasEnumeration(const Attributes& attributes) {
    return std::apply([](const auto&... attribute) {
        return (false || ... || isEnumAttribute<std::remove_cvref_t<decltype(attribute)>>);
    },attributes);
}

template<class Attributes> constexpr std::string enumChoices(const Attributes& attributes) {
    std::string choices;
    std::apply([&](const auto&... attribute) {
        ([&] {
            if constexpr (isEnumAttribute<std::remove_cvref_t<decltype(attribute)>>) {
                choices.clear();
                bool first=true;
                for (const auto& value:attribute.values) {
                    if (!first) choices+='|';
                    first=false;
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>,std::string_view>)
                        choices.append(value);
                    else choices.append(value.view());
                }
            }
        }(),...);
    },attributes);
    return choices;
}

constexpr std::string_view levelName(meta::AutoMeshLevel level) {
    switch (level) {
    case meta::AutoMeshLevel::Preset: return "Preset";
    case meta::AutoMeshLevel::Simple: return "Simple";
    case meta::AutoMeshLevel::Advanced: return "Advanced";
    }
    throw nicxlive::core::d_runtime::SwitchError(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/automesh/config.d"),423);
}

template<class PT,meta::AutoMeshLevel Level,class Declaration>
constexpr void appendSummary(std::string& summary,const Declaration& declaration) {
    using Value=std::remove_reference_t<decltype(declaration.get(std::declval<PT&>()))>;
    if constexpr (std::is_same_v<Value,float> || std::is_same_v<Value,compat::Slice<float>> ||
            std::is_same_v<Value,compat::RetainedString>) {
        meta::source_binding::AMParamDeclaration parameter{};
        bool hasParameter=false;
        std::apply([&](const auto&... attribute) {
            ([&] {
                using Attribute=std::remove_cvref_t<decltype(attribute)>;
                if constexpr (std::is_same_v<Attribute,meta::source_binding::AMParamDeclaration>) {
                    parameter=attribute;
                    hasParameter=true;
                }
            }(),...);
        },declaration.attributes);
        if (hasParameter && parameter.level==Level) {
            if (!summary.empty()) summary.append("; ");
            summary.append(parameter.id);
            summary+='=';
            summary.append(parameter.label);
            if constexpr (std::is_same_v<Value,compat::Slice<float>>) summary.append(" array");
            else if constexpr (std::is_same_v<Value,compat::RetainedString>) {
                if (hasEnumeration(declaration.attributes)) {
                    const auto choices=enumChoices(declaration.attributes);
                    if (!choices.empty()) {
                        summary.append(" (");
                        summary.append(choices);
                        summary+=')';
                    }
                }
            }
            if (!parameter.desc.empty()) {
                summary.append(" - ");
                summary.append(parameter.desc);
            }
        }
    }
}

template<class PT,meta::AutoMeshLevel Level> constexpr std::string parameterSummary() {
    std::string summary;
    const auto declarations=meta::AMReflectionSource<PT>::declarations();
    std::apply([&](const auto&... declaration) {
        (appendSummary<PT,Level>(summary,declaration),...);
    },declarations);
    return summary.empty()?std::string("No configurable parameters for this level."):summary;
}

template<class PT,meta::AutoMeshLevel Level> constexpr std::string descriptionText() {
    std::string text="Set ";
    text.append(levelName(Level));
    text.append(" AutoMesh configuration for processor '");
    text.append(meta::AMProcInfo<PT>::name);
    text.append("' (id: ");
    text.append(meta::AMProcInfo<PT>::id);
    text.append("). Parameters: ");
    text.append(parameterSummary<PT,Level>());
    return text;
}

template<class PT,meta::AutoMeshLevel Level> inline constexpr auto descriptionBytes=[]() consteval {
    constexpr auto length=[]() consteval { return descriptionText<PT,Level>().size(); }();
    std::array<char,length+1> result{};
    const auto text=descriptionText<PT,Level>();
    for (std::size_t index=0;index<length;++index) result[index]=text[index];
    return result;
}();
} // namespace description_detail

template<class PT,viewport::vertex::automesh::meta::AutoMeshLevel Level>
constexpr std::string_view configSetDescription() {
    const auto& bytes=description_detail::descriptionBytes<PT,Level>;
    return {bytes.data(),bytes.size()-1};
}
} // namespace nijigenerate::commands::automesh::config::detail
