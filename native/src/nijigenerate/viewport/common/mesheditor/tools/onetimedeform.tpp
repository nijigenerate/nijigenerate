#pragma once
// Definitions use the completed real Scope and the same production providers.
#include "onetimedeform_scope.hpp"
#include "grid.hpp"
#include "package.hpp"
#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/model/mesheditor/deformable.hpp>
#include <nijigenerate/viewport/vertex/mesheditor/deformable.hpp>
#include <algorithm>

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform {
namespace template_detail {
using detail::BindingRef;
using detail::ParameterRef;
using ID = OneTimeDeformBase::OneTimeDeformActionID;
constexpr std::int32_t value(ID id) { return static_cast<std::int32_t>(id); }
inline detail::Vec2u closest(ParameterRef parameter) { return compat::ngRequireObject(parameter)->findClosestKeypoint(); }
inline detail::BindingRef binding(std::shared_ptr<nicxlive::core::param::ParameterBinding> value) {
    return std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(value);
}
template<class Values> bool contains(Values values, NodeRef value) {
    for (auto candidate : values)
        if (nicxlive::core::ngResourceObjectEquals(candidate, value)) return true;
    return false;
}
template<std::size_t N> compat::RetainedString translated(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
inline bool updateShared(std::shared_ptr<detail::OneTimeDeformScope> session, SubToolMode mode,
    std::int32_t action, ImGuiIO* io, bool& changed) {
    changed = false;
    const auto frame = ImGui::GetFrameCount();
    if (session->cachedUpdateFrame == frame && session->cachedUpdateMode == mode && session->cachedUpdateAction == action) {
        changed = session->cachedUpdateChanged; return session->cachedUpdateResult;
    }
    bool localChanged = false;
    const auto tool = mode == SubToolMode::Vertex ? compat::ngRequireObject(session->vertImpl)->getTool() :
        compat::ngRequireObject(session->defImpl)->getTool();
    const bool result = compat::ngRequireObject(tool)->update(io,
        mode == SubToolMode::Vertex ? session->vertImpl : session->defImpl, action, localChanged);
    if (mode == SubToolMode::Vertex && session->vertImpl)
        compat::ngRequireObject(session->vertImpl)->updateSelectionPreview(io);
    else if (mode == SubToolMode::Deform && session->defImpl)
        compat::ngRequireObject(session->defImpl)->updateSelectionPreview(io);
    session->cachedUpdateFrame = frame; session->cachedUpdateMode = mode; session->cachedUpdateAction = action;
    session->cachedUpdateResult = result; session->cachedUpdateChanged = localChanged; changed = localChanged;
    return result;
}
inline std::int32_t peekShared(std::shared_ptr<detail::OneTimeDeformScope> session, SubToolMode mode, ImGuiIO* io) {
    const auto frame = ImGui::GetFrameCount();
    if (session->cachedPeekFrame == frame && session->cachedPeekMode == mode) return session->cachedPeekAction;
    const auto tool = mode == SubToolMode::Vertex ? compat::ngRequireObject(session->vertImpl)->getTool() :
        compat::ngRequireObject(session->defImpl)->getTool();
    auto action = compat::ngRequireObject(tool)->peek(io,
        mode == SubToolMode::Vertex ? session->vertImpl : session->defImpl);
    session->cachedPeekFrame = frame; session->cachedPeekMode = mode; session->cachedPeekAction = action;
    return action;
}
}
namespace detail {
template<class T> bool OneTimeDeformScope::initialize(NodeRef target, NodeRef current) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = shared_from_this();
    if (!filter || !std::dynamic_pointer_cast<T>(filter)) {
        if (filter) forceFinalize(current);
        auto created = OneTimeDeformer<T>::createSource([&] {
            return std::tuple{compat::ngRequireObject(incActivePuppet())->root};
        });
        filter = created;
        using VertexEditor = viewport::vertex::mesheditor::deformable::IncMeshEditorOneFor<
            T, project::EditMode::VertexEdit>;
        vertImpl = std::allocate_shared<VertexEditor>(
            nicxlive::core::d_runtime::SourceAllocator<VertexEditor>{});
        if (!vertImpl) compat::ngFailNullAccess(true);
        vertImpl->vertexColor = {0, 1, 1, 1};
        vertImpl->edgeColor = {0, 1, 1, 1};
        const auto vertexEditor = compat::ngRequireObject(vertImpl);
        vertexEditor->setTarget(std::dynamic_pointer_cast<T>(filter));
        compat::ngRequireObject(vertImpl)->addFilterTarget(target);
        vertActionId = 0;
        using ModelEditor = viewport::model::mesheditor::deformable::IncMeshEditorOneFor<
            T, project::EditMode::ModelEdit>;
        defImpl = std::allocate_shared<ModelEditor>(
            nicxlive::core::d_runtime::SourceAllocator<ModelEditor>{});
        if (!defImpl) compat::ngFailNullAccess(true);
        defImpl->vertexColor = {0, 1, 0, 1};
        defImpl->edgeColor = {0, 1, 0, 1};
        const auto modelEditor = compat::ngRequireObject(defImpl);
        modelEditor->setTarget(std::dynamic_pointer_cast<T>(filter));
        compat::ngRequireObject(defImpl)->addFilterTarget(target);
        defActionId = 0;
        setup<T>(); currentMode = SubToolMode::Vertex; initialTargetDeformations = {}; invalidatePeekCache();
        const auto filterKey = filter;
        gFilterEditors[filterKey] = defImpl;
        return true;
    }
    return false;
}
template<class T> void OneTimeDeformScope::setup() {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = shared_from_this();
    if constexpr (std::same_as<T, GridDeformer>) {
        compat::ngRequireObject(vertImpl)->setToolMode(base::VertexToolMode::Grid);
        if (auto tool = std::dynamic_pointer_cast<grid::GridTool>(compat::ngRequireObject(vertImpl)->getTool()))
            tool->resetVirtualMeshAsEmpty(vertImpl);
        compat::ngRequireObject(defImpl)->setToolMode(base::VertexToolMode::Points);
    } else {
        compat::ngRequireObject(vertImpl)->setToolMode(base::VertexToolMode::BezierDeform);
        compat::ngRequireObject(defImpl)->setToolMode(base::VertexToolMode::BezierDeform);
    }
}
template<class T> void OneTimeDeformScope::enterSubTool(SubToolMode next) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = shared_from_this();
    if (!active()) return;
    currentMode = next; invalidatePeekCache();
    switch (next) {
    case SubToolMode::Select: break;
    case SubToolMode::Vertex:
        if (!vertImpl) compat::ngFailNullAccess(true);
        vertImpl->vertexColor = {0, 1, 1, 1}; vertImpl->edgeColor = {0, 1, 1, 1};
        if (!defImpl) compat::ngFailNullAccess(true);
        defImpl->vertexColor = {0, 0.6f, 0, 1}; defImpl->edgeColor = {0, 0.6f, 0, 1};
        resetCurrentTool(vertImpl);
        compat::ngRequireObject(vertImpl)->getCleanDeformAction(); break;
    case SubToolMode::Deform:
        if (!vertImpl) compat::ngFailNullAccess(true);
        vertImpl->vertexColor = {0, 0.6f, 0.6f, 1}; vertImpl->edgeColor = {0, 0.6f, 0.6f, 1};
        if (!defImpl) compat::ngFailNullAccess(true);
        defImpl->vertexColor = {0, 1, 0, 1}; defImpl->edgeColor = {0, 1, 0, 1};
        resetCurrentTool(defImpl);
        compat::ngRequireObject(defImpl)->setTarget(std::dynamic_pointer_cast<T>(filter));
        compat::ngRequireObject(defImpl)->getCleanDeformAction(); seedViewFromBinding(); break;
    default:
        throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
            "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 543);
    }
}
template<class T> void OneTimeDeformScope::ensureSubToolReady(SubToolMode next) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    if (active() && currentMode != next) enterSubTool<T>(next);
}
}
template<class T> bool OneTimeDeform<T>::subToolIsDragging() {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    if (!acquired) return false;
    auto session = ngActiveOneTimeDeformScope(); if (!session) return false;
    EditorRef editor;
    switch (mode) {
    case SubToolMode::Select: return false;
    case SubToolMode::Vertex: editor = session->vertImpl; break;
    case SubToolMode::Deform: editor = session->defImpl; break;
    default:
        throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
            "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 798);
    }
    if (!editor) return false;
    auto tool = std::dynamic_pointer_cast<select::NodeSelect>(compat::ngRequireObject(editor)->getTool());
    return compat::ngRequireObject(editor)->isSelecting || (tool && tool->isDragging);
}
template<class T> void OneTimeDeform<T>::switchSubTool(SubToolMode next) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    if (mode == next) return;
    auto session = ngOneTimeDeformScope(); session->leaveSubTool(mode); mode = next;
    session->enterSubTool<T>(next); appliedMode = next;
}
template<class T> bool OneTimeDeform<T>::onDragStart(base::Vec2 mouse, EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    const auto dragMode = mode;
    if (dragMode == SubToolMode::Select) return NodeSelect::onDragStart(mouse, editor);
    if (acquired && (dragMode == SubToolMode::Vertex || dragMode == SubToolMode::Deform)) {
        auto nested = dragMode == SubToolMode::Vertex ? ngOneTimeDeformScope()->vertImpl : ngOneTimeDeformScope()->defImpl;
        if (auto tool = std::dynamic_pointer_cast<base::Draggable>(compat::ngRequireObject(nested)->getTool())) {
            nested = dragMode == SubToolMode::Vertex ? ngOneTimeDeformScope()->vertImpl : ngOneTimeDeformScope()->defImpl;
            return tool->onDragStart(mouse, nested);
        }
    }
    return false;
}
template<class T> bool OneTimeDeform<T>::onDragUpdate(base::Vec2 mouse, EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    const auto dragMode = mode;
    if (dragMode == SubToolMode::Select) return NodeSelect::onDragUpdate(mouse, editor);
    if (acquired && (dragMode == SubToolMode::Vertex || dragMode == SubToolMode::Deform)) {
        auto nested = dragMode == SubToolMode::Vertex ? ngOneTimeDeformScope()->vertImpl : ngOneTimeDeformScope()->defImpl;
        if (auto tool = std::dynamic_pointer_cast<base::Draggable>(compat::ngRequireObject(nested)->getTool())) {
            nested = dragMode == SubToolMode::Vertex ? ngOneTimeDeformScope()->vertImpl : ngOneTimeDeformScope()->defImpl;
            return tool->onDragUpdate(mouse, nested);
        }
    }
    return false;
}
template<class T> bool OneTimeDeform<T>::onDragEnd(base::Vec2 mouse, EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    const auto dragMode = mode;
    if (dragMode == SubToolMode::Select) return NodeSelect::onDragEnd(mouse, editor);
    if (acquired && (dragMode == SubToolMode::Vertex || dragMode == SubToolMode::Deform)) {
        auto nested = dragMode == SubToolMode::Vertex ? ngOneTimeDeformScope()->vertImpl : ngOneTimeDeformScope()->defImpl;
        if (auto tool = std::dynamic_pointer_cast<base::Draggable>(compat::ngRequireObject(nested)->getTool())) {
            nested = dragMode == SubToolMode::Vertex ? ngOneTimeDeformScope()->vertImpl : ngOneTimeDeformScope()->defImpl;
            return tool->onDragEnd(mouse, nested);
        }
    }
    return false;
}
template<class T> void OneTimeDeform<T>::setToolMode(base::VertexToolMode toolMode, EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    NodeSelect::setToolMode(toolMode, editor); selectingTargetVertices = false; subToolGestureActive = false;
    ngEnsureOneTimeDeformCallbacks(); auto session = ngOneTimeDeformScope();
    session->initialize<T>(compat::ngRequireObject(editor)->getTarget()); acquired = session->active();
    if (session->vertImpl) {
        const auto targets = compat::ngRequireObject(session->vertImpl)->getFilterTargets();
        const auto target = compat::ngRequireObject(editor)->getTarget();
        if (!contains(targets, target))
            compat::ngRequireObject(session->vertImpl)->addFilterTarget(compat::ngRequireObject(editor)->getTarget());
    }
    if (session->defImpl) {
        const auto targets = compat::ngRequireObject(session->defImpl)->getFilterTargets();
        const auto target = compat::ngRequireObject(editor)->getTarget();
        if (!contains(targets, target))
            compat::ngRequireObject(session->defImpl)->addFilterTarget(compat::ngRequireObject(editor)->getTarget());
    }
    const auto children = compat::ngRequireObject(std::dynamic_pointer_cast<T>(session->filter))->children;
    const auto childTarget = compat::ngRequireObject(editor)->getTarget();
    if (!contains(children, childTarget)) {
        if (auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(
                compat::ngRequireObject(editor)->getTarget()))
            session->initialTargetDeformations[target] = target->deformation.dup();
        compat::ngRequireObject(session->filter)->captureTarget(compat::ngRequireObject(editor)->getTarget());
        if (session->defImpl) {
            compat::ngRequireObject(session->defImpl)->setTarget(std::dynamic_pointer_cast<T>(session->filter));
            compat::ngRequireObject(session->defImpl)->setToolMode(
                compat::ngRequireObject(session->defImpl)->getToolMode());
        }
    }
    const auto selectionFilter = compat::ngRequireObject(std::dynamic_pointer_cast<OneTimeDeformer<T>>(session->filter));
    const auto selectedTarget = compat::ngRequireObject(editor)->getTarget();
    const auto selected = compat::ngRequireObject(editor)->selected;
    const auto mirrored = compat::ngRequireObject(editor)->mirrorSelected;
    selectionFilter->setSelectedVertices(selectedTarget, selected, mirrored);
    compat::ngRequireObject(editor)->getDeformAction(); session->begin(); mode = SubToolMode::Vertex; appliedMode = mode;
}
template<class T> void OneTimeDeform<T>::finalizeToolMode(EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    if (!acquired) return;
    auto session = ngOneTimeDeformScope(); session->leaveSubTool(mode, bool(session->actionScope)); session->closeActionScope();
    auto parameter = incArmedParameter();
    if (session->filter && parameter && session->hasTargetChanges()) {
        auto point = closest(parameter);
        actions::parameter::Bindings bindings, addedBindings;
        const auto children = compat::ngRequireObject(session->filterNode())->children;
        for (auto child : children) if (auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(child)) {
            auto current = parameter->getBinding(target,
                nicxlive::core::common::NullableString::fromStaticLiteral("deform"));
            if (!current) {
                current = parameter->getOrAddBinding(target,
                    nicxlive::core::common::NullableString::fromStaticLiteral("deform"));
                if (current) addedBindings.push_back(current);
            }
            if (current) bindings.push_back(current);
        }
        if (!bindings.empty()) {
            auto persist = actions::parameter::ngCreateParameterBindingsActionDeferred<
                actions::parameter::ParameterChangeBindingsValueAction>([&] {
                return std::tuple{translated("apply deform"), parameter, bindings, point.x, point.y};
            });
            session->applyToTargetsWithFitting(parameter, point); persist->updateNewState();
            if (!addedBindings.empty()) {
                auto group = std::allocate_shared<actions::GroupAction>(
                    nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
                for (auto added : addedBindings) group->addAction(
                    actions::binding::ngCreateBindingAddRemoveAction<actions::binding::ParameterBindingAddAction>(parameter, added));
                group->addAction(persist); incActionPush(group);
            } else incActionPush(persist);
        } else {
            const auto release = compat::ngRequireObject(session->filterNode())->children.duplicate();
            for (auto child : release) compat::ngRequireObject(session->filter)->releaseTarget(child);
            if (auto current = parameter->getBinding(session->filterNode(),
                    nicxlive::core::common::NullableString::fromStaticLiteral("deform")))
                parameter->removeBinding(current);
        }
        session->cleanupTarget(editor, true); viewport::model::deform::ngViewportNodeDeformNotifyParamValueChanged();
    } else if (session->filter) {
        if (parameter) if (auto current = parameter->getBinding(session->filterNode(),
                nicxlive::core::common::NullableString::fromStaticLiteral("deform")))
            parameter->removeBinding(current);
        session->cleanupTarget(editor, true);
        if (parameter) viewport::model::deform::ngViewportNodeDeformNotifyParamValueChanged();
    }
    compat::ngRequireObject(editor)->pushDeformAction();
}
template<class T> void OneTimeDeform<T>::abortToolMode(EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    if (auto session = ngActiveOneTimeDeformScope()) session->cleanupTarget(editor, acquired);
}
template<class T> std::int32_t OneTimeDeform<T>::peek(ImGuiIO* io, EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    if (core::input::ngInputIsKeyPressed(ImGuiKey_Tab) && !subToolGestureActive && !selectingTargetVertices)
        return value(ID::SwitchMode);
    const bool released = core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left);
    struct Release { bool released; bool& active; ~Release() { if (released) active = false; } } release{released, subToolGestureActive};
    if (subToolGestureActive && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !released) subToolGestureActive = false;
    if (!subToolGestureActive && !compat::ngRequireObject(io)->KeyAlt &&
        !selectingTargetVertices && mode != SubToolMode::Select &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) subToolGestureActive = true;
    if (!subToolGestureActive && !subToolIsDragging() && (compat::ngRequireObject(io)->KeyAlt ||
            selectingTargetVertices || compat::ngRequireObject(editor)->isSelecting)) {
        NodeSelect::peek(io, editor);
        if (released && (selectingTargetVertices || compat::ngRequireObject(editor)->isSelecting)) {
            if (compat::ngRequireObject(editor)->isSelecting)
                NodeSelect::onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
            selectingTargetVertices = false; return value(ID::EndTargetSelection);
        }
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            selectingTargetVertices = true;
            if (!editor) compat::ngFailNullAccess(true);
            editor->maybeSelectOne = UINT64_MAX;
            if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX)
                return value(compat::ngRequireObject(io)->KeyShift ? ID::ToggleTargetVertex : ID::SelectTargetVertex);
            return value(ID::SelectTargetArea);
        }
        return value(ID::ContinueTargetSelection);
    }
    switch (mode) {
    case SubToolMode::Select: return NodeSelect::peek(io, editor);
    case SubToolMode::Vertex:
        if (acquired) { auto session = ngOneTimeDeformScope(); return session->vertActionId = peekShared(session, mode, io); }
        break;
    case SubToolMode::Deform:
        if (acquired) {
            auto session = ngOneTimeDeformScope();
#ifndef NDEBUG
            if (!session->defImpl) throw compat::ngStaticError<compat::AssertError>(
                compat::RetainedString::fromStaticLiteral("Deform editor missing on peek"),
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 1055);
#endif
            return session->defActionId = peekShared(session, mode, io);
        }
        break;
    }
    return 0;
}
template<class T> std::int32_t OneTimeDeform<T>::unify(compat::Slice<std::int32_t> actions) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    for (auto action : actions) if (action == value(ID::SwitchMode)) return action;
    for (auto action : actions) if (action == value(ID::ToggleTargetVertex) || action == value(ID::SelectTargetVertex)) return action;
    for (auto action : actions) if (action >= value(ID::SelectTargetArea) && action <= value(ID::EndTargetSelection)) return action;
    auto session = ngActiveOneTimeDeformScope();
    switch (mode) {
    case SubToolMode::Select: return NodeSelect::unify(actions);
    case SubToolMode::Vertex:
        if (acquired && session && session->vertImpl)
            return compat::ngRequireObject(compat::ngRequireObject(session->vertImpl)->getTool())->unify(actions);
        break;
    case SubToolMode::Deform:
        if (acquired && session && session->defImpl)
            return compat::ngRequireObject(compat::ngRequireObject(session->defImpl)->getTool())->unify(actions);
        break;
    default:
        throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
            "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 1086);
    }
    return 0;
}
template<class T> bool OneTimeDeform<T>::update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    changed = false;
    const auto keepAlive = this->shared_from_this();
    const auto switchLabel = translated("Switch Mode");
    const auto switchKey = translated("TAB");
    widgets::ngStatusTooltip(switchLabel, switchKey);
    const auto selectLabel = translated("Select vertices");
    const auto selectKey = translated("ALT");
    widgets::ngStatusTooltip(selectLabel, selectKey);
    if (appliedMode != mode) { ngOneTimeDeformScope()->enterSubTool<T>(mode); appliedMode = mode; }
    auto ready = ngOneTimeDeformScope(); if (acquired) ready->ensureSubToolReady<T>(mode);
    auto session = ngOneTimeDeformScope(); if (action != value(ID::SwitchMode)) session->switchModeConsumed = false;
    if (action >= value(ID::SelectTargetVertex) && action <= value(ID::EndTargetSelection)) {
        switch (static_cast<ID>(action)) {
        case ID::SelectTargetVertex:
            if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX) {
                if (compat::ngRequireObject(editor)->isSelected(compat::ngRequireObject(editor)->vtxAtMouse)) {
                    editor->maybeSelectOne = editor->vtxAtMouse;
                } else {
                    compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->vtxAtMouse);
                    compat::ngRequireObject(editor)->updateMirrorSelected();
                }
            } else compat::ngRequireObject(editor)->deselectAll();
            break;
        case ID::ToggleTargetVertex:
            if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX)
                compat::ngRequireObject(editor)->toggleSelect(compat::ngRequireObject(editor)->vtxAtMouse);
            break;
        case ID::SelectTargetArea: {
            const auto origin = compat::ngRequireObject(editor)->mousePos;
            editor->selectOrigin = origin; editor->isSelecting = true; break;
        }
        case ID::EndTargetSelection:
            if (compat::ngRequireObject(editor)->maybeSelectOne != UINT64_MAX) {
                compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->maybeSelectOne);
                compat::ngRequireObject(editor)->updateMirrorSelected();
            }
            if (!editor) compat::ngFailNullAccess(true);
            editor->maybeSelectOne = UINT64_MAX; break;
        case ID::ContinueTargetSelection: break;
        default: compat::ngAssertUnreachable("Invalid target selection action", __FILE__, __LINE__);
        }
        compat::ngRequireObject(editor)->updateSelectionPreview(io);
        if (acquired && session->filter) {
            const auto selectionFilter = compat::ngRequireObject(
                std::dynamic_pointer_cast<OneTimeDeformer<T>>(session->filter));
            const auto target = compat::ngRequireObject(editor)->getTarget();
            const auto selected = compat::ngRequireObject(editor)->selected;
            const auto mirrored = compat::ngRequireObject(editor)->mirrorSelected;
            if (selectionFilter->setSelectedVertices(target, selected, mirrored))
                compat::ngRequireObject(incActivePuppet())->update();
        }
        return false;
    }
    if (action == value(ID::SwitchMode)) {
        if (session->switchModeConsumed) return false;
        session->switchModeConsumed = true; incActionPushGroup();
        auto targets = session->defImpl ? compat::ngRequireObject(session->defImpl)->getFilterTargets() :
            (session->vertImpl ? compat::ngRequireObject(session->vertImpl)->getFilterTargets() : compat::Slice<NodeRef>{});
        compat::Slice<SubToolMode> oldModes, newModes;
        for (auto target : targets) {
            auto editorForTarget = operations::impl::ngGetEditorFor(target);
            auto current = ngOneTimeDeformMode(editorForTarget).value_or(mode); oldModes.append(current);
            switch (current) {
            case SubToolMode::Vertex: newModes.append(SubToolMode::Deform); break;
            case SubToolMode::Deform: newModes.append(SubToolMode::Vertex); break;
            case SubToolMode::Select: newModes.append(SubToolMode::Select); break;
            default:
                throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
                    "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 1203);
            }
        }
        auto nextMode = mode;
        switch (mode) {
        case SubToolMode::Select: nextMode = SubToolMode::Vertex; break;
        case SubToolMode::Vertex: nextMode = SubToolMode::Deform; break;
        case SubToolMode::Deform: nextMode = SubToolMode::Vertex; break;
        default:
            throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
                "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 1212);
        }
        switchSubTool(nextMode);
        if (!targets.empty() && newModes.size() == targets.size()) for (std::size_t i = 0; i < targets.size(); ++i) {
            auto current = operations::impl::ngGetEditorFor(targets[i]);
            auto tool = current ? std::dynamic_pointer_cast<OneTimeDeformBase>(current->getTool()) : nullptr;
            if (tool) { tool->mode = newModes[i]; tool->appliedMode = newModes[i]; }
        }
        if (!targets.empty() && oldModes.size() == targets.size() && newModes.size() == targets.size())
            incActionPush(std::allocate_shared<actions::mesheditor::SubToolModeChangeAction>(
                nicxlive::core::d_runtime::SourceAllocator<actions::mesheditor::SubToolModeChangeAction>{},
                targets, oldModes, newModes));
        incActionPopGroup(); return true;
    }
    prevMode = mode;
    switch (mode) {
    case SubToolMode::Select: return NodeSelect::update(io, editor, action, changed);
    case SubToolMode::Vertex:
        if (acquired) {
            auto current = ngOneTimeDeformScope(); const bool result = updateShared(current, mode, action, io, changed);
            if (compat::ngRequireObject(current->vertImpl)->vertexMapDirty) current->applyVertexToolToFilter();
            return result;
        }
        break;
    case SubToolMode::Deform:
        if (acquired) {
            auto current = ngOneTimeDeformScope(); const bool result = updateShared(current, mode, action, io, changed);
            if (changed) {
                if (auto parameter = incArmedParameter()) if (auto currentBinding = binding(parameter->getOrAddBinding(
                    std::dynamic_pointer_cast<T>(current->filter),
                    nicxlive::core::common::NullableString::fromStaticLiteral("deform")))) {
                    auto offsets = compat::ngRequireObject(current->defImpl)->getOffsets();
                    current->normalizeDeformationBinding(currentBinding, offsets.size());
                    currentBinding->update(closest(parameter), offsets);
                    parameter->update(); compat::ngRequireObject(incActivePuppet())->update();
                }
                compat::ngRequireObject(current->defImpl)->markActionDirty();
            }
            return result;
        }
        break;
    }
    return false;
}
template<class T> void OneTimeDeform<T>::draw(base::CameraRef camera, EditorRef editor) {
    using namespace project;
    using namespace core::actionstack;
    using namespace ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::template_detail;
    using detail::ngOneTimeDeformScope;
    using detail::ngActiveOneTimeDeformScope;
    using detail::ngEnsureOneTimeDeformCallbacks;
    const auto keepAlive = this->shared_from_this();
    if (mode == SubToolMode::Select) NodeSelect::draw(camera, editor);
    if (acquired) {
        auto session = ngOneTimeDeformScope();
        if (mode == SubToolMode::Vertex) {
            compat::ngRequireObject(session->defImpl)->draw(camera);
            compat::ngRequireObject(compat::ngRequireObject(session->defImpl)->getTool())->draw(
                camera, session->defImpl);
            compat::ngRequireObject(session->vertImpl)->draw(camera);
            compat::ngRequireObject(compat::ngRequireObject(session->vertImpl)->getTool())->draw(
                camera, session->vertImpl);
        } else {
            compat::ngRequireObject(session->vertImpl)->draw(camera);
            compat::ngRequireObject(compat::ngRequireObject(session->vertImpl)->getTool())->draw(
                camera, session->vertImpl);
            compat::ngRequireObject(session->defImpl)->draw(camera);
            compat::ngRequireObject(compat::ngRequireObject(session->defImpl)->getTool())->draw(
                camera, session->defImpl);
        }
    }
}
}
