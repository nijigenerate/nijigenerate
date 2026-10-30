#pragma once
#include "param_metadata.hpp"

#include <param/parameter.hpp>
#include <common/nullable_string.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nijigenerate::ext {
using Parameter = nicxlive::core::param::Parameter;
using Puppet = nicxlive::core::Puppet;
using ParameterPtr = std::shared_ptr<Parameter>;
namespace serde = nicxlive::core::serde;

class ExParameterGroup : public Parameter {
protected:
    template<class Derived> static std::shared_ptr<Derived> allocateSourceReceiver() {
        using Allocator = nicxlive::core::d_runtime::SourceAllocator<Derived>;
        Allocator allocator;
        auto* storage = allocator.allocate(1);
        try {
            ::new (static_cast<void*>(storage)) Derived();
        } catch (...) {
            allocator.deallocate(storage, 1);
            throw;
        }
        return std::shared_ptr<Derived>(storage, [](Derived* object) noexcept {
            std::destroy_at(object);
            Allocator{}.deallocate(object, 1);
        }, allocator);
    }
    void constructSource() {}
    void constructSource(nicxlive::core::common::NullableString name) {
        Parameter::constructSource(std::move(name), false);
    }
    void constructSource(nicxlive::core::common::NullableString name, compat::Slice<ParameterPtr> children) {
        Parameter::constructSource(std::move(name), false);
        this->children = std::move(children);
    }
    void serializeSelf(serde::TypedJsonWriter& serializer) const override;
    void serializeSelf(serde::InochiSerializer& serializer) const override;

public:
    nicxlive::core::nodes::Vec3 color{0.15f, 0.15f, 0.15f};
    compat::Slice<ParameterPtr> children;

    ExParameterGroup() = default;
    // Allocate the actual most-derived receiver before the logical Source constructor.
    // Derived private native/logical constructors may grant this class friendship.
    template<class Derived = ExParameterGroup, class... Args> requires std::derived_from<Derived, ExParameterGroup>
    static std::shared_ptr<Derived> createSource(Args&&... args) {
        auto result = allocateSourceReceiver<Derived>();
        result->constructSource(std::forward<Args>(args)...);
        return result;
    }
    template<class Derived = ExParameterGroup, class... Producers>
        requires std::derived_from<Derived, ExParameterGroup>
    static std::shared_ptr<Derived> createSourceDeferred(Producers&&... producers) {
        auto result = allocateSourceReceiver<Derived>();
        // Braced argument evaluation is left-to-right, after the real owner exists.
        std::tuple<std::decay_t<std::invoke_result_t<Producers&&>>...> arguments{
            std::invoke(std::forward<Producers>(producers))...};
        std::apply([&](auto&&... args) {
            result->constructSource(std::forward<decltype(args)>(args)...);
        }, std::move(arguments));
        return result;
    }
    // Native compatibility constructors; Source argument expressions use createSourceDeferred.
    explicit ExParameterGroup(nicxlive::core::common::NullableString name)
        : Parameter(std::move(name), false) {}
    ExParameterGroup(nicxlive::core::common::NullableString name, compat::Slice<ParameterPtr> children)
        : Parameter(std::move(name), false), children(std::move(children)) {}
    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) override;
    serde::SerdeException deserializeFromFghj(const serde::Fghj& data) override;
    void reconstruct(const std::shared_ptr<Puppet>& puppet) override;
protected:
    void serializeSelf(serde::TypedJsonWriter& serializer) override;
};

class ExParameter : public Parameter {
protected:
    template<class Derived> static std::shared_ptr<Derived> allocateSourceReceiver() {
        using Allocator = nicxlive::core::d_runtime::SourceAllocator<Derived>;
        Allocator allocator;
        auto* storage = allocator.allocate(1);
        try {
            ::new (static_cast<void*>(storage)) Derived();
        } catch (...) {
            allocator.deallocate(storage, 1);
            throw;
        }
        return std::shared_ptr<Derived>(storage, [](Derived* object) noexcept {
            std::destroy_at(object);
            Allocator{}.deallocate(object, 1);
        }, allocator);
    }
    void constructSource() { parent.reset(); }
    void constructSource(nicxlive::core::common::NullableString name) {
        constructSource(std::move(name), false);
    }
    void constructSource(nicxlive::core::common::NullableString name, bool isVec2) {
        Parameter::constructSource(std::move(name), isVec2);
        parent.reset();
    }
    void constructSource(nicxlive::core::common::NullableString name, std::shared_ptr<ExParameterGroup> parent) {
        constructSource(std::move(name), false, std::move(parent));
    }
    void constructSource(nicxlive::core::common::NullableString name, bool isVec2,
                         std::shared_ptr<ExParameterGroup> parent) {
        Parameter::constructSource(std::move(name), isVec2);
        this->parent = std::move(parent);
    }
public:
    // The source class reference retains its actual parent object.
    std::shared_ptr<ExParameterGroup> parent;
    std::uint32_t parentUUID = std::numeric_limits<std::uint32_t>::max();

    ExParameter() = default;
    // Allocate the actual most-derived receiver before the logical Source constructor.
    // Derived private native/logical constructors may grant this class friendship.
    template<class Derived = ExParameter, class... Args> requires std::derived_from<Derived, ExParameter>
    static std::shared_ptr<Derived> createSource(Args&&... args) {
        auto result = allocateSourceReceiver<Derived>();
        result->constructSource(std::forward<Args>(args)...);
        return result;
    }
    template<class Derived = ExParameter, class... Producers>
        requires std::derived_from<Derived, ExParameter>
    static std::shared_ptr<Derived> createSourceDeferred(Producers&&... producers) {
        auto result = allocateSourceReceiver<Derived>();
        // Each Source argument finishes before evaluating the next producer.
        std::tuple<std::decay_t<std::invoke_result_t<Producers&&>>...> arguments{
            std::invoke(std::forward<Producers>(producers))...};
        std::apply([&](auto&&... args) {
            result->constructSource(std::forward<decltype(args)>(args)...);
        }, std::move(arguments));
        return result;
    }
    // Native compatibility constructors; Source argument expressions use createSourceDeferred.
    explicit ExParameter(nicxlive::core::common::NullableString name)
        : Parameter(std::move(name), false) {}
    ExParameter(nicxlive::core::common::NullableString name, bool isVec2)
        : Parameter(std::move(name), isVec2) {}
    ExParameter(nicxlive::core::common::NullableString name, std::shared_ptr<ExParameterGroup> parent)
        : Parameter(std::move(name), false), parent(std::move(parent)) {}
    ExParameter(nicxlive::core::common::NullableString name, bool isVec2,
        std::shared_ptr<ExParameterGroup> parent)
        : Parameter(std::move(name), isVec2), parent(std::move(parent)) {}

    serde::ReadError deserializeFromFghj(serde::TypedJsonView data) override;
    serde::SerdeException deserializeFromFghj(const serde::Fghj& data) override;
    void serializeSelf(serde::TypedJsonWriter& serializer) const override;
    void serializeSelf(serde::InochiSerializer& serializer) const override;
    virtual std::shared_ptr<ExParameterGroup> getParent() { return parent; }
    virtual void setParent(std::shared_ptr<ExParameterGroup> newParent);
    void finalize(const std::shared_ptr<Puppet>& puppet) override;
    ParameterPtr dup() override;
    void serializeSelf(serde::TypedJsonWriter& serializer) override;
};

void incRegisterExParameter();
} // namespace nijigenerate::ext
