#pragma once
#include "../evidence.hpp"
#include "../templates.hpp"
#include "../neck.hpp"
#include "../registered.hpp"
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/core/selector/phobos_sort.hpp>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <regex>
#include <type_traits>
#include <utility>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/resource_key_map.hpp>
#include <compat/source_allocator.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
namespace nijigenerate::autorig::deterministic::evidence::detail {
using namespace contracts;
using core::settings::SettingsJsonObject;
template<class UInt> struct UnsignedEvidenceHash {
    std::size_t operator()(UInt value) const noexcept { return static_cast<std::size_t>(value); }
};
template<class UInt, class Value> using UnsignedEvidenceMap =
    nicxlive::core::common::ResourceKeyMap<UInt, Value, UnsignedEvidenceHash<UInt>, std::equal_to<UInt>>;

inline compat::RetainedString copiedEvidenceMessage(std::string_view message) {
    try {
        return compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline std::shared_ptr<AlphaSupportIndex> allocateAlphaSupportIndex(compat::Slice<Point2> points) {
    return std::allocate_shared<AlphaSupportIndex>(
        nicxlive::core::d_runtime::SourceAllocator<AlphaSupportIndex>{}, points);
}
inline Json& at(Json& v,std::string_view k) { return v.as_object().at(k); }
inline const Json& at(const Json& v,std::string_view k) { return v.as_object().at(k); }
inline Json atEvidence(const Json& value, std::string_view key) {
    return value.sourceIndexValue(key);
}
inline void set(Json& v,std::string_view k,Json x) { v.as_object()[std::string(k)]=std::move(x); }
template<class Key> void setEvidence(Json& value, Key&& key, Json entry) {
    core::settings::SettingsString retainedKey;
    if constexpr (std::is_array_v<std::remove_reference_t<Key>>)
        retainedKey = compat::RetainedString::fromStaticLiteral(key);
    else
        retainedKey = core::settings::SettingsString(std::forward<Key>(key));
    value.sourceIndexAssignSlot(std::move(retainedKey)) = std::move(entry);
}
template<class Key> void setNestedEvidence(Json& value, std::string_view parentKey, Key&& key, Json entry) {
    SettingsJsonObject retainedObject;
    auto& parent = value.sourceIndexReference(parentKey, retainedObject);
    setEvidence(parent, std::forward<Key>(key), std::move(entry));
}
inline std::string str(const Json& v) { return std::string(v.as_string().view()); }
inline compat::RetainedStringSlice text(std::string_view s) { return compat::RetainedString(s); }
inline void enforce(bool b,std::string s) { if(!b) throw std::runtime_error(s); }
template<std::size_t N> void enforceEvidence(bool condition, const char (&message)[N], std::size_t line) {
    if (!condition) throw compat::ThrowableException(copiedEvidenceMessage(std::string_view(message, N - 1)),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/evidence.d"), line);
}
template<class Message> void enforceEvidenceLazy(bool condition, Message message, std::size_t line) {
    if (condition) return;
    // All defining-module producers only concatenate builtin message bytes.
    compat::RetainedString copy;
    try {
        const auto evaluated = message();
        copy = copiedEvidenceMessage(std::string_view(evaluated));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(copy,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/evidence.d"), line);
}
inline Json object(std::initializer_list<std::pair<std::string_view,Json>> fields) { SettingsJsonObject o; for(auto& [k,v]:fields) o[std::string(k)]=v; return o; }
inline Json array(std::initializer_list<Json> values) { JsonArray a;for(auto v:values)a.append(v);return a; }
template<std::size_t N> core::settings::SettingsString stringLiteralEvidence(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
template<std::size_t N> Json literalEvidence(const char (&value)[N]) {
    return stringLiteralEvidence(value);
}
inline Json objectEvidence(std::initializer_list<core::settings::SettingsJsonMember> fields) {
    SettingsJsonObject result;
    for (const auto& field : fields) result[field.name] = field.item;
    return result;
}
template<std::size_t N> Json getEvidence(Json value, const char (&key)[N], Json fallback = {}) {
    return ngRigGet(value, stringLiteralEvidence(key), std::move(fallback));
}
inline Json duplicate(Json v) { SettingsJsonObject o;for(const auto& m:v.as_object())o[std::string(m.key())]=m.value();return o; }
inline SettingsJsonObject& evidenceObject(Json& value) {
    return value.sourceObjectRef();
}
inline Json duplicateEvidence(Json v) {
    return evidenceObject(v).duplicate();
}
inline Json get(Json v,std::string_view k,Json fallback={}) { return ngRigGet(v,text(k),fallback); }
inline std::string stringGet(Json v,std::string_view k,std::string_view fallback="") { return std::string(ngRigString(v,text(k),text(fallback)).view()); }
inline double dmin(double a,double b) { return b<a?b:a; }
inline double dmax(double a,double b) { return a<b?b:a; }
inline double norm(Point2 p) { return std::sqrt(p[0]*p[0]+p[1]*p[1]); }
inline Point2 minus(Point2 a,Point2 b) { return {a[0]-b[0],a[1]-b[1]}; }
inline Point2 midpoint(Point2 a,Point2 b) { return {(a[0]+b[0])/2,(a[1]+b[1])/2}; }
inline Json pointJson(Point2 p) { return array({p[0],p[1]}); }
template<class T,class Compare> void sort(compat::Slice<T> a,Compare less) {
    // LDC Windows source sizes, not C++ carrier sizes: JSONValue=32, vec2d/string=16,
    // double/size_t=8. Phobos quickSortImpl uses max(32,1024/Elem.sizeof).
    constexpr std::size_t sourceSize=std::is_same_v<T,Json>?32:
        (std::is_same_v<T,Point2>||std::is_same_v<T,std::string>)?16:8;
    core::selector::detail::phobosQuickSort(std::span<T>(a.data(),a.size()),less,a.size(),1024/sourceSize);
}
inline std::vector<std::string> split(std::string_view v,char separator) {
    std::vector<std::string> result;std::size_t p=0;
    for(;;) { auto n=v.find(separator,p);result.emplace_back(v.substr(p,n==v.npos?v.size()-p:n-p));if(n==v.npos)break;p=n+1; } return result;
}
double quantile(compat::Slice<double>,double);
std::string nameTokens(compat::RetainedStringSlice);
compat::RetainedStringSlice semanticRole(compat::RetainedStringSlice,Strings);
}
