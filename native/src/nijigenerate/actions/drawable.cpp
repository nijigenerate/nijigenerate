/*
    Copyright © 2020-2023,2022 Inochi2D Project
    Copyright © 2024 nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
#include "drawable.hpp"
#include "binding_add_remove_action.hpp"
#include "nijigenerate/core/translation.hpp"
#include "nijigenerate/core/typed_format.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"
#include <nodes/mesh_group.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::actions::drawable {
using nicxlive::core::nodes::MeshData;
using nicxlive::core::nodes::Drawable;

namespace {
ActionText retainFormatted(std::string text) {
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

void prepareDrawableActionClassInfo() {
    static const bool registered = [] {
        using namespace nicxlive::core::d_runtime;
        using Names=SourceObjectTypeNames<DrawableChangeAction>;
        ngPrepareSourceTypeSpellings<DrawableChangeAction>();
        ngRegisterObjectTypeNames<DrawableChangeAction>(
            compat::RetainedString::fromStaticLiteral(Names::className),
            compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DrawableChangeAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.drawable.DrawableChangeAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool drawableActionSourceClassInfo = [] {
    prepareDrawableActionClassInfo();
    return true;
}();
}

// Object.toString, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void DrawableChangeAction::prepareSourceClassInfo() {
    prepareDrawableActionClassInfo();
}
compat::RetainedString DrawableChangeAction::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}

void DrawableChangeAction::copy(MeshData& source, MeshData& destination) {
    destination.vertices = source.vertices.duplicate();
    destination.uvs = source.uvs.duplicate();
    destination.indices = source.indices.duplicate();
    destination.origin = source.origin;
    // The D helper intentionally leaves every other destination field untouched.
}

DrawableChangeAction::DrawableChangeAction(SourceAllocationTag) : GroupAction() {}

DrawableChangeAction::DrawableChangeAction(ActionText value,
        std::shared_ptr<nicxlive::core::nodes::Drawable> target) : DrawableChangeAction(SourceAllocationTag{}) {
    initializeSource(std::move(value), std::move(target));
}

void DrawableChangeAction::initializeSource(ActionText value,
        std::shared_ptr<nicxlive::core::nodes::Drawable> target) {
    name = std::move(value);
    self = std::move(target);
    undoable = true;
    const auto receiver = compat::ngRequireObject(self);
    copy(receiver->Drawable::getMesh(), mesh);
}

void DrawableChangeAction::updateNewState() {}
void DrawableChangeAction::clear() {}

void DrawableChangeAction::addBinding(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
        std::shared_ptr<nicxlive::core::param::ParameterBinding> bindingValue) {
    const auto keepAlive = shared_from_this();
    addAction(binding::ngCreateBindingAddRemoveAction<binding::ParameterBindingAddAction>(
        parameter, bindingValue));
}

void DrawableChangeAction::rollback() {
    const auto keepAlive = shared_from_this();
    if (undoable) {
        MeshData temporary;
        {
            const auto receiver = compat::ngRequireObject(self);
            copy(receiver->Drawable::getMesh(), temporary);
        }
        {
            // A virtual runtime call may replace the public self handle.
            // Retain its evaluated receiver until that call has returned.
            const auto receiver = compat::ngRequireObject(self);
            receiver->rebuffer(mesh);
        }
        if (auto group = std::dynamic_pointer_cast<nicxlive::core::nodes::MeshGroup>(self)) group->clearCache();
        mesh = std::move(temporary);
        undoable = false;
    }
    GroupAction::rollback();
}

void DrawableChangeAction::redo() {
    const auto keepAlive = shared_from_this();
    if (!undoable) {
        MeshData temporary;
        {
            const auto receiver = compat::ngRequireObject(self);
            copy(receiver->Drawable::getMesh(), temporary);
        }
        {
            const auto receiver = compat::ngRequireObject(self);
            receiver->rebuffer(mesh);
        }
        if (auto group = std::dynamic_pointer_cast<nicxlive::core::nodes::MeshGroup>(self)) group->clearCache();
        mesh = std::move(temporary);
        undoable = true;
    }
    GroupAction::redo();
}

ActionText DrawableChangeAction::describe() {
    const auto keepAlive = shared_from_this();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Changed drawable mesh of %s"));
    const auto receiver = compat::ngRequireObject(self);
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(receiver->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText DrawableChangeAction::describeUndo() {
    const auto keepAlive = shared_from_this();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Drawable %s was changed"));
    const auto receiver = compat::ngRequireObject(self);
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(receiver->getName())};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}

ActionText DrawableChangeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}
bool DrawableChangeAction::merge(ActionPtr) { return false; }
bool DrawableChangeAction::canMerge(ActionPtr) { return false; }
} // namespace nijigenerate::actions::drawable
