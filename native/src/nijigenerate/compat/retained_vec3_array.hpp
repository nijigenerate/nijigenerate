#pragma once

#include <math/veca.hpp>
#include "assert_error.hpp"
#include "array_access.hpp"
#include <initializer_list>
#include <memory>
#include <source_location>

namespace nijigenerate::compat {

// Store the genuine SDK SoA descriptor by value. Its lane descriptors retain
// allocations while a size change replaces only the receiving descriptor.
class RetainedVec3Array {
    using Storage = nicxlive::core::math::Vec3Array;
public:
    using Value = nicxlive::core::math::Vec3;
    RetainedVec3Array() = default;
    RetainedVec3Array(const RetainedVec3Array&) = default;
    // Source veca.opAssign copies elements into the current destination lanes.
    RetainedVec3Array& operator=(const RetainedVec3Array&) = default;
    RetainedVec3Array(RetainedVec3Array&& other) : storage_(other.storage_) {}
    RetainedVec3Array& operator=(RetainedVec3Array&& other) {
        return *this = static_cast<const RetainedVec3Array&>(other);
    }
    RetainedVec3Array(std::initializer_list<Value> values) : storage_(values) {}
    bool isNull() const noexcept {
        return storage_.x.isNull() && storage_.y.isNull() && storage_.z.isNull();
    }
    bool empty() const noexcept { return size() == 0; }
    std::size_t size() const noexcept { return storage_.size(); }
    void checkViewIndex(std::size_t index, std::source_location source = std::source_location::current()) const {
#ifndef NDEBUG
        if (index >= size())
            throw ngStaticError<AssertError>(
                ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("veca index out of range"),
                ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 139);
#else
        (void)index;
#endif
        (void)source;
    }
    Value at(std::size_t index, std::source_location source = std::source_location::current()) const {
        checkViewIndex(index, source);
        return viewValue(index, source);
    }
    // A source vecv keeps a pointer to the current lane descriptors. Converting
    // that view reads those lanes now, with array bounds rather than opIndex's
    // separate assertion at the earlier point where the view was created.
    Value viewValue(std::size_t index, std::source_location source = std::source_location::current()) const {
        (void)source;
        return {sourceRead(storage_.x, index), sourceRead(storage_.y, index), sourceRead(storage_.z, index)};
    }
    void set(std::size_t index, Value value,
            std::source_location source = std::source_location::current()) {
        checkViewIndex(index, source);
        ngArrayAt(storage_.x, index, source) = value.x;
        ngArrayAt(storage_.y, index, source) = value.y;
        ngArrayAt(storage_.z, index, source) = value.z;
    }
    void append(Value value) { storage_.append(value); }
    void clear() { storage_ = Storage{}; }
    // out parameters are default initialized, not assigned through veca.opAssign.
    void resetForOut() {
        std::destroy_at(std::addressof(storage_));
        std::construct_at(std::addressof(storage_));
    }
private:
    static float sourceRead(const nicxlive::core::common::SharedArray<float>& lane, std::size_t index) {
        if (index >= lane.size()) throw ngStaticError<ArrayIndexError>(index, lane.size(),
            ::nicxlive::core::d_runtime::RetainedString::fromStaticLiteral("nijilive/math/veca.d"), 467);
        return lane[index];
    }
    Storage storage_;
};

} // namespace nijigenerate::compat
