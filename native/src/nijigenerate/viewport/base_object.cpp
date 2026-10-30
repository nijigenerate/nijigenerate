// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
#include "base.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <string>
#include <utility>
#include <new>
#include <stdexcept>
#include <source_location>

namespace nijigenerate::viewport::base {
namespace {
compat::RetainedString viewportComparisonMessage(const compat::RetainedString& name) {
    try {
        std::string message = "need opCmp for class ";
        message += name.view();
        return compat::RetainedString(std::move(message));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
void Viewport::prepareSourceClassInfo() {
    registerSourceClassInfo<Viewport>("nijigenerate.viewport.base.Viewport");
}

void DelegationViewport::prepareSourceClassInfo() {
    Viewport::prepareSourceClassInfo();
    registerSourceClassInfo<DelegationViewport>("nijigenerate.viewport.base.DelegationViewport");
}

compat::RetainedString Viewport::toString() {
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

std::size_t Viewport::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}

bool Viewport::opEquals(std::shared_ptr<Viewport> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}

int Viewport::opCmp(std::shared_ptr<Viewport>) {
    prepareSourceClassInfo();
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = viewportComparisonMessage(name);
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

bool ngViewportObjectEquals(std::shared_ptr<Viewport> left, std::shared_ptr<Viewport> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right));
}
} // namespace nijigenerate::viewport::base
