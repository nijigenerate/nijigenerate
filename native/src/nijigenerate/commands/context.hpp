#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/panels/inspector/common.hpp>
#include <math/types.hpp>
#include <nodes/node.hpp>
#include <param/binding.hpp>
#include <param/parameter.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <puppet.hpp>
#include <bit>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::commands {

class Context : public std::enable_shared_from_this<Context> {
public:
    using PuppetRef = std::shared_ptr<nicxlive::core::Puppet>;
    using Nodes = compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>;
    using Parameters = compat::Slice<std::shared_ptr<nicxlive::core::param::Parameter>>;
    using Bindings = compat::Slice<std::shared_ptr<nicxlive::core::param::ParameterBinding>>;
    using KeyPoint = compat::Vec2u;
    using ParameterValue = nicxlive::core::math::Vec2;
    using Inspectors = compat::Slice<std::shared_ptr<panels::inspector::TypedInspector<nicxlive::core::nodes::Node>>>;

    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    PuppetRef _puppet;
    Nodes _nodes;
    Parameters _parameters;
    Parameters _armedParameters;
    Bindings _bindings;
    Bindings _activeBindings;
    KeyPoint _keyPoint{};
    ParameterValue _parameterValue{std::numeric_limits<float>::quiet_NaN(),
                                   std::numeric_limits<float>::quiet_NaN()};
    Inspectors _inspectors;

    enum class ContextMask : std::int32_t {
        None = 0,
        HasPuppet = 1,
        HasNodes = 2,
        HasParameters = 4,
        HasBindings = 8,
        HasKeyPoint = 16,
        HasInspectors = 32,
        HasArmedParameters = 64,
        HasActiveBindings = 128,
        HasExplicitKeyPoint = 256,
        HasParameterValue = 512,
    };
    ContextMask masks = ContextMask::None;

    Context();
    virtual ~Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<Context> other);
    virtual int opCmp(std::shared_ptr<Context> other);

    virtual bool hasPuppet() { return flag(ContextMask::HasPuppet); }
    virtual bool hasNodes() { return flag(ContextMask::HasNodes); }
    virtual bool hasParameters() { return flag(ContextMask::HasParameters); }
    virtual bool hasArmedParameters() { return flag(ContextMask::HasArmedParameters); }
    virtual bool hasBindings() { return flag(ContextMask::HasBindings); }
    virtual bool hasKeyPoint() { return flag(ContextMask::HasKeyPoint); }
    virtual bool hasInspectors() { return flag(ContextMask::HasInspectors); }
    virtual bool hasActiveBindings() { return flag(ContextMask::HasActiveBindings); }
    virtual bool hasExplicitKeyPoint() { return flag(ContextMask::HasExplicitKeyPoint); }
    virtual bool hasParameterValue() { return flag(ContextMask::HasParameterValue); }

    virtual void hasPuppet(bool value) { flag(ContextMask::HasPuppet, value); }
    virtual void hasNodes(bool value) { flag(ContextMask::HasNodes, value); }
    virtual void hasParameters(bool value) { flag(ContextMask::HasParameters, value); }
    virtual void hasArmedParameters(bool value) { flag(ContextMask::HasArmedParameters, value); }
    virtual void hasBindings(bool value) { flag(ContextMask::HasBindings, value); }
    virtual void hasKeyPoint(bool value) { flag(ContextMask::HasKeyPoint, value); }
    virtual void hasInspectors(bool value) { flag(ContextMask::HasInspectors, value); }
    virtual void hasActiveBindings(bool value) { flag(ContextMask::HasActiveBindings, value); }
    virtual void hasExplicitKeyPoint(bool value) { flag(ContextMask::HasExplicitKeyPoint, value); }
    virtual void hasParameterValue(bool value) { flag(ContextMask::HasParameterValue, value); }

    virtual PuppetRef puppet() { return _puppet; }
    virtual void puppet(PuppetRef value) { _puppet = std::move(value); hasPuppet(true); }
    virtual Nodes nodes() { return _nodes; }
    virtual void nodes(Nodes value) { _nodes = std::move(value); hasNodes(true); }
    virtual Parameters parameters() { return _parameters; }
    virtual void parameters(Parameters value) { _parameters = std::move(value); hasParameters(true); }
    virtual Parameters armedParameters() { return _armedParameters; }
    virtual void armedParameters(Parameters value) { _armedParameters = std::move(value); hasArmedParameters(true); }
    virtual Bindings bindings() { return _bindings; }
    virtual void bindings(Bindings value) { _bindings = std::move(value); hasBindings(true); }
    virtual KeyPoint keyPoint() { return _keyPoint; }
    virtual void keyPoint(KeyPoint value) { _keyPoint = value; hasKeyPoint(true); }
    virtual ParameterValue parameterValue() { return _parameterValue; }
    virtual void parameterValue(ParameterValue value) { _parameterValue = value; hasParameterValue(true); }
    virtual Inspectors inspectors() { return _inspectors; }
    virtual void inspectors(Inspectors value) { _inspectors = std::move(value); hasInspectors(true); }
    virtual Bindings activeBindings() { return _activeBindings; }
    virtual void activeBindings(Bindings value) { _activeBindings = std::move(value); hasActiveBindings(true); }

protected:
    virtual void prepareSourceClassInfo();

private:
    bool flag(ContextMask bit) const {
        return (static_cast<std::uint32_t>(static_cast<std::int32_t>(masks)) &
                static_cast<std::uint32_t>(static_cast<std::int32_t>(bit))) != 0;
    }
    void flag(ContextMask bit, bool value) {
        const auto current = static_cast<std::uint32_t>(static_cast<std::int32_t>(masks));
        const auto selected = static_cast<std::uint32_t>(static_cast<std::int32_t>(bit));
        const auto updated = value ? (current | selected) : (current & ~selected);
        masks = static_cast<ContextMask>(std::bit_cast<std::int32_t>(updated));
    }
};

} // namespace nijigenerate::commands

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::commands::Context> {
    inline static constexpr char className[] = "nijigenerate.commands.base.Context";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.base.Context)";
};
template<> struct SourceTypeSpellings<::nijigenerate::commands::Context> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.base.Context)";
    inline static constexpr char unqualifiedArgument[] = "(Context)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.base.Context";
    inline static constexpr char unqualifiedElement[] = "Context";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {
inline Context::Context() {
    // Make the defining ClassInfo available to qualified formatting too.
    Context::prepareSourceClassInfo();
}

// Source Object defaults, object.d (Digital Mars 2000-2011), BSL-1.0.
// Retained notice: nicxlive/licenses/druntime-associative-array.txt.
inline void Context::prepareSourceClassInfo() {
    using namespace nicxlive::core::d_runtime;
    try {
        ngPrepareSourceTypeSpellings<Context>();
        (void)ngObjectTemplateArgumentName<Context>();
        ngRegisterObjectTypeNames(typeid(Context),
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<Context>::className),
            compat::RetainedString::fromStaticLiteral(SourceObjectTypeNames<Context>::templateArgumentName));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::RetainedString Context::toString() {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
inline std::size_t Context::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
inline bool Context::opEquals(std::shared_ptr<Context> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
inline int Context::opCmp(std::shared_ptr<Context>) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = [&] {
        try {
            std::string bytes = "need opCmp for class ";
            bytes += name.view();
            return compat::RetainedString(std::move(bytes));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

} // namespace nijigenerate::commands

#include <nijigenerate/core/typed_format.hpp>

namespace nijigenerate::core::translation {
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::commands::Context>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::commands::Context>> {};
}
