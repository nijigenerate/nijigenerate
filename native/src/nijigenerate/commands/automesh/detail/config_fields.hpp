#pragma once
#include "config_description.hpp"
#include <nijigenerate/commands/fields.hpp>

namespace nijigenerate::commands::automesh::config::detail {
template<class PT,viewport::vertex::automesh::meta::AutoMeshLevel Level,std::size_t Index>
constexpr auto configLevelMember() {
    constexpr auto declaration=std::get<Index>(
        viewport::vertex::automesh::meta::AMReflectionSource<PT>::declarations());
    using Type=std::remove_reference_t<decltype(declaration.get(std::declval<PT&>()))>;
    if constexpr (std::is_same_v<Type,float> || std::is_same_v<Type,compat::Slice<float>> ||
            std::is_same_v<Type,compat::RetainedString>) {
        constexpr auto selected=[&]() constexpr {
            description_detail::meta::source_binding::AMParamDeclaration parameter{};
            bool hasParameter=false;
            bool hasEnumeration=false;
            std::apply([&](const auto&... attribute) {
                ([&] {
                    using Attribute=std::remove_cvref_t<decltype(attribute)>;
                    if constexpr (std::is_same_v<Attribute,
                            description_detail::meta::source_binding::AMParamDeclaration>) {
                        parameter=attribute;
                        hasParameter=true;
                    } else if constexpr (description_detail::isEnumAttribute<Attribute>) {
                        hasEnumeration=true;
                    }
                }(),...);
            },declaration.attributes);
            return hasParameter && parameter.level==Level &&
                (!std::is_same_v<Type,compat::RetainedString> || hasEnumeration);
        }();
        if constexpr (selected) return std::tuple{std::integral_constant<std::size_t,Index>{}};
        else return std::tuple{};
    } else return std::tuple{};
}

template<class PT,viewport::vertex::automesh::meta::AutoMeshLevel Level>
constexpr auto configLevelMembers() {
    using Declarations=decltype(viewport::vertex::automesh::meta::AMReflectionSource<PT>::declarations());
    return []<std::size_t... Index>(std::index_sequence<Index...>) {
        return std::tuple_cat(configLevelMember<PT,Level,Index>()...);
    }(std::make_index_sequence<std::tuple_size_v<Declarations>>{});
}

// The accessor remains the real named ExCommand argument field. Its metadata
// comes from the same defining declaration used by runtime reflection.
template<class PT,viewport::vertex::automesh::meta::AutoMeshLevel Level,
    std::size_t Index,auto Member> struct ConfigSourceFieldText {
    using Type=typename CommandMember<decltype(Member)>::Type;
    inline static constexpr auto declaration=std::get<Index>(
        viewport::vertex::automesh::meta::AMReflectionSource<PT>::declarations());
    using SourceType=std::remove_reference_t<decltype(declaration.get(std::declval<PT&>()))>;
    static_assert(std::is_same_v<Type,SourceType>);
    static_assert(std::is_same_v<Type,float> || std::is_same_v<Type,compat::Slice<float>> ||
        std::is_same_v<Type,compat::RetainedString>);
    inline static constexpr bool hasEnumeration=description_detail::hasEnumeration(declaration.attributes);
    static_assert(!std::is_same_v<Type,compat::RetainedString> || hasEnumeration);
    inline static constexpr auto parameter=[]() consteval {
        description_detail::meta::source_binding::AMParamDeclaration result{};
        std::apply([&](const auto&... attribute) {
            ([&] {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(attribute)>,
                        description_detail::meta::source_binding::AMParamDeclaration>) result=attribute;
            }(),...);
        },declaration.attributes);
        return result;
    }();
    static_assert(parameter.level==Level && !parameter.id.empty());
    static constexpr std::string label() {
        std::string text(parameter.label);
        if constexpr (std::is_same_v<Type,compat::Slice<float>>) text.append(" (array)");
        else if constexpr (std::is_same_v<Type,compat::RetainedString>) {
            // Original string arguments are emitted only for an AMEnum.
            if (hasEnumeration) {
                text.append(" (");
                text.append(description_detail::enumChoices(declaration.attributes));
                text+=')';
            }
        }
        return text;
    }
    inline static constexpr auto labelBytes=[]() consteval {
        constexpr auto length=[]() consteval { return label().size(); }();
        std::array<char,length+1> result{};
        const auto text=label();
        for (std::size_t index=0;index<length;++index) result[index]=text[index];
        return result;
    }();
    inline static constexpr auto nameLiteral=[]() consteval {
        char result[parameter.id.size()+1]{};
        for (std::size_t index=0;index<parameter.id.size();++index) result[index]=parameter.id[index];
        return CommandLiteral(result);
    }();
    inline static constexpr auto descriptionLiteral=[]() consteval {
        char result[labelBytes.size()]{};
        for (std::size_t index=0;index<labelBytes.size();++index) result[index]=labelBytes[index];
        return CommandLiteral(result);
    }();
    inline static constexpr auto typeLiteral=[]() consteval {
        if constexpr (std::is_same_v<Type,float>) return CommandLiteral("float");
        else if constexpr (std::is_same_v<Type,compat::Slice<float>>) return CommandLiteral("float[]");
        else return CommandLiteral("string");
    }();
};

