#pragma once
#include "../physics.hpp"
#include "../physics_detail.hpp"
#include "../physics_math_detail.hpp"
#include "../linear.hpp"
#include "../evidence.hpp"
#include "../templates.hpp"
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/phobos_string_hash.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <algorithm>
#include <new>
#include <regex>
#include <source_location>
#include <stdexcept>
#include <type_traits>
namespace nijigenerate::autorig::deterministic::physics::detail {
using namespace contracts;
using Array = core::settings::SettingsJsonArray;
using Object = core::settings::SettingsJsonObject;
template<class T> using Slice = compat::Slice<T>;
struct PhysicsStringHash {
    std::size_t operator()(const core::settings::SettingsString& value) const noexcept {
        return compat::ngPhobosStringHash(value.view());
    }
};
template<class Value> using PhysicsStringMap=nicxlive::core::common::ResourceKeyMap<
    core::settings::SettingsString,Value,PhysicsStringHash,std::equal_to<core::settings::SettingsString>>;

template<class Key, class Value, class Hash, class Equal> Value physicsMapValue(
        const nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal>& map,
        const std::type_identity_t<Key>& key, std::size_t sourceLine) {
    const auto found = map.find(key);
    if (found == map.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/physics.d"),
        sourceLine);
    return found->second;
}
template<class Key,class Value,class Hash,class Equal> decltype(auto) physicsMapSlot(
        nicxlive::core::common::ResourceKeyMap<Key,Value,Hash,Equal>& map,
        const std::type_identity_t<Key>& key,
        std::source_location source=std::source_location::current()) {
    try {
        return (map[key]);
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
inline Json at(const Json& v,std::string_view k) { return v.sourceIndexValue(k); }
template<std::size_t N> core::settings::SettingsString jsonLiteral(const char (&value)[N]) {
    // Callers supply the authored process-static string literals.
    return compat::RetainedString::fromStaticLiteral(value);
}
inline void set(Json& v,std::string_view k,Json x) {
    v.sourceIndexAssignSlot(core::settings::SettingsString(k))=std::move(x);
}
template<std::size_t N> void set(Json& v,const char (&k)[N],Json x) {
    v.sourceIndexAssignSlot(jsonLiteral(k))=std::move(x);
}
template<std::size_t N,std::size_t M> void set(Json& v,const char (&k)[N],const char (&x)[M]) {
    set(v,k,Json(jsonLiteral(x)));
}
inline std::string str(Json v) { return std::string(v.as_string().view()); }
inline compat::RetainedString text(std::string_view v) { return compat::RetainedString(v); }
inline Json get(Json v,std::string_view k,Json fallback={}) { return ngRigGet(v,text(k),fallback); }
template<std::size_t N> Json get(Json v,const char (&k)[N],Json fallback={}) {
    return ngRigGet(v,jsonLiteral(k),fallback);
}
inline core::settings::SettingsString stringGet(Json v,std::string_view k,std::string_view fallback="") {
    return ngRigString(v,text(k),text(fallback));
}
template<std::size_t N> core::settings::SettingsString stringGet(Json v,const char (&k)[N]) {
    return ngRigString(v,jsonLiteral(k),compat::RetainedString::emptyLiteral());
}
template<std::size_t N,std::size_t M> core::settings::SettingsString stringGet(
        Json v,const char (&k)[N],const char (&fallback)[M]) {
    return ngRigString(v,jsonLiteral(k),jsonLiteral(fallback));
}
inline Json object(std::initializer_list<std::pair<core::settings::SettingsString,Json>> fields,
        std::source_location source=std::source_location::current()) {
    Object result;
    try {
        for(const auto& [key,value]:fields)result[key]=value;
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
    return result;
}
inline Json array(std::initializer_list<Json> values,
        std::source_location source=std::source_location::current()) {
    if(values.size()==0)return Array{};
    try {
        return Array(values);
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
template<class T> Json array(Slice<T> values) {
    if constexpr(std::is_same_v<T,Json>) return Json(values);
    else {
        Array result;result.resize(values.size());
        for(std::size_t i=0;i<values.size();++i) result[i]=Json(values[i]);
        return result;
    }
}
inline Json point(Point2 p) {
    Array result;result.resize(p.size());
    for(std::size_t i=0;i<p.size();++i) result[i]=Json(p[i]);
    return result;
}
inline Json duplicate(Json v,std::source_location source=std::source_location::current()) {
    const auto& sourceObject=v.sourceObjectRef();
    Object result;
    try {
        for(const auto& member:sourceObject)result[member.name]=member.item;
    }catch(const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }catch(const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
    return result;
}
inline double smooth(double v) { auto s=clamp(v,0,1);return s*s*(3-2*s); }
inline std::string lower(std::string_view value) { return io::imageexport::detail::lowerExtension(value); }
inline std::string replace(std::string value,std::string_view needle,std::string_view replacement) {
    for(std::size_t p=0;(p=value.find(needle,p))!=value.npos;p+=replacement.size())value.replace(p,needle.size(),replacement);return value;
}
inline bool contains(std::initializer_list<std::string_view> values,std::string_view v) {return std::find(values.begin(),values.end(),v)!=values.end();}
template<class T> bool contains(Slice<T> values,const T& wanted) {
    for(const auto& value:values)if(value==wanted)return true;
    return false;
}
Json currentRole(compat::RetainedStringSlice);
bool physicsRegexSearch(compat::RetainedStringSlice text, compat::RetainedStringSlice pattern,
    std::source_location source = std::source_location::current());
}
