#include "onetimedeform_scope.hpp"
#include "grid.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <resource.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform::detail {
using namespace core::actionstack;
using namespace project;
namespace deform = viewport::model::deform;
thread_local FilterEditorMap gFilterEditors;
thread_local std::shared_ptr<OneTimeDeformScope> activeScope;
namespace {
void prepareScopeClassInfo();
thread_local bool registeredCallbacks = false;
Vec2u closest(ParameterRef parameter) { return compat::ngRequireObject(parameter)->findClosestKeypoint(); }
BindingRef asBinding(std::shared_ptr<nicxlive::core::param::ParameterBinding> binding) {
    return std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(binding);
}
bool contains(compat::Slice<NodeRef> values, NodeRef value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}
}
void ngDetachOneTimeDeformFilterForSave(PuppetRef puppet) {
    if (const auto scope = activeScope) scope->detachForSave(puppet);
}
void ngRestoreOneTimeDeformFilterAfterSave(PuppetRef puppet) {
    if (const auto scope = activeScope) scope->restoreAfterSave(puppet);
}
void ngOnOneTimeDeformScopeClosed(ActionStackScopeUnit) {
    if (const auto scope = activeScope) scope->onActionScopeClosed();
}
std::shared_ptr<OneTimeDeformScope> ngOneTimeDeformScope() {
    if (!activeScope) {
        auto scope = std::allocate_shared<OneTimeDeformScope>(
            nicxlive::core::d_runtime::SourceAllocator<OneTimeDeformScope>{});
        prepareScopeClassInfo();
        activeScope = std::move(scope);
    }
    return activeScope;
}
std::shared_ptr<OneTimeDeformScope> ngActiveOneTimeDeformScope() { return activeScope; }
void ngEnsureOneTimeDeformCallbacks() {
    if (registeredCallbacks) return;
    registeredCallbacks = true;
    incRegisterSaveFunc(&ngDetachOneTimeDeformFilterForSave);
    ngRegisterPostSaveFunc(&ngRestoreOneTimeDeformFilterAfterSave);
    ngRegisterActionStackScopeCloseHandler(ActionStackScopeUnit::OneTimeDeform, &ngOnOneTimeDeformScopeClosed);
}
NodeRef OneTimeDeformScope::filterNode() { return std::dynamic_pointer_cast<nicxlive::core::nodes::Node>(filter); }
namespace {
void prepareScopeClassInfo() {
    base::detail::prepareSourceClassRecord<OneTimeDeformScope>();
}
}
compat::RetainedString OneTimeDeformScope::toString() {
    prepareScopeClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t OneTimeDeformScope::toHash() noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool OneTimeDeformScope::opEquals(std::shared_ptr<OneTimeDeformScope> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int OneTimeDeformScope::opCmp(std::shared_ptr<OneTimeDeformScope>) {
    prepareScopeClassInfo();
    base::detail::throwObjectComparison(typeid(*this));
}
void OneTimeDeformScope::begin() {
    if (!actionScope) actionScope = ngOpenActionStackScope(ActionStackScopeUnit::OneTimeDeform);
}
void OneTimeDeformScope::closeActionScope() {
    const auto keepAlive = shared_from_this();
    if (actionScope) {
        explicitScopeClose = true;
        struct Reset { bool& flag; ~Reset() { flag = false; } } reset{explicitScopeClose};
        auto closing = actionScope; actionScope.reset(); closing->close();
    }
}
void OneTimeDeformScope::onActionScopeClosed() {
    const auto keepAlive = shared_from_this();
    if (explicitScopeClose) return;
    actionScope.reset();
    compat::Slice<EditorRef> affected;
    EditorRef finalize;
    if (defImpl) {
        auto targets = compat::ngRequireObject(defImpl)->getFilterTargets();
        for (auto target : targets) {
            auto editor = operations::impl::ngGetEditorFor(target);
            auto tool = editor ? std::dynamic_pointer_cast<OneTimeDeformBase>(editor->getTool()) : nullptr;
            if (tool) { affected.append(editor); if (!finalize && tool->acquired) finalize = editor; }
        }
    }
    if (finalize) finalize->finalizeToolMode();
    if (filter) cleanupTarget(nullptr, true);
    if (!filter) activeScope.reset();
    for (auto editor : affected) if (auto tool = std::dynamic_pointer_cast<OneTimeDeformBase>(editor->getTool())) {
        tool->acquired = false; tool->mode = SubToolMode::Select; tool->appliedMode = SubToolMode::Select;
    }
    if (!affected.empty()) {
        if (auto parent = deform::ngViewportModelDeformGetEditor()) parent->setToolMode(base::VertexToolMode::Points);
        else for (auto editor : affected) editor->setToolMode(base::VertexToolMode::Points);
    }
}
void OneTimeDeformScope::forceFinalize(NodeRef target) {
    const auto keepAlive = shared_from_this();
    if (filter) {
        closeActionScope();
        const auto lookupFilter = filter;
        const auto editors = gFilterEditors;
        if (editors.contains(lookupFilter)) {
            const auto currentFilter = filter;
            gFilterEditors.erase(currentFilter);
        }
        const auto children = compat::ngRequireObject(filterNode())->children.duplicate();
        for (auto child : children) {
            auto current = filter;
            compat::ngRequireObject(current)->applyDeformToChildren({incArmedParameter()}, false);
            compat::ngRequireObject(filter)->releaseTarget(child);
        }
        if (vertImpl) compat::ngRequireObject(vertImpl)->removeFilterTarget(target);
        if (defImpl) compat::ngRequireObject(defImpl)->removeFilterTarget(target);
        if (compat::ngRequireObject(filterNode())->children.empty()) {
            compat::ngRequireObject(filterNode())->reparent(nullptr, 0);
            filter.reset(); initialTargetDeformations = {};
        }
        deform::ngViewportNodeDeformNotifyParamValueChanged();
    }
}
Values OneTimeDeformScope::copyValues(Values source) {
    Values result; result.resize(source.size());
    for (std::size_t x = 0; x < source.size(); ++x) {
        result[x].resize(source[x].size());
        for (std::size_t y = 0; y < source[x].size(); ++y) result[x][y] = source[x][y];
    }
    return result;
}
IsSet OneTimeDeformScope::copyIsSet(IsSet source) {
    IsSet result; result.resize(source.size());
    for (std::size_t x = 0; x < source.size(); ++x) result[x] = source[x].duplicate();
    return result;
}
BindingRef OneTimeDeformScope::backupTargetBinding(ParameterRef parameter, DeformableRef target) {
    const auto keepAlive = shared_from_this();
    auto existing = asBinding(compat::ngRequireObject(parameter)->getBinding(
        target, nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
    const bool added = !existing;
    auto binding = asBinding(compat::ngRequireObject(parameter)->getOrAddBinding(
        target, nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
    if (!binding) return nullptr;
    using BindingStorage = nicxlive::core::param::ParameterBindingImpl<nicxlive::core::param::DeformSlot>;
    saveBackups.append({binding, added, added ? Values{} : copyValues(binding->values),
        added ? IsSet{} : copyIsSet(binding->BindingStorage::getIsSet()), parameter, target});
    return binding;
}
void OneTimeDeformScope::detachFilterNodeForSave(PuppetRef puppet) {
    const auto keepAlive = shared_from_this();
    auto node = filterNode();
    if (!node || !node->parentPtr() || node->puppetRef() != puppet) return;
    detachedFilterParent = node->parentPtr();
    const auto children = compat::ngRequireObject(detachedFilterParent)->children;
    const auto count = children.size();
    auto offset = nicxlive::core::nodes::Node::OFFSET_END;
    for (std::size_t i = 0; i < count; ++i) {
        const auto child = children[i];
        if (nicxlive::core::ngResourceObjectEquals(child, node)) {
            offset = i;
            break;
        }
    }
    detachedFilterOffset = offset;
    node->reparent(nullptr, 0, true);
}
void OneTimeDeformScope::flushCurrentSubToolForSave() {
    const auto keepAlive = shared_from_this();
    if (!active()) return;
    auto parameter = incArmedParameter();
    switch (currentMode) {
    case SubToolMode::Select: break;
    case SubToolMode::Vertex: if (vertImpl) applyVertexToolToFilter(); break;
    case SubToolMode::Deform:
        if (defImpl && parameter) if (auto binding = asBinding(parameter->getOrAddBinding(
                filterNode(), nicxlive::core::common::NullableString::fromStaticLiteral("deform")))) {
            auto offsets = compat::ngRequireObject(defImpl)->getOffsets();
            normalizeDeformationBinding(binding, offsets.size());
            binding->update(closest(parameter), offsets);
            parameter->update(); compat::ngRequireObject(incActivePuppet())->update();
        }
        break;
    default:
        throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
            "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 348);
    }
}
bool OneTimeDeformScope::applyVertexToolToFilter() {
    const auto keepAlive = shared_from_this();
    if (!vertImpl) return false;
    bool applied = false;
    if (auto tool = std::dynamic_pointer_cast<grid::GridTool>(compat::ngRequireObject(vertImpl)->getTool())) {
        applied = tool->applyVirtualMeshToTarget(vertImpl); if (!applied) return false;
    } else { compat::ngRequireObject(vertImpl)->applyToTarget(); applied = true; }
    if (auto parameter = incArmedParameter()) parameter->update();
    compat::ngRequireObject(incActivePuppet())->update();
    if (!vertImpl) compat::ngFailNullAccess(true);
    vertImpl->vertexMapDirty = false;
    return applied;
}
bool OneTimeDeformScope::hasTargetChanges() {
    const auto keepAlive = shared_from_this();
    const auto children = compat::ngRequireObject(filterNode())->children;
    for (auto child : children) {
        auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(child);
        if (!target) continue;
        auto initial = initialTargetDeformations;
        const auto old = initial.find(target);
        if (old == initial.end() || old->second.size() != target->deformation.size()) return true;
        if (target->deformation.forEachValue([&](std::size_t i, nicxlive::core::math::Vec2 value) {
            if (std::abs(value.x - old->second[i].x) > 0.0001f ||
                std::abs(value.y - old->second[i].y) > 0.0001f) return 1;
            return 0;
        })) return true;
    }
    return false;
}
void OneTimeDeformScope::detachForSave(PuppetRef puppet) {
    const auto keepAlive = shared_from_this();
    auto node = filterNode(); if (!node || node->puppetRef() != puppet) return;
    flushCurrentSubToolForSave();
    auto parameter = incArmedParameter();
    if (!parameter) { detachFilterNodeForSave(puppet); return; }
    compat::Slice<SaveTargetFit> fits;
    auto point = closest(parameter);
    const auto children = node->children;
    for (auto child : children) {
        auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(child);
        if (!target) continue;
        auto binding = backupTargetBinding(parameter, target); if (!binding) continue;
        auto initial = binding->getValue(point).vertexOffsets.dup(); initial.resize(target->getVertices().size());
        auto desired = target->deformation.dup(); desired.resize(target->getVertices().size());
        fits.append({target, binding, desired, initial});
    }
    detachFilterNodeForSave(puppet); parameter->update(); compat::ngRequireObject(incActivePuppet())->update();
    for (auto fit : fits) fitOffsetsForFinalResult(parameter, fit.binding, point, fit.target, fit.initial, fit.desired);
}
void OneTimeDeformScope::restoreAfterSave(PuppetRef) {
    const auto keepAlive = shared_from_this();
    const auto backups = saveBackups;
    for (auto backup : backups) {
        if (!backup.binding) continue;
        auto parameter = backup.binding->parameter;
        if (backup.bindingAdded) { if (parameter) parameter->removeBinding(backup.binding); }
        else {
            using BindingStorage = nicxlive::core::param::ParameterBindingImpl<nicxlive::core::param::DeformSlot>;
            backup.binding->values = backup.values;
            backup.binding->BindingStorage::getIsSet() = backup.isSet;
            backup.binding->reInterpolate();
        }
    }
    saveBackups.resize(0);
    auto node = filterNode(); if (!node || !detachedFilterParent) return;
    node->reparent(detachedFilterParent, detachedFilterOffset, true);
    detachedFilterParent.reset(); detachedFilterOffset = 0;
    if (auto parameter = incArmedParameter()) parameter->update();
    compat::ngRequireObject(incActivePuppet())->update();
}
void OneTimeDeformScope::cleanupTarget(EditorRef editor, bool allTargets) {
    const auto keepAlive = shared_from_this();
    if (!filter) return;
    auto node = filterNode();
    if (allTargets) {
        const auto children = compat::ngRequireObject(node)->children.duplicate();
        for (auto child : children) compat::ngRequireObject(filter)->releaseTarget(child);
    }
    else if (editor && editor->getTarget()) {
        const auto children = compat::ngRequireObject(node)->children;
        const auto target = editor->getTarget();
        bool found = false;
        for (auto child : children) {
            if (nicxlive::core::ngResourceObjectEquals(child, target)) { found = true; break; }
        }
        if (found) {
            const auto receiver = compat::ngRequireObject(filter);
            const auto currentTarget = editor->getTarget();
            receiver->releaseTarget(currentTarget);
        }
    }
    if (vertImpl && editor)
        compat::ngRequireObject(vertImpl)->removeFilterTarget(compat::ngRequireObject(editor)->getTarget());
    if (defImpl && editor)
        compat::ngRequireObject(defImpl)->removeFilterTarget(compat::ngRequireObject(editor)->getTarget());
    if (compat::ngRequireObject(node)->children.empty()) {
        const auto lookupFilter = filter;
        const auto editors = gFilterEditors;
        if (editors.contains(lookupFilter)) {
            const auto currentFilter = filter;
            gFilterEditors.erase(currentFilter);
        }
        node->reparent(nullptr, 0); filter.reset(); initialTargetDeformations = {};
        switchModeConsumed = false; invalidatePeekCache(); closeActionScope();
    }
}
void OneTimeDeformScope::invalidatePeekCache() {
    cachedPeekFrame = -1; cachedPeekAction = 0; cachedUpdateFrame = -1; cachedUpdateAction = 0;
    cachedUpdateResult = false; cachedUpdateChanged = false;
}
void OneTimeDeformScope::resetCurrentTool(EditorRef editor) {
    if (!editor || !editor->getTool()) return;
    compat::ngRequireObject(editor->getTool())->setToolMode(editor->getToolMode(), editor);
}
void OneTimeDeformScope::leaveSubTool(SubToolMode current, bool record) {
    const auto keepAlive = shared_from_this();
    if (!active()) return;
    switch (current) {
    case SubToolMode::Select: break;
    case SubToolMode::Vertex:
        if (vertImpl) {
            if (record) compat::ngRequireObject(vertImpl)->pushDeformAction();
            applyVertexToolToFilter();
        }
        break;
    case SubToolMode::Deform:
        if (defImpl && record) compat::ngRequireObject(defImpl)->pushDeformAction();
        break;
    default:
        throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
            "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 522);
    }
}
void OneTimeDeformScope::seedViewFromBinding() {
    const auto keepAlive = shared_from_this();
    auto parameter = incArmedParameter(); if (!parameter || !defImpl) return;
    if (auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(
            compat::ngRequireObject(defImpl)->getTarget())) {
        auto binding = asBinding(parameter->getBinding(
            target, nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
        if (!binding) return;
        auto offsets = binding->getValue(closest(parameter)).vertexOffsets.dup();
        auto length = compat::ngRequireObject(defImpl)->getOffsets().size();
        if (offsets.size() != length) {
            Vec2Array resized(length); const auto n = std::min(offsets.size(), length);
            for (std::size_t i = 0; i < n; ++i) resized[i] = offsets[i];
            for (std::size_t i = n; i < length; ++i) resized[i] = nicxlive::core::math::Vec2{0, 0};
            offsets = resized;
        }
        compat::ngRequireObject(defImpl)->applyOffsets(offsets);
    }
}
void OneTimeDeformScope::normalizeDeformationBinding(BindingRef binding, std::size_t length) {
    if (!binding) return;
    for (std::size_t x = 0; x < binding->values.size(); ++x)
        for (std::size_t y = 0; y < binding->values[x].size(); ++y) {
            auto offsets = binding->values[x][y].vertexOffsets.dup(); if (offsets.size() == length) continue;
            Vec2Array resized(length); const auto n = std::min(offsets.size(), length);
            for (std::size_t i = 0; i < n; ++i) resized[i] = offsets[i];
            for (std::size_t i = n; i < length; ++i) resized[i] = nicxlive::core::math::Vec2{0, 0};
            binding->values[x][y].vertexOffsets = resized;
        }
}
float OneTimeDeformScope::squaredError(Vec2Array lhs, Vec2Array rhs) {
    const auto count = std::min(lhs.size(), rhs.size()); if (!count) return std::numeric_limits<float>::max();
    double total = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto a = lhs[i].toVector(), b = rhs[i].toVector();
        const float x = a.x - b.x, y = a.y - b.y;
        if (!std::isfinite(x) || !std::isfinite(y)) return std::numeric_limits<float>::max();
        total += static_cast<double>(x) * x + static_cast<double>(y) * y;
    }
    return static_cast<float>(total / count);
}
Vec2Array OneTimeDeformScope::evaluateWithCandidate(ParameterRef parameter, BindingRef binding,
    Vec2u point, Vec2Array candidate) {
    const auto keepAlive = shared_from_this();
    normalizeDeformationBinding(binding, candidate.size());
    compat::ngRequireObject(binding)->update(point, candidate);
    compat::ngRequireObject(parameter)->update(); compat::ngRequireObject(incActivePuppet())->update();
    auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(
        compat::ngRequireObject(binding)->getTarget().target.lock());
    return target ? target->deformation.dup() : Vec2Array{};
}
Vec2Array OneTimeDeformScope::fitOffsetsForFinalResult(ParameterRef parameter, BindingRef binding, Vec2u point,
    DeformableRef target, Vec2Array initial, Vec2Array desired) {
    const auto keepAlive = shared_from_this();
    if (desired.size() != compat::ngRequireObject(target)->getVertices().size()) return initial;
    auto candidate = initial.dup(); candidate.resize(desired.size());
    auto evaluated = evaluateWithCandidate(parameter, binding, point, candidate);
    float currentError = squaredError(evaluated, desired);
    auto best = candidate.dup(); float bestError = currentError, step = 1.0f;
    for (int iteration = 0; iteration < 10; ++iteration) {
        if (bestError <= 0.0001f) break;
        auto trial = candidate.dup();
        const auto count = std::min({trial.size(), desired.size(), evaluated.size()});
        for (std::size_t i = 0; i < count; ++i) {
            auto slot = trial[i];
            const auto b = desired[i].toVector(), c = evaluated[i].toVector();
            slot = nicxlive::core::math::Vec2{slot.x + (b.x - c.x) * step, slot.y + (b.y - c.y) * step};
        }
        auto trialEvaluated = evaluateWithCandidate(parameter, binding, point, trial);
        const float trialError = squaredError(trialEvaluated, desired);
        if (trialError < currentError) {
            candidate = trial; evaluated = trialEvaluated; currentError = trialError;
            if (trialError < bestError) { best = trial.dup(); bestError = trialError; }
            step = std::min(step * 1.25f, 1.0f);
        } else { step *= 0.5f; if (step < 0.015625f) break; }
    }
    compat::ngRequireObject(binding)->update(point, best);
    compat::ngRequireObject(parameter)->update(); compat::ngRequireObject(incActivePuppet())->update(); return best;
}
void OneTimeDeformScope::applyToTargetsWithFitting(ParameterRef parameter, Vec2u point) {
    const auto keepAlive = shared_from_this();
    compat::Slice<SaveTargetFit> fits;
    const auto children = compat::ngRequireObject(filterNode())->children;
    for (auto child : children) {
        auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(child); if (!target) continue;
        auto binding = asBinding(compat::ngRequireObject(parameter)->getOrAddBinding(
            target, nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
        if (!binding) continue;
        auto initial = binding->getValue(point).vertexOffsets.dup(); initial.resize(target->getVertices().size());
        auto desired = target->deformation.dup(); desired.resize(target->getVertices().size());
        fits.append({target, binding, desired, initial});
    }
    const auto release = compat::ngRequireObject(filterNode())->children.duplicate();
    for (auto child : release) compat::ngRequireObject(filter)->releaseTarget(child);
    const auto receiver = compat::ngRequireObject(parameter);
    if (auto binding = receiver->getBinding(filterNode(),
            nicxlive::core::common::NullableString::fromStaticLiteral("deform")))
        compat::ngRequireObject(parameter)->removeBinding(binding);
    for (auto fit : fits) fitOffsetsForFinalResult(parameter, fit.binding, point, fit.target, fit.initial, fit.desired);
}
}

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform {
EditorRef ngGetArmedParameterEditorFor(NodeRef node) {
    if (auto filter = std::dynamic_pointer_cast<nicxlive::core::nodes::NodeFilter>(node)) {
        const auto editors = detail::gFilterEditors;
        if (editors.contains(filter)) {
            const auto currentEditors = detail::gFilterEditors;
            const auto current = currentEditors.find(filter);
            if (current == currentEditors.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 1395);
            return current->second;
        }
    }
    auto session = detail::ngActiveOneTimeDeformScope();
    if (session && session->filter) {
        const auto children = compat::ngRequireObject(session->filterNode())->children;
        for (auto child : children) if (child == node) {
            const auto filter = session->filter;
            const auto editors = detail::gFilterEditors;
            if (editors.contains(filter)) {
                const auto currentFilter = session->filter;
                const auto currentEditors = detail::gFilterEditors;
                const auto current = currentEditors.find(currentFilter);
                if (current == currentEditors.end()) throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral(
                        "source/nijigenerate/viewport/common/mesheditor/tools/onetimedeform.d"), 1403);
                return current->second;
            }
            return session->defImpl;
        }
    }
    return nullptr;
}
std::shared_ptr<nicxlive::core::nodes::NodeFilter> ngCurrentNodeFilter() {
    auto session = detail::ngActiveOneTimeDeformScope(); return session ? session->filter : nullptr;
}
std::optional<SubToolMode> ngOneTimeDeformMode(EditorRef editor) {
    if (editor) if (auto tool = std::dynamic_pointer_cast<OneTimeDeformBase>(editor->getTool())) return tool->mode;
    return {};
}
void ngRestoreOneTimeDeformMode(EditorRef editor, SubToolMode mode) {
    if (editor) if (auto tool = std::dynamic_pointer_cast<OneTimeDeformBase>(editor->getTool())) tool->mode = mode;
}
void OneTimeDeformBase::setSubToolMode(SubToolMode next, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (mode == next || mode == SubToolMode::Select) return;
    const auto session = detail::ngOneTimeDeformScope();
    session->switchModeConsumed = false;
    bool changed = false;
    update(&ImGui::GetIO(), editor, static_cast<std::int32_t>(OneTimeDeformActionID::SwitchMode), changed);
}
}
