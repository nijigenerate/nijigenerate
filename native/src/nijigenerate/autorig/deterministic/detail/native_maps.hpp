#pragma once
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace nijigenerate::autorig::deterministic::native_detail {
// Actual uint/ulong key origins: core/internal/hash.d263-266.
template<class Key> struct NativeIntegerHash {
    std::size_t operator()(Key key) const noexcept {
        if constexpr (sizeof(Key) <= sizeof(std::size_t)) return static_cast<std::size_t>(key);
        else return static_cast<std::size_t>(key ^ (key >> (sizeof(std::size_t) * 8)));
    }
};
template<class Key, class Value> using NativeIntegerMap = nicxlive::core::common::ResourceKeyMap<
    Key, Value, NativeIntegerHash<Key>, std::equal_to<Key>>;
template<class Map, class Key> auto readNativeMap(const Map& map, const Key& key, std::size_t line) {
    const auto found = map.find(key);
    if (found == map.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/autorig/deterministic/native.d"), line);
    return found->second;
}
}