template<class PT,viewport::vertex::automesh::meta::AutoMeshLevel Level,std::size_t Index,auto Member>
using ConfigSourceField=FieldDescriptor<Member,
    ConfigSourceFieldText<PT,Level,Index,Member>::nameLiteral,
    ConfigSourceFieldText<PT,Level,Index,Member>::descriptionLiteral,
    ConfigSourceFieldText<PT,Level,Index,Member>::typeLiteral>;

template<class PT,viewport::vertex::automesh::meta::AutoMeshLevel Level,auto... Member>
constexpr auto configSourceFields() {
    using Indices=decltype(configLevelMembers<PT,Level>());
    static_assert(std::tuple_size_v<Indices> == sizeof...(Member),
        "The actual argument members must match the generated TW list");
    return []<std::size_t... Index>(std::index_sequence<Index...>) {
        return std::tuple{ConfigSourceField<PT,Level,std::tuple_element_t<Index,Indices>::value,Member>{}...};
    }(std::index_sequence_for<decltype(Member)...>{});
}

template<class PT,std::size_t Index,class Minimum,class Maximum>
void writeConfigFloat(PT& processor,const float& input,Minimum minimum,Maximum maximum) {
    using Source=viewport::vertex::automesh::meta::AMReflectionSource<PT>;
    static constexpr auto declaration=std::get<Index>(Source::declarations());
    static_assert(std::is_same_v<std::remove_reference_t<decltype(declaration.get(processor))>,float>);
    float value=input;
    if (value==value) {
        // These are the actual generated source literals, not AMParam float
        // values standing in for the original D to!string output.
        if (value<minimum) value=static_cast<float>(minimum);
        if (value>maximum) value=static_cast<float>(maximum);
        declaration.get(processor)=value;
        if constexpr (Source::hasPostParamWrite) {
            static constexpr auto id=[]() consteval {
                std::string_view result;
                std::apply([&](const auto&... attribute) {
                    ([&] {
                        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(attribute)>,
                                description_detail::meta::source_binding::AMParamDeclaration>) result=attribute.id;
                    }(),...);
                },declaration.attributes);
                return result;
            }();
            Source::postParamWrite(processor,compat::RetainedString::fromStaticView(id));
        }
    }
}

template<class PT,std::size_t Index>
void writeConfigString(PT& processor,const compat::RetainedString& input) {
    using Source=viewport::vertex::automesh::meta::AMReflectionSource<PT>;
    static constexpr auto declaration=std::get<Index>(Source::declarations());
    static_assert(std::is_same_v<std::remove_reference_t<decltype(declaration.get(processor))>,
        compat::RetainedString>);
    static_assert(description_detail::hasEnumeration(declaration.attributes));
    static constexpr auto enumIndex=[]() consteval {
        using Attributes=std::remove_cvref_t<decltype(declaration.attributes)>;
        return []<std::size_t... I>(std::index_sequence<I...>) {
            std::size_t result=0;
            ((description_detail::isEnumAttribute<std::tuple_element_t<I,Attributes>>
                ? result=I : result),...);
            return result;
        }(std::make_index_sequence<std::tuple_size_v<Attributes>>{});
    }();
    const auto choice=input;
    compat::RetainedString selected;
    for (const auto& value:std::get<enumIndex>(declaration.attributes).values) {
        const auto view=[&]() {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>,std::string_view>)
                return value;
            else return value.view();
        }();
        if (choice.view()==view) {
            selected=compat::RetainedString::fromStaticView(view);
            break;
        }
    }
    if (!selected.empty()) declaration.get(processor)=selected;
    // The authored hook is outside the successful-choice assignment condition.
    if constexpr (Source::hasPostParamWrite) {
        static constexpr auto id=[]() consteval {
            std::string_view result;
            std::apply([&](const auto&... attribute) {
                ([&] {
                    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(attribute)>,
                            description_detail::meta::source_binding::AMParamDeclaration>) result=attribute.id;
                }(),...);
            },declaration.attributes);
            return result;
        }();
        Source::postParamWrite(processor,compat::RetainedString::fromStaticView(id));
    }
}

template<class PT,std::size_t Index>
void writeConfigArray(PT& processor,const compat::Slice<float>& values) {
    using Source=viewport::vertex::automesh::meta::AMReflectionSource<PT>;
    static constexpr auto declaration=std::get<Index>(Source::declarations());
    static_assert(std::is_same_v<std::remove_reference_t<decltype(declaration.get(processor))>,
        compat::Slice<float>>);
    if (!values.isNull()) {
        declaration.get(processor)=values;
        if constexpr (Source::hasPostParamWrite) {
            static constexpr auto id=[]() consteval {
                std::string_view result;
                std::apply([&](const auto&... attribute) {
                    ([&] {
                        if constexpr (std::is_same_v<std::remove_cvref_t<decltype(attribute)>,
                                description_detail::meta::source_binding::AMParamDeclaration>) result=attribute.id;
                    }(),...);
                },declaration.attributes);
                return result;
            }();
            Source::postParamWrite(processor,compat::RetainedString::fromStaticView(id));
        }
    }
}
} // namespace nijigenerate::commands::automesh::config::detail
