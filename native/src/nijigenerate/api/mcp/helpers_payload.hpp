#pragma once
#include "helpers.hpp"
#include <nijigenerate/commands/detail/base_ex_args.hpp>
#include <nijigenerate/windows/command_browser_enum_traits.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/selector/resource.hpp>
#include <array>
#include <bit>
#include <cmath>

namespace nijigenerate::api::mcp::helpers::detail {
using Json = core::settings::SettingsJsonValue;
inline std::int64_t sourceLong(double number) {
    const auto value=std::trunc(number);
    if(!std::isfinite(value) || value < -9223372036854775808.0 || value >= 9223372036854775808.0)
        return (std::numeric_limits<std::int64_t>::min)();
    return static_cast<std::int64_t>(value);
}
inline float arrayFloat(const Json& value) {
    return static_cast<float>(value.is_double()?value.as_double():static_cast<double>(value.as_int64()));
}
inline std::uint32_t arrayUInt(const Json& value) {
    return static_cast<std::uint32_t>(value.is_int64()?value.as_int64():sourceLong(value.as_double()));
}
template<class T> inline constexpr bool stringField = std::same_as<T,compat::RetainedString> ||
    std::same_as<T,compat::RetainedStringSlice> || std::same_as<T,core::settings::SettingsString>;
template<class T,class Destination> void assignPayload(Destination destination, const Json& value) {
    if constexpr(std::same_as<T,bool>) { if(value.is_bool()) destination()=value.as_bool(); }
    else if constexpr(windows::command_browser::detail::isBrowserEnum<T>) {
        using Meta=windows::command_browser::detail::BrowserEnumTraits<T>;
        if(value.is_string()) {
            for(const auto& [name,member]:Meta::members) {
                bool matches=value.as_string().view()==name;
                if constexpr(requires { Meta::descriptionChoices; }) {
                    auto choices=Meta::descriptionChoices;
                    while(!choices.empty()) {
                        const auto end=choices.find('|');const auto item=choices.substr(0,end);const auto equal=item.find('=');
                        if(equal!=std::string_view::npos && item.substr(0,equal)==name &&
                           item.substr(equal+1)==value.as_string().view()) matches=true;
                        if(end==std::string_view::npos) break;choices.remove_prefix(end+1);
                    }
                }
                if(matches) destination()=member;
            }
        } else if(value.is_int64()) {
            // These two independent D enums have string base types, with no integer cast.
            if constexpr(std::is_enum_v<T> && !std::same_as<T,nicxlive::core::nodes::PhysicsModel> &&
                         !std::same_as<T,nicxlive::core::nodes::ParamMapMode>)
                destination()=static_cast<T>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value.as_int64())));
        }
    } else if constexpr (core::translation::HasNumericEnumFormatMetadata<T>) {
        using Traits = core::translation::FormatNumericEnumTraits<T>;
        if (value.is_string()) {
            for (const auto& member:Traits::members)
                if (value.as_string().view()==member.name) destination()=member.value;
        } else if (value.is_int64()) {
            const auto ordinal = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value.as_int64()));
            destination()=Traits::fromUnderlying(static_cast<typename Traits::Underlying>(ordinal));
        }
    } else if constexpr (std::is_enum_v<T> && requires { core::translation::FormatEnumTraits<T>::members; }) {
        if (value.is_string()) {
            for (const auto& member:core::translation::FormatEnumTraits<T>::members)
                if (value.as_string().view()==member.name) destination()=member.value;
        } else if (value.is_int64()) {
            if constexpr (!std::same_as<T,nicxlive::core::nodes::PhysicsModel> &&
                    !std::same_as<T,nicxlive::core::nodes::ParamMapMode>)
                destination()=static_cast<T>(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value.as_int64())));
        }
    } else if constexpr(std::same_as<T,std::int32_t> || std::same_as<T,std::uint32_t> ||
                        std::same_as<T,std::int64_t> || std::same_as<T,std::uint64_t>) {
        if(value.is_int64()) {
            if constexpr(std::is_signed_v<T> && sizeof(T)==4)
                destination()=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value.as_int64()));
            else destination()=static_cast<T>(value.as_int64());
        }
    } else if constexpr(std::same_as<T,float> || std::same_as<T,double>) {
        if(value.is_double()) destination()=static_cast<T>(value.as_double());
        else if(value.is_int64()) destination()=static_cast<T>(static_cast<double>(value.as_int64()));
    } else if constexpr(stringField<T>) {
        if(value.is_string()) {
            if constexpr(std::same_as<T,compat::RetainedString>) destination()=value.as_string().retainedString();
            else destination()=value.as_string();
        }
    } else if constexpr(std::same_as<T,compat::Slice<compat::RetainedString>> ||
                        std::same_as<T,compat::Slice<compat::RetainedStringSlice>>) {
        if(value.is_array()) {
            T output;
            const auto entries=value.as_array();
            for(auto entry:entries) if(entry.is_string()) {
                if constexpr(std::same_as<T,compat::Slice<compat::RetainedString>>)
                    output.append(entry.as_string().retainedString());
                else output.append(entry.as_string());
            }
            destination()=output;
        }
    } else if constexpr(std::same_as<T,Json>) destination()=value;
    else if constexpr(std::same_as<T,compat::Vec2u>) {
        if(value.is_array() && value.as_array().size()>=2)
            destination()={arrayUInt(value.as_array()[0]),arrayUInt(value.as_array()[1])};
    } else if constexpr(std::same_as<T,nicxlive::core::math::Vec3>) {
        if(value.is_array() && value.as_array().size()>=3)
            destination()={arrayFloat(value.as_array()[0]),arrayFloat(value.as_array()[1]),arrayFloat(value.as_array()[2])};
    } else if constexpr(std::same_as<T,compat::Slice<float>> || std::same_as<T,compat::Slice<std::uint16_t>>) {
        if(value.is_array()) {
            T output;const auto entries=value.as_array();
            for(auto entry:entries) {
                if(!entry.is_double() && !entry.is_int64()) continue;
                const double number=entry.is_double()?entry.as_double():static_cast<double>(entry.as_int64());
                if constexpr(std::same_as<T,compat::Slice<float>>) output.append(static_cast<float>(number));
                else output.append(static_cast<std::uint16_t>(compat::ngFloatToInt32(number)));
            }
            destination()=output;
        }
    } else if constexpr(std::same_as<T,std::array<float,2>> || std::same_as<T,std::array<float,3>> ||
                        std::same_as<T,std::array<std::uint32_t,2>>) {
        if(value.is_array() && value.as_array().size()>=std::tuple_size_v<T>) {
            T output;
            for(std::size_t i=0;i<output.size();++i) {
                if constexpr(std::same_as<typename T::value_type,float>) output[i]=arrayFloat(value.as_array()[i]);
                else output[i]=arrayUInt(value.as_array()[i]);
            }
            destination()=output;
        }
    } else if constexpr(requires { typename T::element_type; }) {
        using E=typename T::element_type;
        if(value.is_int64()) if(auto puppet=project::incActivePuppet()) {
            const auto uuid=static_cast<std::uint32_t>(value.as_int64());
            if constexpr(std::derived_from<E,nicxlive::core::nodes::Node>) {
                if(auto node=std::dynamic_pointer_cast<E>(puppet->findNodeById(uuid))) destination()=node;
            } else if constexpr(std::derived_from<E,nicxlive::core::param::Parameter>) {
                if(auto parameter=std::dynamic_pointer_cast<E>(puppet->findParameter(uuid))) destination()=parameter;
            } else if constexpr(std::derived_from<E,core::selector::Resource>) {
                if(auto node=puppet->findNodeById(uuid)) destination()=std::dynamic_pointer_cast<E>(node);
                else if(auto parameter=puppet->findParameter(uuid)) destination()=std::dynamic_pointer_cast<E>(parameter);
            }
        }
    }
}
}
namespace nijigenerate::api::mcp::helpers {
template<class C> void applyPayloadToInstance(std::shared_ptr<C> instance,
        core::settings::SettingsJsonValue payload) {
    using S=typename commands::detail::BaseExArgsOf<C>::Storage;
    std::apply([&](const auto&... fields) {
        ([&] {
            using F=std::remove_cvref_t<decltype(fields)>;
            const auto object = payload.sourceObjectNoRef();
            const auto found = object.if_contains(F::fieldName);
            if(found) {
                const auto value = payload.sourceIndexValue(F::fieldName);
                detail::assignPayload<typename F::Type>([&]() -> decltype(auto) {
                    if(!instance) compat::ngFailNullAccess(true);
                    return F::get(*instance);
                },value);
            } else if constexpr(std::same_as<typename F::Type,core::settings::SettingsJsonValue>) {
                if(!instance) compat::ngFailNullAccess(true);
                F::get(*instance)={};
            }
        }(),...);
    },commands::CommandFields<S>::descriptors);
}
}
