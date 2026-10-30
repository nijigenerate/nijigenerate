#pragma once

#include <nijigenerate/compat/range_error.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>

namespace nijigenerate_tests::regression::detail {

template<class T> class UIntAssociativeArray;

// A membership result retains the actual value, including after key removal.
// A raw pointer from get() is borrowed for the lifetime of this real handle.
template<class T> class UIntArrayMember {
public:
    UIntArrayMember() noexcept = default;
    UIntArrayMember(const UIntArrayMember&) noexcept = default;
    UIntArrayMember& operator=(const UIntArrayMember&) noexcept = default;
    UIntArrayMember(UIntArrayMember&& other) noexcept : value_(other.value_) {}
    UIntArrayMember& operator=(UIntArrayMember&& other) noexcept {
        value_ = other.value_;
        return *this;
    }
    explicit operator bool() const noexcept { return static_cast<bool>(value_); }
    T* get() const noexcept { return value_.get(); }
    T& operator*() const noexcept { return *value_; }
    T* operator->() const noexcept { return value_.get(); }
private:
    template<class> friend class UIntAssociativeArray;
    explicit UIntArrayMember(std::shared_ptr<T> value) noexcept : value_(std::move(value)) {}
    std::shared_ptr<T> value_;
};

// The original module's uint-key AA descriptors. No value is default-constructed
// by a null declaration, a read lookup, or copying a descriptor.
template<class T> class UIntAssociativeArray {
    using Storage = std::unordered_map<std::uint32_t, std::shared_ptr<T>>;
public:
    UIntAssociativeArray() noexcept = default;
    UIntAssociativeArray(std::nullptr_t) noexcept {}
    UIntAssociativeArray(const UIntAssociativeArray&) noexcept = default;
    UIntAssociativeArray& operator=(const UIntAssociativeArray&) noexcept = default;
    UIntAssociativeArray(UIntAssociativeArray&& other) noexcept : storage_(other.storage_) {}
    UIntAssociativeArray& operator=(UIntAssociativeArray&& other) noexcept {
        storage_ = other.storage_;
        return *this;
    }
    UIntAssociativeArray& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }
    bool isNull() const noexcept { return !storage_; }
    std::size_t size() const noexcept { return storage_ ? storage_->size() : 0; }
    bool empty() const noexcept { return size() == 0; }
    UIntArrayMember<T> find(std::uint32_t key) {
        if (!storage_) return {};
        const auto found = storage_->find(key);
        return found == storage_->end() ? UIntArrayMember<T>{} : UIntArrayMember<T>(found->second);
    }
    UIntArrayMember<const T> find(std::uint32_t key) const {
        if (!storage_) return {};
        const auto found = storage_->find(key);
        return found == storage_->end() ? UIntArrayMember<const T>{}
                                       : UIntArrayMember<const T>(found->second);
    }
    bool contains(std::uint32_t key) const { return static_cast<bool>(find(key)); }
    UIntArrayMember<T> at(std::uint32_t key) {
        auto value = find(key);
        if (!value) throw nijigenerate::compat::RangeError();
        return value;
    }
    UIntArrayMember<const T> at(std::uint32_t key) const {
        auto value = find(key);
        if (!value) throw nijigenerate::compat::RangeError();
        return value;
    }
    void insertOrAssign(std::uint32_t key, T value) {
        if (auto existing = find(key)) {
            *existing = std::move(value);
            return;
        }
        auto member = std::make_shared<T>(std::move(value));
        auto backing = storage_;
        if (!backing) backing = std::make_shared<Storage>();
        backing->emplace(key, std::move(member));
        storage_ = std::move(backing);
    }
    bool remove(std::uint32_t key) {
        return storage_ && storage_->erase(key) != 0;
    }
    void clear() {
        if (storage_) storage_->clear();
    }
    void reset() noexcept { storage_.reset(); }
private:
    std::shared_ptr<Storage> storage_;
};
} // namespace nijigenerate_tests::regression::detail
