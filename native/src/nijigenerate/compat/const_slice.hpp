#pragma once

#include "slice.hpp"
#include "throwable.hpp"
#include <memory>
#include <span>
#include <stdexcept>

namespace nijigenerate::compat {

// A read-only descriptor retains its original backing; conversion allocates nothing.
template<class T> class ConstSlice {
public:
    ConstSlice() = default;
    ConstSlice(const ConstSlice&) = default;
    ConstSlice& operator=(const ConstSlice& other) noexcept {
        if (this == &other) return *this;
        const auto previousOwner = owner_;
        owner_ = other.owner_;
        data_ = other.data_;
        length_ = other.length_;
        return *this;
    }
    // Moving a native argument must not invalidate another retained D descriptor.
    ConstSlice(ConstSlice&& other) noexcept
        : owner_(other.owner_), data_(other.data_), length_(other.length_) {}
    ConstSlice& operator=(ConstSlice&& other) noexcept {
        return *this = static_cast<const ConstSlice&>(other);
    }
    ConstSlice(Slice<T> source) noexcept
        : owner_(source.retentionOwner()), data_(source.data()), length_(source.size()) {}
    static ConstSlice retainedView(std::shared_ptr<const void> owner, const T* data, std::size_t length) {
        if ((data || length) && !owner) throw std::invalid_argument("Const slice requires backing owner");
        if (length && !data) throw std::invalid_argument("Const slice requires elements");
        ConstSlice result;
        result.owner_ = std::move(owner);
        result.data_ = data;
        result.length_ = length;
        return result;
    }
    bool isNull() const { return !owner_ && !data_; }
    bool empty() const { return length_ == 0; }
    std::size_t size() const { return length_; }
    const T* data() const { return data_; }
    std::shared_ptr<const void> retentionOwner() const noexcept { return owner_; }
    const T* begin() const { return data_; }
    const T* end() const { return length_ ? data_ + length_ : data_; }
    const T& operator[](std::size_t index) const {
        if (index >= length_) throw ngStaticError<ArrayIndexError>(index, length_, std::source_location::current());
        return data_[index];
    }
    std::span<const T> view() const { return {data_, length_}; }
    ConstSlice slice(std::size_t first, std::size_t last) const {
        if (first > last || last > length_)
            throw ngStaticError<ArraySliceError>(first, last, length_, std::source_location::current());
        auto result = *this;
        if (first) result.data_ += first;
        result.length_ = last - first;
        return result;
    }
private:
    std::shared_ptr<const void> owner_;
    const T* data_ = nullptr;
    std::size_t length_ = 0;
};

} // namespace nijigenerate::compat
