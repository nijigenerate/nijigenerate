#pragma once

#include "retained_string.hpp"
#include "range_error.hpp"
#include "slice.hpp"
#include "out_of_memory_error.hpp"
#include <common/resource_key_map.hpp>
#include <compat/phobos_string_hash.hpp>
#include <concepts>
#include <cstddef>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace nijigenerate::compat {

// Retained string-key AA storage on the canonical Source table.
// Copies of null handles remain
// independent on first insertion; initialized handles retain shared storage.
template<class Value> requires (std::same_as<Value, bool> || std::same_as<Value, RetainedString>
    || std::same_as<Value, Slice<RetainedString>>)
class RetainedStringMap {
    struct Hash {
        std::size_t operator()(const RetainedString& key) const noexcept {
            return nicxlive::core::d_runtime::ngPhobosStringHash(key.view());
        }
    };
    struct Equal {
        bool operator()(const RetainedString& left, const RetainedString& right) const noexcept {
            return left.view() == right.view();
        }
    };
    using Storage = nicxlive::core::common::ResourceKeyMap<RetainedString, Value, Hash, Equal>;

public:
    RetainedStringMap() noexcept = default;
    bool isNull() const noexcept { return storage_.isNull(); }
    bool empty() const noexcept { return size() == 0; }
    std::size_t size() const noexcept { return storage_.size(); }
    const Value* find(std::string_view key) const {
        return findRetained(key).get();
    }
    // Retain the same key/value allocation, even after its index entry is removed.
    std::shared_ptr<const Value> findRetained(std::string_view key) const {
        return storage_.findRetained(RetainedString::fromBorrowedView(key));
    }
    bool contains(std::string_view key) const { return find(key) != nullptr; }
    Value get(std::string_view key) const {
        const auto found = findRetained(key);
        if (!found) throw ngStaticError<RangeError>(__FILE__, __LINE__);
        return *found;
    }
    void set(std::string_view key, Value value) {
        const auto probe = RetainedString::fromBorrowedView(key);
        if (storage_.contains(probe)) {
            auto destination = storage_[probe];
            destination = std::move(value);
            return;
        }
        auto ownedKey = allocateBytes([&] { return RetainedString(key); });
        setRetained(std::move(ownedKey), std::move(value));
    }
    void setRetained(RetainedString key, Value value) {
        auto destination = storage_[key];
        destination = std::move(value);
    }
    bool remove(std::string_view key) {
        return storage_.erase(RetainedString::fromBorrowedView(key)) != 0;
    }
    // Clear the initialized shared AA, so existing aliases observe the change.
    // A null handle remains null; resetting one descriptor is a different action.
    void clear() { storage_.clear(); }
    void reset() noexcept { storage_ = Storage{}; }
    // Copy entries into an independent AA, as a null-initialized D foreach copy.
    // Values retain their original descriptors; an empty source produces null.
    RetainedStringMap duplicate() const {
        RetainedStringMap result;
        const auto owner = storage_;
        for (const auto& item : owner)
            result.setRetained(item.first, Value(item.second));
        return result;
    }
    template<class F> void forEach(F&& visitor) const {
        const auto owner = storage_;
        for (const auto& item : owner) {
            const auto key = item.first;
            visitor(key.view(), Value(item.second));
        }
    }
    template<class F> void forEachRetained(F&& visitor) const {
        const auto owner = storage_;
        for (const auto& item : owner) {
            const auto key = item.first;
            visitor(RetainedString(key), Value(item.second));
        }
    }

private:
    template<class Operation> static decltype(auto) allocateBytes(Operation&& operation) {
        try {
            return std::forward<Operation>(operation)();
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
    }
    Storage storage_;
};

} // namespace nijigenerate::compat
