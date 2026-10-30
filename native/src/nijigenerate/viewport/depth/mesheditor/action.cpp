// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "action.hpp"
#include "editor.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>

namespace nijigenerate::viewport::depth::mesheditor::action {
namespace {
void prepareDepthActionClassInfo() {
    static const bool registered = [] {
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        using nicxlive::core::d_runtime::SourceObjectTypeNames;
        ngRegisterObjectClassName(typeid(DepthOperationListChangeAction),
            compat::RetainedString::fromStaticLiteral(
                SourceObjectTypeNames<DepthOperationListChangeAction>::className));
        ngRegisterObjectClassName(typeid(DepthEditorDepthChangeAction),
            compat::RetainedString::fromStaticLiteral(
                SourceObjectTypeNames<DepthEditorDepthChangeAction>::className));
        return true;
    }();
    (void)registered;
}
}

compat::RetainedString DepthOperationListChangeAction::toString() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    prepareDepthActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
int DepthOperationListChangeAction::opCmp(actions::ActionPtr other) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    prepareDepthActionClassInfo();
    return actions::Action::opCmp(std::move(other));
}
compat::RetainedString DepthEditorDepthChangeAction::toString() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    prepareDepthActionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
int DepthEditorDepthChangeAction::opCmp(actions::ActionPtr other) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    prepareDepthActionClassInfo();
    return actions::Action::opCmp(std::move(other));
}

DepthOperationListChangeAction::DepthOperationListChangeAction(std::shared_ptr<Editor> receiver, EditorRef value) {
    initializeSource(std::move(receiver), std::move(value));
}
void DepthOperationListChangeAction::initializeSource(std::shared_ptr<Editor> receiver, EditorRef value) {
    editor = std::move(receiver);
    target = std::move(value);
    oldOperations = compat::ngRequireObject(editor)->copyOperations(target);
}
std::shared_ptr<DepthOperationListChangeAction::Editor>
DepthOperationListChangeAction::retainedEditor() const {
    return editor ? editor : pendingEditor.lock();
}
std::shared_ptr<DepthOperationListChangeAction> DepthOperationListChangeAction::createPending(
    std::shared_ptr<Editor> receiver, EditorRef target) {
    auto action = create(receiver, std::move(target));
    action->pendingEditor = receiver;
    action->editor.reset();
    return action;
}
std::shared_ptr<DepthOperationListChangeAction> DepthOperationListChangeAction::retainForPublication(
    std::shared_ptr<DepthOperationListChangeAction> action) {
    if (!action || action->editor) return action;
    struct PublicationOwner {
        // Release the editor while the actual action storage is still alive.
        std::shared_ptr<DepthOperationListChangeAction> action;
        std::shared_ptr<Editor> editor;
    };
    const auto editor = action->retainedEditor();
    std::shared_ptr<PublicationOwner> owner;
    try {
        owner = std::make_shared<PublicationOwner>(PublicationOwner{action, editor});
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return std::shared_ptr<DepthOperationListChangeAction>(std::move(owner), action.get());
}
void DepthOperationListChangeAction::updateNewState() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    newOperations = compat::ngRequireObject(retainedEditor())->copyOperations(target);
}
void DepthOperationListChangeAction::rollback() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(retainedEditor())->replaceOperations(target, oldOperations);
}
void DepthOperationListChangeAction::redo() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(retainedEditor())->replaceOperations(target, newOperations);
}
actions::ActionText DepthOperationListChangeAction::describe() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Changed depth operations"));
}
actions::ActionText DepthOperationListChangeAction::describeUndo() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Depth operations were changed"));
}
actions::ActionText DepthOperationListChangeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}
DepthEditorDepthChangeAction::DepthEditorDepthChangeAction(std::shared_ptr<Editor> receiver, EditorRef value) {
    initializeSource(std::move(receiver), std::move(value));
}
void DepthEditorDepthChangeAction::initializeSource(std::shared_ptr<Editor> receiver, EditorRef value) {
    editor = std::move(receiver);
    target = std::move(value);
    oldDepths = compat::ngRequireObject(target)->copyEditorDepths();
    oldBaseDepths = compat::ngRequireObject(target)->baseDepths.duplicate();
    oldOperations = compat::ngRequireObject(editor)->copyOperations(target);
    oldDirectDepthDirty = compat::ngRequireObject(editor)->isDirectDepthDirty(target);
}
void DepthEditorDepthChangeAction::updateNewState() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    newDepths = compat::ngRequireObject(target)->copyEditorDepths();
    newBaseDepths = compat::ngRequireObject(target)->baseDepths.duplicate();
    newOperations = compat::ngRequireObject(editor)->copyOperations(target);
    newDirectDepthDirty = compat::ngRequireObject(editor)->isDirectDepthDirty(target);
}
void DepthEditorDepthChangeAction::rollback() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(editor)->replaceDepthState(target, oldDepths, oldBaseDepths,
        oldOperations, oldDirectDepthDirty);
}
void DepthEditorDepthChangeAction::redo() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(editor)->replaceDepthState(target, newDepths, newBaseDepths,
        newOperations, newDirectDepthDirty);
}
actions::ActionText DepthEditorDepthChangeAction::describe() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Changed vertex depths"));
}
actions::ActionText DepthEditorDepthChangeAction::describeUndo() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Vertex depths were changed"));
}
actions::ActionText DepthEditorDepthChangeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}
}
