#pragma once
#include "helpers.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/core/selector/resource.hpp>
#include <nijigenerate/windows/command_browser_reflection.hpp>
#include <nijigenerate/windows/command_browser_enum_traits.hpp>
#include <bit>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <resource.hpp>
#include "detail/helper_value_string.hpp"

namespace nijigenerate::api::mcp::helpers {
namespace detail {
using Json = core::settings::SettingsJsonValue;
using Object = core::settings::SettingsJsonObject;
using Array = core::settings::SettingsJsonArray;
template<class T> struct SourceType;
#define NG_MCP_SOURCE_TYPE(CPP, DNAME) template<> struct SourceType<CPP> { static constexpr char name[] = DNAME; };
NG_MCP_SOURCE_TYPE(nicxlive::core::nodes::Node, "Node")
NG_MCP_SOURCE_TYPE(nicxlive::core::param::Parameter, "Parameter")
NG_MCP_SOURCE_TYPE(nicxlive::core::param::ParameterBinding, "ParameterBinding")
NG_MCP_SOURCE_TYPE(nicxlive::core::Puppet, "Puppet")
NG_MCP_SOURCE_TYPE(ext::ExParameterGroup, "ExParameterGroup")
NG_MCP_SOURCE_TYPE(core::selector::Resource, "Resource")
NG_MCP_SOURCE_TYPE(bool, "bool")
NG_MCP_SOURCE_TYPE(std::int32_t, "int")
NG_MCP_SOURCE_TYPE(std::uint32_t, "uint")
NG_MCP_SOURCE_TYPE(std::int64_t, "long")
NG_MCP_SOURCE_TYPE(std::uint64_t, "ulong")
NG_MCP_SOURCE_TYPE(std::int16_t, "short")
NG_MCP_SOURCE_TYPE(std::uint16_t, "ushort")
NG_MCP_SOURCE_TYPE(float, "float")
NG_MCP_SOURCE_TYPE(double, "double")
NG_MCP_SOURCE_TYPE(compat::RetainedString, "string")
NG_MCP_SOURCE_TYPE(compat::RetainedStringSlice, "string")
NG_MCP_SOURCE_TYPE(Json, "JSONValue")
NG_MCP_SOURCE_TYPE(boost::json::value, "JSONValue")
#undef NG_MCP_SOURCE_TYPE
template<class T> requires requires { SourceType<T>::name; }
struct SourceType<std::shared_ptr<T>> : SourceType<T> {};
template<class T> requires windows::command_browser::detail::isBrowserEnum<T> struct SourceType<T> {
    static constexpr auto name=windows::command_browser::detail::BrowserEnumTraits<T>::typeName;
};
template<class T> compat::RetainedString sourceTypeName() {
    if constexpr (requires { SourceType<T>::name; }) {
        if constexpr(std::is_array_v<decltype(SourceType<T>::name)>)
            return compat::RetainedString::fromStaticLiteral(SourceType<T>::name);
        else {
            static const compat::RetainedString descriptor=[] {
                const std::string_view name=SourceType<T>::name;
                try { return compat::RetainedString(std::string(name)); }
                catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            return descriptor;
        }
    } else return nicxlive::core::d_runtime::ngSourceTypeSpelling<T>(
        nicxlive::core::d_runtime::SourceTypeSpelling::UnqualifiedElement);
}

template<class T> requires requires { SourceType<T>::name; }
struct SourceType<compat::Slice<T>> {
    inline static const std::string name = std::string(SourceType<T>::name) + "[]";
};
template<class T> Json encodeResource(const std::shared_ptr<T>& value) {
    Object object; object[compat::RetainedString::fromStaticLiteral("type")] = core::settings::SettingsString(sourceTypeName<T>());
    if constexpr(std::derived_from<T,nicxlive::core::Resource>) {
        object[compat::RetainedString::fromStaticLiteral("uuid")]=static_cast<std::int64_t>(compat::ngRequireObject(value)->getUUID());
        object[compat::RetainedString::fromStaticLiteral("name")]=core::settings::SettingsString(compat::ngRequireObject(value)->getName());
    } else {
        if constexpr (requires { value->uuid; }) object[compat::RetainedString::fromStaticLiteral("uuid")] = static_cast<std::int64_t>(compat::ngRequireObject(value)->uuid);
        else if constexpr (requires { value->uuid(); }) object[compat::RetainedString::fromStaticLiteral("uuid")] = static_cast<std::int64_t>(compat::ngRequireObject(value)->uuid());
        if constexpr (requires { value->name; }) object[compat::RetainedString::fromStaticLiteral("name")] = compat::ngRequireObject(value)->name;
        else if constexpr (requires { value->name(); }) object[compat::RetainedString::fromStaticLiteral("name")] = core::settings::SettingsString(compat::ngRequireObject(value)->name());
    }
    return object;
}
template<class T> Json encodeValue(const T& value) {
    if constexpr (std::same_as<T,bool>) return value;
    else if constexpr (core::translation::HasNumericEnumFormatMetadata<T>) {
        using Underlying = typename core::translation::FormatNumericEnumTraits<T>::Underlying;
        const auto raw = core::translation::FormatNumericEnumTraits<T>::underlying(value);
        if constexpr (std::is_unsigned_v<Underlying> && sizeof(Underlying)==8)
            return std::bit_cast<std::int64_t>(raw);
        else return static_cast<std::int64_t>(raw);
    }
    else if constexpr(std::is_enum_v<T> &&
            !std::same_as<T,nicxlive::core::nodes::PhysicsModel> &&
            !std::same_as<T,nicxlive::core::nodes::ParamMapMode>) {
        using U=std::underlying_type_t<T>;
        return encodeValue(static_cast<U>(value));
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_unsigned_v<T> && sizeof(T)==8) return std::bit_cast<std::int64_t>(value);
        else return static_cast<std::int64_t>(value);
    } else if constexpr (std::is_floating_point_v<T>) return static_cast<double>(value);
    else if constexpr (std::same_as<T,compat::RetainedString> || std::same_as<T,compat::RetainedStringSlice>)
        return core::settings::SettingsString(value);
    else if constexpr (std::same_as<T,Json> || std::same_as<T,boost::json::value>) return Json(value);
    else if constexpr (std::same_as<T,compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>> ||
                       std::same_as<T,compat::Slice<std::shared_ptr<nicxlive::core::param::Parameter>>>) {
        Array result;
        for (auto item : value) result.append(encodeResource(item));
        return result;
    } else if constexpr (requires { typename T::element_type; }) {
        if constexpr (std::derived_from<typename T::element_type,core::selector::Resource>) return encodeResource(value);
        else return core::settings::SettingsString(sourceValueString(value));
    } else return core::settings::SettingsString(sourceValueString(value));
}
template<class T> std::type_identity<commands::CreateResult<T>> createBase(const commands::CreateResult<T>*);
std::type_identity<void> createBase(...);
template<class T> std::type_identity<commands::DeleteResult<T>> deleteBase(const commands::DeleteResult<T>*);
std::type_identity<void> deleteBase(...);
template<class T> std::type_identity<commands::LoadResult<T>> loadBase(const commands::LoadResult<T>*);
std::type_identity<void> loadBase(...);
template<class T> struct ResultInfo { static constexpr int kind=0; using Value=void; using Base=T; };
template<class T> struct ResultInfo<commands::CreateResult<T>> {
    static constexpr int kind=1; using Value=T; using Base=commands::CreateResult<T>;
};
template<class T> struct ResultInfo<commands::DeleteResult<T>> {
    static constexpr int kind=2; using Value=T; using Base=commands::DeleteResult<T>;
};
template<class T> struct ResultInfo<commands::LoadResult<T>> {
    static constexpr int kind=3; using Value=T; using Base=commands::LoadResult<T>;
};
template<class T> struct ResultKind : ResultInfo<std::conditional_t<
    !std::is_void_v<typename decltype(createBase(static_cast<T*>(nullptr)))::type>,
    typename decltype(createBase(static_cast<T*>(nullptr)))::type,std::conditional_t<
    !std::is_void_v<typename decltype(deleteBase(static_cast<T*>(nullptr)))::type>,
    typename decltype(deleteBase(static_cast<T*>(nullptr)))::type,
    typename decltype(loadBase(static_cast<T*>(nullptr)))::type>>> {};
template<class T> struct ResultKind<commands::ExCommandResultImpl<T>> {
    static constexpr int kind=4; using Value=T; using Base=commands::ExCommandResultImpl<T>;
};

template<class T> Json encodeResourceResult(const std::shared_ptr<T>& result) {
    Object object; object[compat::RetainedString::fromStaticLiteral("succeeded")] = compat::ngRequireObject(result)->succeeded;
    if (!compat::ngRequireObject(result)->message.empty()) object[compat::RetainedString::fromStaticLiteral("message")] = core::settings::SettingsString(compat::ngRequireObject(result)->message);
    Array values;
    if constexpr (ResultKind<T>::kind==1) {
        const auto entries=compat::ngRequireObject(result)->created; for(auto entry:entries) values.append(encodeResource(entry));
        object[compat::RetainedString::fromStaticLiteral("created")]=values;
    } else if constexpr (ResultKind<T>::kind==2) {
        const auto entries=compat::ngRequireObject(result)->deleted; for(auto entry:entries) values.append(encodeResource(entry));
        object[compat::RetainedString::fromStaticLiteral("deleted")]=values;
    } else {
        const auto entries=compat::ngRequireObject(result)->loaded; for(auto entry:entries) values.append(encodeResource(entry));
        object[compat::RetainedString::fromStaticLiteral("loaded")]=values;
    }
    return object;
}
}
template<std::derived_from<commands::CommandResult> R>
core::settings::SettingsJsonValue commandResultToJson(std::shared_ptr<R> result) {
    using namespace detail;
    Object object; object[compat::RetainedString::fromStaticLiteral("status")]=compat::ngRequireObject(result)->succeeded?core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("ok")):core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("error"));object[compat::RetainedString::fromStaticLiteral("succeeded")]=compat::ngRequireObject(result)->succeeded;
    if(!compat::ngRequireObject(result)->message.empty()) object[compat::RetainedString::fromStaticLiteral("message")]=core::settings::SettingsString(compat::ngRequireObject(result)->message);
    constexpr int kind=ResultKind<R>::kind;
    if constexpr(kind>=1 && kind<=3) {
        object[compat::RetainedString::fromStaticLiteral("result")]=encodeResourceResult(std::static_pointer_cast<typename ResultKind<R>::Base>(result));
        static const compat::RetainedString typeName(std::string(kind==1?"CreateResult!(":kind==2?"DeleteResult!(":"LoadResult!(")+
            std::string(sourceTypeName<typename ResultKind<R>::Value>().view())+")");
        object[compat::RetainedString::fromStaticLiteral("resultType")]=core::settings::SettingsString(typeName);
    } else if constexpr(kind==4) {
        object[compat::RetainedString::fromStaticLiteral("resultType")]=core::settings::SettingsString(sourceTypeName<typename ResultKind<R>::Value>());
        object[compat::RetainedString::fromStaticLiteral("result")]=encodeValue(compat::ngRequireObject(result)->result);
    } else object[compat::RetainedString::fromStaticLiteral("resultType")]=core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("CommandResult"));
    return object;
}
core::settings::SettingsJsonValue commandResultToJsonRuntime(std::shared_ptr<commands::CommandResult> result);
}
