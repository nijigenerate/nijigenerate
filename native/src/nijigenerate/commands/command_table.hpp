#pragma once

#include <nijigenerate/core/selector/tokenizer.hpp>
#include <common/resource_key_map.hpp>
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>

namespace nijigenerate::commands {

class Command;

// Integral enum hashOf returns the original signed value converted to size_t.
// The canonical runtime provider performs hash mixing and bucket traversal.
template<class Key> struct CommandKeyHash {
    std::size_t operator()(Key key) const noexcept {
        static_assert(std::is_enum_v<Key>);
        return static_cast<std::size_t>(static_cast<std::underlying_type_t<Key>>(key));
    }
};

template<class Key, class Value, class Hash = CommandKeyHash<Key>, class Equal = std::equal_to<Key>>
class CommandMapStorage : public nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal> {
    using Base = nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal>;
public:
    // Preserve the existing command-map lvalue API on the same real entry.
    // No shadow storage or reordered copy is created by this facade.
    Value& operator[](const Key& key) requires requires(const Key& query, const Key& stored) {
        { Hash{}(query) } -> std::convertible_to<std::size_t>;
        { Equal{}(query, stored) } -> std::convertible_to<bool>;
    } {
        auto cell = Base::operator[](key);
        return static_cast<Value&>(cell);
    }
};

template<class Key>
using CommandTableStorage = CommandMapStorage<Key, std::shared_ptr<Command>>;

}
