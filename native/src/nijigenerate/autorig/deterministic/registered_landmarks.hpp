#pragma once

#include "contracts.hpp"
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/core/settings_string.hpp>
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
#include <cstdint>
#include <initializer_list>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nijigenerate::autorig::deterministic::registered::detail {

// Actual original AA key families: null descriptors allocate independently;
// initialized copies share entries and immutable string keys retain their owner.
template<class Value, class Key = core::settings::SettingsString>
    requires (std::is_same_v<Key, core::settings::SettingsString> || std::is_same_v<Key, std::uint64_t>)
class RegisteredAA {
    static constexpr bool stringKey = std::is_same_v<Key, core::settings::SettingsString>;
    using Lookup = std::conditional_t<stringKey, std::string_view, Key>;
    struct Hash {
        using is_transparent = void;
        std::size_t operator()(const Key& key) const noexcept {
            if constexpr (stringKey) return (*this)(key.view());
            else if constexpr (sizeof(Key) <= sizeof(std::size_t)) return key;
            else return static_cast<std::size_t>(key ^ (key >> (sizeof(std::size_t) * 8)));
        }
        std::size_t operator()(std::string_view key) const noexcept requires (stringKey) {
            return nicxlive::core::d_runtime::ngPhobosStringHash(key);
        }
    };
    struct Equal {
        using is_transparent = void;
        bool operator()(const Key& a, const Key& b) const noexcept {
            if constexpr (stringKey) return a.view() == b.view();
            else return a == b;
        }
        bool operator()(const Key& a, std::string_view b) const noexcept requires (stringKey) {
            return a.view() == b;
        }
        bool operator()(std::string_view a, const Key& b) const noexcept requires (stringKey) {
            return a == b.view();
        }
    };
    using Storage = nicxlive::core::common::ResourceKeyMap<Key, Value, Hash, Equal>;
    static Key probe(Lookup key) {
        if constexpr (stringKey)
            return Key(compat::RetainedString::fromBorrowedView(key));
        else return key;
    }

public:
    using iterator = typename Storage::iterator;
    using const_iterator = typename Storage::const_iterator;

    RegisteredAA() noexcept = default;
    RegisteredAA(const RegisteredAA&) noexcept = default;
    RegisteredAA& operator=(const RegisteredAA&) noexcept = default;
    RegisteredAA(RegisteredAA&& other) noexcept : storage_(other.storage_) {}
    RegisteredAA& operator=(RegisteredAA&& other) noexcept {
        storage_ = other.storage_;
        return *this;
    }
    RegisteredAA(std::initializer_list<std::pair<Key, Value>> entries) {
        for (const auto& [key, value] : entries) (*this)[key] = value;
    }

    bool isNull() const noexcept { return storage_.isNull(); }
    bool empty() const noexcept { return size() == 0; }
    std::size_t size() const noexcept { return storage_.size(); }
    bool contains(Lookup key) const { return storage_.contains(probe(key)); }
    iterator begin() { return storage_.begin(); }
    iterator end() { return storage_.end(); }
    const_iterator begin() const { return storage_.begin(); }
    const_iterator end() const { return storage_.end(); }
    iterator find(Lookup key) { return storage_.find(probe(key)); }
    const_iterator find(Lookup key) const { return storage_.find(probe(key)); }
    Value& at(Lookup key) {
        const auto found = find(key);
        if (found == end())
            throw compat::ngStaticError<compat::RangeError>(std::source_location::current());
        return found->second;
    }
    const Value& at(Lookup key) const {
        const auto found = find(key);
        if (found == end())
            throw compat::ngStaticError<compat::RangeError>(std::source_location::current());
        return found->second;
    }
    Value& operator[](Key key) {
        return storage_[key];
    }
    Value& operator[](compat::RetainedStringSlice key) requires (stringKey) {
        return (*this)[Key(std::move(key))];
    }
    Value& operator[](const std::string& key) requires (stringKey) {
        return (*this)[Key(std::string_view(key))];
    }
    Value& operator[](std::string&& key) requires (stringKey) { return (*this)[Key(std::move(key))]; }
    Value& operator[](std::string_view key) requires (stringKey) { return (*this)[Key(key)]; }
    Value& operator[](const char* key) requires (stringKey) { return (*this)[Key(key)]; }
    bool remove(Lookup key) {
        return storage_.erase(probe(key)) != 0;
    }
    void clear() { storage_.clear(); }
    void reset() noexcept { storage_ = Storage{}; }
    RegisteredAA duplicate() const {
        RegisteredAA result;
        const auto snapshot = storage_;
        for (const auto& entry : snapshot) result[entry.first] = Value(entry.second);
        return result;
    }

private:
    Storage storage_;
};

template<class Value> using RegisteredStringAA = RegisteredAA<Value>;
template<class Value> using RegisteredUlongAA = RegisteredAA<Value, std::uint64_t>;

} // namespace nijigenerate::autorig::deterministic::registered::detail

namespace nijigenerate::autorig::deterministic::registered {
using Landmarks = detail::RegisteredStringAA<contracts::Point2>;
}
