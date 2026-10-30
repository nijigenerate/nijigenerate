#pragma once
#include <nijigenerate/viewport/vertex/automesh/package.hpp>
#include <nijigenerate/viewport/vertex/automesh/meta.hpp>
#include <array>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::commands::automesh::config::detail {
template<class PT,std::size_t Kind> inline constexpr auto configEnumNameBytes=[]() consteval {
    constexpr std::string_view prefix="AutoMeshSet";
    constexpr std::array<std::string_view,3> kinds{"Simple","Advanced","Preset"};
    constexpr auto kind=kinds[Kind];
    constexpr auto identifier=viewport::vertex::automesh::meta::AMSourceAttributes<PT>::identifier;
    std::array<char,prefix.size()+kind.size()+1+identifier.size()+1> result{};
    std::size_t offset=0;
    for (char value:prefix) result[offset++]=value;
    for (char value:kind) result[offset++]=value;
    result[offset++]='_';
    // Source foreach infers char: sanitize the actual identifier byte by byte.
    for (char value:identifier) {
        const bool alphaNumeric=(value>='a' && value<='z') ||
            (value>='A' && value<='Z') || (value>='0' && value<='9');
        result[offset++]=alphaNumeric?value:'_';
    }
    return result;
}();

template<class PT,std::size_t Kind> constexpr std::string_view configEnumMemberName() {
    const auto& bytes=configEnumNameBytes<PT,Kind>;
    return {bytes.data(),bytes.size()-1};
}

template<class PT,std::size_t Kind> inline constexpr auto configClassNameBytes=[]() consteval {
    constexpr auto name=configEnumMemberName<PT,Kind>();
    constexpr std::string_view suffix="Command";
    std::array<char,name.size()+suffix.size()+1> result{};
    std::size_t offset=0;
    for (char value:name) result[offset++]=value;
    for (char value:suffix) result[offset++]=value;
    return result;
}();

template<class PT,std::size_t Kind> constexpr std::string_view configClassLeafName() {
    const auto& bytes=configClassNameBytes<PT,Kind>;
    return {bytes.data(),bytes.size()-1};
}

template<class... PT> constexpr auto configTypedEnumNames(compat::TypeSequence<PT...>) {
    const auto names=std::tuple_cat(std::tuple{configEnumMemberName<PT,0>(),
        configEnumMemberName<PT,1>(),configEnumMemberName<PT,2>()}...);
    return std::apply([](auto... name) { return std::array<std::string_view,sizeof...(name)>{name...}; },names);
}
} // namespace nijigenerate::commands::automesh::config::detail
