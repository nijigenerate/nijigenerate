#pragma once
// Required production std.algorithm comparison/sorting contracts from Phobos.
// Copyright Andrei Alexandrescu 2008-. BSL-1.0: core/selector/phobos_sort.LICENSE.
#include "../registered.hpp"
#include "../linear.hpp"
#include "../templates.hpp"
#include "../observation.hpp"
#include <nijigenerate/autorig/json.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/io/depthmap_psd_names.hpp>
#include <nijigenerate/core/selector/phobos_sort.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <source_location>
#include <string>
#include <string_view>
#include <new>
#include <stdexcept>
#include <utility>
#include "sha256.hpp"
#include "registered_resource.hpp"
namespace nijigenerate::autorig::deterministic::registered::detail {
using namespace contracts;
using namespace linear;
using core::settings::SettingsJsonArray;
using core::settings::SettingsJsonObject;
inline compat::RetainedString registeredSourceFile() {
    return compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/registered.d");
}

inline compat::RetainedString copyEnforceMessage(std::string_view message) {
    if (message.empty()) return compat::RetainedString::fromStaticLiteral("Enforcement failed");
    try {
        return compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

inline std::string joinEnforceMessage(std::string_view prefix, std::string_view suffix) {
    try {
        std::string result;
        if (prefix.size() > result.max_size() || suffix.size() > result.max_size() - prefix.size())
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        result.resize(prefix.size() + suffix.size());
        std::copy(prefix.begin(), prefix.end(), result.begin());
        std::copy(suffix.begin(), suffix.end(), result.begin() + prefix.size());
        return result;
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<std::size_t N> void enforce(bool test, const char (&message)[N], std::size_t line) {
    if (!test) throw compat::ThrowableException(
        copyEnforceMessage(std::string_view(message, N - 1)), registeredSourceFile(), line);
}
template<class Message> void enforceLazy(bool test, Message&& message, std::size_t line) {
    if (!test) {
        const auto originalMessage = message();
        // std.exception.bailOut duplicates the evaluated message before throwing.
        throw compat::ThrowableException(
            copyEnforceMessage(std::string_view(originalMessage)), registeredSourceFile(), line);
    }
}
template<class Map, class Key> decltype(auto) aa(Map& map, const Key& key, std::size_t line) {
    const auto found = map.find(key);
    if (found == map.end())
        throw compat::ngStaticError<compat::RangeError>(registeredSourceFile(),line);
    return (found->second);
}

template<class T> T& indexed(compat::Slice<T>& values, std::size_t index, std::size_t line) {
    if (index>=values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),registeredSourceFile(),line);
    return values[index];
}

template<class T> const T& indexed(const compat::Slice<T>& values, std::size_t index, std::size_t line) {
    if (index>=values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),registeredSourceFile(),line);
    return values[index];
}
template<class T> T& indexed(compat::Slice<T>& values, std::size_t index,
        std::source_location source=std::source_location::current()) {
    if (index>=values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),source);
    return values[index];
}
template<class T> const T& indexed(const compat::Slice<T>& values, std::size_t index,
        std::source_location source=std::source_location::current()) {
    if (index>=values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),source);
    return values[index];
}
inline std::string registeredResourceDigest() {
    const auto bytes = registeredResource();
    contracts::detail::Sha256 hash;
    hash.put({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    const auto digest = hash.finish();
    std::string result;
    try {
        result.resize(digest.size() * 2);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    constexpr char digits[] = "0123456789ABCDEF";
    std::size_t index = 0;
    for (const auto byte : digest) {
        result[index++] = digits[byte >> 4];
        result[index++] = digits[byte & 15];
    }
    return result;
}
inline Json at(const Json& value, std::string_view key) { return value.sourceIndexValue(key); }
inline Json at(const Json& value, std::size_t index) { return value.sourceIndexValue(index); }
inline void set(Json& value, core::settings::SettingsString key, Json item) {
    value.sourceIndexAssignSlot(std::move(key)) = std::move(item);
}
// The caller supplies static-lifetime bytes; runtime keys retain their real owner.
template<std::size_t N> core::settings::SettingsString literalKey(const char (&key)[N]) {
    return compat::RetainedString::fromStaticLiteral(key);
}
template<std::size_t N> void set(Json& value, const char (&key)[N], Json item) {
    set(value,literalKey(key),std::move(item));
}
inline compat::RetainedStringSlice text(std::string_view value) {
    return core::settings::SettingsString(value);
}
template<std::size_t N> compat::RetainedStringSlice text(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
template<std::size_t N> std::pair<core::settings::SettingsString,Json> objectField(
        const char (&key)[N], Json value) {
    return {literalKey(key),std::move(value)};
}
inline Json object(std::initializer_list<std::pair<core::settings::SettingsString,Json>> values) {
    SettingsJsonObject result; for (const auto& [key,value] : values) result[key] = value; return result;
}
inline Json array(std::initializer_list<Json> values) { SettingsJsonArray out; for (auto value:values) out.append(value); return out; }
inline Json numbersJson(compat::Slice<double> values) { SettingsJsonArray out; for(auto v:values) out.append(v); return out; }
inline double norm(Point2 p) { return std::sqrt(p[0]*p[0]+p[1]*p[1]); }
inline Point2 sub(Point2 a,Point2 b) { return {a[0]-b[0],a[1]-b[1]}; }
inline double dot(Point2 a,Point2 b) { return a[0]*b[0]+a[1]*b[1]; }
inline core::settings::SettingsString lowerString(core::settings::SettingsString value) {
    return io::depthmap_psd::ngPsdDepthLowerString(value.retainedString());
}
inline double dmin(double a,double b) { return b<a?b:a; }
inline double dmax(double a,double b) { return a<b?b:a; }
inline double clamp(double a,double lo,double hi) {
#ifndef NDEBUG
    if (lo>hi) throw compat::ngStaticError<compat::AssertError>(compat::RetainedString::fromStaticLiteral(
        "Lower can't be greater than upper."),
        compat::RetainedString::fromStaticLiteral("std/algorithm/comparison.d"),588);
#endif
    return a<lo?lo:a>hi?hi:a;
}
inline void sort(compat::Slice<double> values) {
    core::selector::detail::phobosQuickSort(std::span<double>(values.data(),values.size()),
        [](double a,double b){return a<b;},values.size(),128);
#ifndef NDEBUG
    // The normal std.algorithm.sort contract runs before its SortedRange result
    // is materialized by std.array.array. No StdUnittest path is included.
    if (!values.empty()) for(std::size_t i=0;i<values.size()-1;++i) {
        const double next=indexed(values,i+1),current=indexed(values,i);
        if (!(next<current)) continue;
        const double reverseLeft=indexed(values,i),reverseRight=indexed(values,i+1);
        if (reverseLeft<reverseRight) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral(
                "Predicate for isSorted is not antisymmetric. Both"
                " pred(a, b) and pred(b, a) are true for certain values."),
            compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"),179);
        throw compat::ngStaticError<compat::AssertError>(compat::RetainedString::fromStaticLiteral(
            "Failed to sort range of type double[]"),
            compat::RetainedString::fromStaticLiteral("std/algorithm/sorting.d"),1973);
    }
#endif
}
double quantile(compat::Slice<double>,double);
double sample(compat::Slice<double>,compat::Slice<double>,compat::Slice<double>,Point2);
compat::Slice<double> anchoredAxis(double,double,std::size_t,compat::Slice<double>);
core::settings::SettingsString componentRole(Json);
struct ChartFrame {
    Point2 origin{NAN, NAN};
    std::array<double,4> matrix{NAN,NAN,NAN,NAN};
    compat::Slice<Point2> centers,coefficients;
    Point2 affineTo(Point2);
    Point2 forward(Point2,std::array<double,4>&);
    Point2 toFrame(Point2);
    Point2 fromFrame(Point2);
    void install(Json,const Landmarks&);
};
}
