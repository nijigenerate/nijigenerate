#include "base.hpp"
#include <nijigenerate/actions/mesheditor.hpp>

namespace nijigenerate::viewport::common::mesheditor::tools::base {
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
void Tool::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Tool),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.viewport.common.mesheditor.tools.base.Tool"));
        return true;
    }();
    (void)registered;
}
compat::RetainedString Tool::toString() {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t Tool::toHash() {
    const auto keepAlive = weak_from_this().lock();
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool Tool::opEquals(std::shared_ptr<Tool> other) {
    const auto keepAlive = weak_from_this().lock();
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
void detail::throwObjectComparison(const std::type_info& type) {
    const auto name = nicxlive::core::d_runtime::ngObjectClassName(type);
    const auto message = [&] {
        try {
            std::string text = "need opCmp for class ";
            text += name.view();
            return compat::RetainedString(std::move(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    throw compat::ThrowableException(message,
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
int Tool::opCmp(std::shared_ptr<Tool>) {
    const auto keepAlive = shared_from_this();
    prepareSourceClassInfo();
    detail::throwObjectComparison(typeid(*this));
}
MeshEditorActionRef<DeformationAction> Tool::editorAction(NodeRef target, std::shared_ptr<DeformationAction> action) {
    const auto keepAlive = shared_from_this();
    return actions::mesheditor::MeshEditorAction<DeformationAction>::create(std::move(target), std::move(action));
}
MeshEditorActionRef<actions::GroupAction> Tool::editorAction(NodeRef target, std::shared_ptr<actions::GroupAction> action) {
    const auto keepAlive = shared_from_this();
    return actions::mesheditor::MeshEditorAction<actions::GroupAction>::create(std::move(target), std::move(action));
}
MeshEditorActionRef<DeformationAction> Tool::editorAction(DrawableRef target, std::shared_ptr<DeformationAction> action) {
    const auto keepAlive = shared_from_this();
    return actions::mesheditor::MeshEditorAction<DeformationAction>::create(std::move(target), std::move(action));
}
MeshEditorActionRef<actions::GroupAction> Tool::editorAction(DrawableRef target, std::shared_ptr<actions::GroupAction> action) {
    const auto keepAlive = shared_from_this();
    return actions::mesheditor::MeshEditorAction<actions::GroupAction>::create(std::move(target), std::move(action));
}
}
