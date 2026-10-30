#pragma once
#include "array_range_error.hpp"
#include "throwable.hpp"
#include "out_of_memory_error.hpp"
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>

#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nijigenerate::compat {

// Retained, shallow-copy slices. Capacity is a C++ allocation detail; callers
// must not rely on D GC-specific append capacity. See the M103 contract.
template<class T> class Slice {
    using Storage = std::vector<T, nicxlive::core::d_runtime::SourceAllocator<T>>;
public:
    Slice() = default;
    Slice(const Slice&) = default;
    Slice& operator=(const Slice& other) noexcept {
        if (this == &other) return *this;
        // Keep old backing alive until every new descriptor field is published.
        const auto previousStorage = storage_;
        const auto previousNativeStorage = nativeStorage_;
        const auto previousOwner = externalOwner_;
        storage_ = other.storage_;
        nativeStorage_ = other.nativeStorage_;
        externalOwner_ = other.externalOwner_;
        externalData_ = other.externalData_;
        length_ = other.length_;
        offset_ = other.offset_;
        return *this;
    }
    // Native move transport retains both D slice descriptors and their backing.
    Slice(Slice&& other) noexcept
        : storage_(other.storage_), nativeStorage_(other.nativeStorage_),
          externalOwner_(other.externalOwner_), externalData_(other.externalData_),
          length_(other.length_), offset_(other.offset_) {}
    Slice& operator=(Slice&& other) noexcept {
        return *this = static_cast<const Slice&>(other);
    }
    Slice(std::initializer_list<T> values) : Slice(values.begin(), values.end()) {}
    explicit Slice(std::vector<T> values)
        : nativeStorage_(std::allocate_shared<std::vector<T>>(
              nicxlive::core::d_runtime::SourceAllocator<std::vector<T>>{}, std::move(values))),
          length_(nativeStorage_->size()) {}
    explicit Slice(std::span<const T> values) : Slice(values.begin(), values.end()) {}
    template<class Iterator> Slice(Iterator first, Iterator last)
        : storage_(std::allocate_shared<Storage>(
              nicxlive::core::d_runtime::SourceAllocator<Storage>{}, first, last)), length_(storage_->size()) {}
    // The caller guarantees process-static storage, as for a D import cast.
    // This descriptor neither allocates an owner token nor owns those bytes.
    static Slice fromStaticBuffer(T* values, std::size_t length) noexcept {
        Slice result;
        result.externalData_ = values;
        result.length_ = length;
        return result;
    }
    // The caller retains a descriptor whose backing cannot move while this owner lives.
    // Growing appends detach; shrinking/slicing preserve the original element cells.
    static Slice retainedView(std::shared_ptr<void> owner, T* values, std::size_t length) {
        if (length && (!owner || !values)) throw std::invalid_argument("Retained slice requires backing");
        Slice result; result.externalOwner_ = std::move(owner);
        result.externalData_ = values; result.length_ = length; return result;
    }
    std::size_t size() const { return length_; }
    bool empty() const { return length_ == 0; }
    bool isNull() const { return !storage_ && !nativeStorage_ && !externalOwner_ && !externalData_; }
    std::shared_ptr<void> retentionOwner() const noexcept {
        if (storage_) return storage_;
        if (nativeStorage_) return nativeStorage_;
        return externalOwner_;
    }
    T* data() {
        auto base = storage_ ? storage_->data() : nativeStorage_ ? nativeStorage_->data() : externalData_;
        return offset_ ? base + offset_ : base;
    }
    const T* data() const {
        auto base = storage_ ? storage_->data() : nativeStorage_ ? nativeStorage_->data() : externalData_;
        return offset_ ? base + offset_ : base;
    }
    T* begin() { return data(); }
    const T* begin() const { return data(); }
    T* end() { return length_ ? data() + length_ : data(); }
    const T* end() const { return length_ ? data() + length_ : data(); }
    T& operator[](std::size_t i) {
        if (i >= length_) throw ngStaticError<ArrayIndexError>(i, length_, std::source_location::current());
        return data()[i];
    }
    const T& operator[](std::size_t i) const {
        if (i >= length_) throw ngStaticError<ArrayIndexError>(i, length_, std::source_location::current());
        return data()[i];
    }
    T get(std::size_t i) const { return (*this)[i]; }
    void set(std::size_t i, T value) { (*this)[i] = std::move(value); }
    void append(T value) {
        if (length_ == (std::numeric_limits<std::size_t>::max)()) {
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
        }
        requireStorageExtent(length_ + 1);
        if ((storage_ || nativeStorage_) && !externalOwner_ && !externalData_
                && offset_ + length_ == storageSize() && storageSize() < storageCapacity()) {
            // Existing capacity means the element constructor is the only throwing operation.
            if (storage_) storage_->push_back(std::move(value));
            else nativeStorage_->push_back(std::move(value));
            ++length_;
            return;
        }
        // Publish a detached descriptor only after the actual append succeeds.
        const auto input = *this;
        auto replacement = input.empty() ? Slice{} : Slice(input.begin(), input.end());
        if (!replacement.storage_) replacement.storage_ = allocateStorage();
        replacement.storage_->push_back(std::move(value));
        ++replacement.length_;
        *this = std::move(replacement);
    }
    void append(Slice values) {
        const auto count = values.size();
        for (std::size_t i = 0; i < count; ++i) append(values.get(i));
    }
    void appendRange(Slice values) {
        const auto count = values.size();
        if (!count) return;
        const auto oldLength = length_;
        if (count > (std::numeric_limits<std::size_t>::max)() - oldLength)
            throw ngStaticError<OutOfMemoryError>(
                RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        const auto newLength = oldLength + count;
        requireStorageExtent(newLength);
        // Retain the actual old backing across allocation, publication and copies.
        const auto previous = *this;
        const bool reuse = (storage_ || nativeStorage_) && !externalOwner_ && !externalData_
            && offset_ + length_ == storageSize()
            && count <= storageCapacity() - storageSize();
        Slice target;
        if (reuse) {
            // Capacity is already available; default-constructor failures are not allocation failures.
            if (storage_) storage_->resize(storage_->size() + count);
            else nativeStorage_->resize(nativeStorage_->size() + count);
            target = *this;
            target.length_ = newLength;
        } else {
            target.storage_ = std::allocate_shared<Storage>(
                nicxlive::core::d_runtime::SourceAllocator<Storage>{}, newLength);
            target.length_ = newLength;
        }
        if (!reuse)
            for (std::size_t i = 0; i < oldLength; ++i) target.data()[i] = previous.data()[i];
        // Source range append publishes the full extent before copying its input.
        *this = target;
        for (std::size_t i = 0; i < count; ++i) target.data()[oldLength + i] = values.data()[i];
    }
    Slice duplicate() const {
        const auto input = *this;
        if (input.length_ == 0) return {};
        Slice result;
        try {
            result.storage_ = allocateStorage();
            result.storage_->reserve(input.length_);
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(
                RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(
                RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
        // Capacity is ready; element-copy exceptions belong to the element.
        for (std::size_t i = 0; i < input.length_; ++i)
            result.storage_->emplace_back(input.data()[i]);
        result.length_ = input.length_;
        return result;
    }
    Slice slice(std::size_t first, std::size_t last,
            std::source_location source = std::source_location::current()) const {
        if (first > last || last > length_) throw ngStaticError<ArraySliceError>(first, last, length_, source);
        auto result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        return result;
    }
    void resize(std::size_t count, T initial = T{}) {
        if (count <= length_) { length_ = count; return; }
        while (length_ < count) append(initial);
    }
    void clear() {
        storage_.reset(); nativeStorage_.reset(); externalOwner_.reset();
        externalData_ = nullptr; length_ = 0; offset_ = 0;
    }
private:
    static std::shared_ptr<Storage> allocateStorage() {
        return std::allocate_shared<Storage>(nicxlive::core::d_runtime::SourceAllocator<Storage>{});
    }
    static void requireStorageExtent(std::size_t count) {
        if (count > Storage{}.max_size())
            throw ngStaticError<OutOfMemoryError>(std::source_location::current());
    }
    std::size_t storageSize() const {
        return storage_ ? storage_->size() : nativeStorage_ ? nativeStorage_->size() : 0;
    }
    std::size_t storageCapacity() const {
        return storage_ ? storage_->capacity() : nativeStorage_ ? nativeStorage_->capacity() : 0;
    }
    std::shared_ptr<Storage> storage_;
    // Preserve the actual allocation adopted by the existing native-vector bridge.
    std::shared_ptr<std::vector<T>> nativeStorage_;
    std::shared_ptr<void> externalOwner_;
    T* externalData_ = nullptr;
    std::size_t length_{};
    std::size_t offset_{};
};

// Actual bool elements are addressable; std::vector<bool> cannot supply bool& or bool*.
// This explicit specialization preserves the canonical Slice<bool> public surface.
template<> class Slice<bool> {
    struct Storage {
        std::unique_ptr<bool[]> values;
        std::size_t size = 0;
        std::size_t capacity = 0;

        explicit Storage(std::size_t count = 0)
            : values(count ? std::make_unique<bool[]>(count) : std::unique_ptr<bool[]>{}), capacity(count) {}
    };

public:
    Slice() = default;
    Slice(const Slice&) = default;
    Slice& operator=(const Slice& other) noexcept {
        if (this == &other) return *this;
        const auto previousStorage = storage_;
        const auto previousOwner = externalOwner_;
        storage_ = other.storage_;
        externalOwner_ = other.externalOwner_;
        externalData_ = other.externalData_;
        length_ = other.length_;
        offset_ = other.offset_;
        return *this;
    }
    // Native move transport must retain the source D descriptor as a value copy.
    Slice(Slice&& other) noexcept
        : storage_(other.storage_), externalOwner_(other.externalOwner_), externalData_(other.externalData_),
          length_(other.length_), offset_(other.offset_) {}
    Slice& operator=(Slice&& other) noexcept {
        return *this = static_cast<const Slice&>(other);
    }
    Slice(std::initializer_list<bool> values) : Slice(values.begin(), values.end()) {}
    // An explicit native input bridge is copied into actual bool[] storage.
    explicit Slice(std::vector<bool> values) : Slice(values.begin(), values.end()) {}
    explicit Slice(std::span<const bool> values) : Slice(values.begin(), values.end()) {}
    template<class Iterator> Slice(Iterator first, Iterator last)
        : storage_(std::allocate_shared<Storage>(nicxlive::core::d_runtime::SourceAllocator<Storage>{})) {
        for (; first != last; ++first) append(static_cast<bool>(*first));
    }

    // Same typed retained-view contract as the current genuine primary template.
    static Slice retainedView(std::shared_ptr<void> owner, bool* values, std::size_t length) {
        if (length && (!owner || !values)) throw std::invalid_argument("Retained slice requires backing");
        Slice result; result.externalOwner_ = std::move(owner);
        result.externalData_ = values; result.length_ = length; return result;
    }
    std::size_t size() const { return length_; }
    bool empty() const { return length_ == 0; }
    bool isNull() const { return !storage_ && !externalOwner_ && !externalData_; }
    std::shared_ptr<void> retentionOwner() const noexcept {
        if (storage_) return storage_;
        return externalOwner_;
    }
    bool* data() {
        const auto base = storage_ ? storage_->values.get() : externalData_;
        return base ? base + offset_ : nullptr;
    }
    const bool* data() const {
        const auto base = storage_ ? storage_->values.get() : externalData_;
        return base ? base + offset_ : nullptr;
    }
    bool* begin() { return data(); }
    const bool* begin() const { return data(); }
    bool* end() { return length_ ? data() + length_ : data(); }
    const bool* end() const { return length_ ? data() + length_ : data(); }
    bool& operator[](std::size_t i) {
        if (i >= length_) throw ngStaticError<ArrayIndexError>(i, length_, std::source_location::current());
        return data()[i];
    }
    const bool& operator[](std::size_t i) const {
        if (i >= length_) throw ngStaticError<ArrayIndexError>(i, length_, std::source_location::current());
        return data()[i];
    }
    bool get(std::size_t i) const { return (*this)[i]; }
    void set(std::size_t i, bool value) { (*this)[i] = value; }
    void append(bool value) {
        try {
            if (externalOwner_) *this = duplicate();
            if (storage_ && (offset_ + length_ != storage_->size || storage_->size == storage_->capacity))
                *this = duplicate();
            if (!storage_) storage_ = std::make_shared<Storage>();
            if (storage_->size == storage_->capacity) {
                const auto maximum = (std::numeric_limits<std::size_t>::max)() / sizeof(bool);
                if (storage_->size == maximum) throw std::length_error("Slice size");
                const auto capacity = storage_->capacity > (maximum - 1) / 2 ?
                    maximum : storage_->capacity * 2 + 1;
                auto replacement = std::make_shared<Storage>(capacity);
                replacement->size = storage_->size;
                for (std::size_t i = 0; i < storage_->size; ++i) replacement->values[i] = storage_->values[i];
                storage_ = std::move(replacement);
            }
            storage_->values[storage_->size] = value;
            ++storage_->size;
            ++length_;
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
        }
    }
    void append(Slice values) {
        const auto count = values.size();
        for (std::size_t i = 0; i < count; ++i) append(values.get(i));
    }
    Slice duplicate() const {
        if (length_ == 0) return {};
        Slice result;
        try {
            result.storage_ = std::make_shared<Storage>(length_);
        } catch (const std::bad_alloc&) {
            throw ngStaticError<OutOfMemoryError>(
                RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        } catch (const std::length_error&) {
            throw ngStaticError<OutOfMemoryError>(
                RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
        }
        result.storage_->size = length_;
        result.length_ = length_;
        for (std::size_t i = 0; i < length_; ++i) result.storage_->values[i] = data()[i];
        return result;
    }
    Slice slice(std::size_t first, std::size_t last,
            std::source_location source = std::source_location::current()) const {
        if (first > last || last > length_) throw ngStaticError<ArraySliceError>(first, last, length_, source);
        auto result = *this;
        result.offset_ += first;
        result.length_ = last - first;
        return result;
    }
    void resize(std::size_t count, bool initial = false) {
        if (count <= length_) { length_ = count; return; }
        while (length_ < count) append(initial);
    }
    void clear() {
        storage_.reset(); externalOwner_.reset(); externalData_ = nullptr; length_ = 0; offset_ = 0;
    }

private:
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<void> externalOwner_;
    bool* externalData_ = nullptr;
    std::size_t length_{};
    std::size_t offset_{};
};

} // namespace nijigenerate::compat

namespace nicxlive::core::d_runtime {
// This actual App carrier represents Source T[]; the element's declaration
// supplies each independent raw spelling through the canonical provider.
template<class T> struct SourceTypeSpellings<::nijigenerate::compat::Slice<T>> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() { return ngSourceDynamicArrayTypeSpellings<T>(); }
};
} // namespace nicxlive::core::d_runtime
