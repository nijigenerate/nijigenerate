#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <common/nullable_string.hpp>
#include <common/object_key_policy.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cstddef>
#include <new>
#include <source_location>
#include <stdexcept>
#include <typeinfo>
#include <utility>

namespace nijigenerate::core::selector {
using ResourceString = nicxlive::core::common::NullableString;
enum class ResourceType : std::int32_t { Node, Parameter, Binding, Vec2, Vec3, Transform, IntArray, FloatArray };

namespace detail {
template<class Class, class Argument> void ngRegisterSelectorClass(std::string_view declaration);

inline ResourceString resourceComparisonMessage(const std::type_info& type) {
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(type);
    try {
        std::string message = "need opCmp for class ";
        message += className.view();
        return ResourceString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

// Every D ResourceInfo specialization is a reference class, including the primary.
// Inherited Object defaults follow independent object.d (Digital Mars, BSL-1.0);
// the canonical provider's full notice is licenses/druntime-associative-array.txt.
struct ResourceInfoIdentity {
    ResourceInfoIdentity() = default;
    virtual ~ResourceInfoIdentity() = default;
    ResourceInfoIdentity(const ResourceInfoIdentity&) = delete;
    ResourceInfoIdentity& operator=(const ResourceInfoIdentity&) = delete;
    virtual ResourceString toString() {
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).toNullableString();
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<ResourceInfoIdentity> other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
    }
    virtual int opCmp(std::shared_ptr<ResourceInfoIdentity>) {
        const auto message = detail::resourceComparisonMessage(typeid(*this));
        throw compat::ThrowableException(message,
            ResourceString::fromStaticLiteral("object.d"), 195);
    }
};

template<class T> struct ResourceInfo : ResourceInfoIdentity {
    ResourceInfo() { detail::ngRegisterSelectorClass<ResourceInfo<T>, T>("ResourceInfo"); }
    // The D primary declares this method without defining a fallback category.
    static ResourceType type();
};

class Resource {
protected:
    ResourceType type_ = ResourceType::Node;
public:
    Resource() = default;
    // Copy the shared handle, never the identity-bearing D class instance.
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    std::shared_ptr<Resource> source;
    // "explicit" is a C++ keyword; all references to D Resource.explicit map here.
    bool explicitSelection = false;
    std::uint64_t index = 0;
    virtual ~Resource() = default;
    virtual ResourceString name() = 0;
    virtual std::uint32_t uuid() = 0;
    virtual ResourceString typeId() = 0;
    virtual ResourceType type() { return type_; }
    // The source class inherits Object's overridable protocol, independently
    // of the SDK resource wrapped by a Proxy. AA keys dispatch on this object.
    virtual ResourceString toString() {
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this)).toNullableString();
    }
    virtual std::size_t toHash() {
        const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
        return address ^ (address >> 4);
    }
    virtual bool opEquals(std::shared_ptr<Resource> other) {
        return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
    }
    virtual int opCmp(std::shared_ptr<Resource>) {
        const auto message = detail::resourceComparisonMessage(typeid(*this));
        throw compat::ThrowableException(message,
            ResourceString::fromStaticLiteral("object.d"), 195);
    }
};
using ResourcePtr = std::shared_ptr<Resource>;

inline bool ngSelectorResourceObjectEquals(ResourcePtr left, ResourcePtr right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}
}

// The original module exports its real SDK specializations and conversions too.
#include "resource_proxy.hpp"
