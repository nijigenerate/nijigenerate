// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
#include "package.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <string>
#include <new>
#include <source_location>
#include <stdexcept>
#include <typeinfo>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor {
void IncMeshEditor::prepareSourceClassInfo() {
    registerSourceClassInfo<IncMeshEditor>("nijigenerate.viewport.common.mesheditor.IncMeshEditor");
}

compat::RetainedString IncMeshEditor::toString() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

std::size_t IncMeshEditor::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}

bool IncMeshEditor::opEquals(std::shared_ptr<IncMeshEditor> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}

int IncMeshEditor::opCmp(std::shared_ptr<IncMeshEditor>) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
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
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

bool ngIncMeshEditorObjectEquals(std::shared_ptr<IncMeshEditor> left,
    std::shared_ptr<IncMeshEditor> right) {
    return nicxlive::core::common::ngClassObjectEquals(
        std::move(left), std::move(right));
}
} // namespace nijigenerate::viewport::common::mesheditor
