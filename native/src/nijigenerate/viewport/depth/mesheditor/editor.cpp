// Copyright 2024 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "editor.hpp"
#include "action.hpp"
#include <nijigenerate/commands/depth/editor_commands.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/commands/depth/map.hpp>
#include <nijigenerate/viewport/depth/renderer.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/ext/nodes/exdepthops.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/widgets/drag.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nodes/grid_deformer.hpp>
#include <puppet.hpp>
#include <cmath>
#include <limits>
#include <bit>
#include <string>
#include <new>
#include <source_location>
#include <utility>
#include <tuple>
#include <stdexcept>
#include <string_view>

namespace nijigenerate::viewport::depth::mesheditor::editor {
using namespace tools::operation;
namespace {
using NodeRef = DepthMeshEditor::NodeRef;
using GridRef = DepthMeshEditor::GridRef;
using DeformableRef = std::shared_ptr<nicxlive::core::nodes::Deformable>;
template<class T, class... Args> std::shared_ptr<T> allocateDepthEditorObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
template<class K, class V, class H, class E>
V sourceValue(const nicxlive::core::common::ResourceKeyMap<K, V, H, E>& map,
    const K& key, std::size_t line) {
    const auto found = map.findRetained(key);
    if (!found) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/mesheditor/editor.d"), line);
    return *found;
}
#if defined(_WIN32)
using SourceReal = double;
#else
using SourceReal = long double;
#endif
SourceReal distance(DepthMeshEditor::Vec2 left, DepthMeshEditor::Vec2 right) {
    const float x = left.x - right.x, y = left.y - right.y;
    // This source calls Vector.length directly: keep its real result until
    // bestDistance's explicit float assignment, rather than a distance!vec2 return.
    SourceReal sum = 0; sum += x * x; sum += y * y;
    return std::sqrt(sum);
}
template<std::size_t N> void text(const char (&key)[N]) {
    const auto value = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
    const compat::SourceCString cString(value);
    compat::detail::ngSourceCStringPostconditions(value.view(), cString.data());
    ImGui::Text(cString.data());
}

}
void DepthMeshEditor::prepareSourceClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthMeshEditor),
            compat::RetainedString::fromStaticLiteral(
                nicxlive::core::d_runtime::SourceObjectTypeNames<DepthMeshEditor>::className));
        return true;
    }();
    (void)registered;
}

// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString DepthMeshEditor::toString() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthMeshEditor::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthMeshEditor::opEquals(std::shared_ptr<DepthMeshEditor> other) {
    return other && dynamic_cast<const void*>(this) == dynamic_cast<const void*>(other.get());
}
int DepthMeshEditor::opCmp(std::shared_ptr<DepthMeshEditor>) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    compat::RetainedString message;
    try {
        message = compat::RetainedString(std::string("need opCmp for class ") + std::string(className.view()));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(std::move(message),
        compat::RetainedString::fromStaticLiteral("object.d"), 195);
}

bool DepthMeshEditor::editable(NodeRef node) {
    return bool(std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(node));
}
void DepthMeshEditor::loadOperationsFromTarget(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); if (!editor) return;
    const auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(compat::ngRequireObject(editor)->getTarget());
    if (!operated) return; const auto saved = compat::ngRequireObject(operated)->copyDepthOps(); if (saved.empty()) return;
    Operations loaded; for (const auto& op : saved) loaded.append(depthOperationFromExDepthOp(op));
    operations[editor] = loaded; const auto base = compat::ngRequireObject(operated)->copyDepthOpBaseDepths();
    if (base.size() == compat::ngRequireObject(compat::ngRequireObject(editor)->getTarget())->getVertices().size()) compat::ngRequireObject(editor)->replaceBaseDepths(base); else compat::ngRequireObject(editor)->clearBaseDepths();
    recompute(editor);
}
void DepthMeshEditor::syncOperationsFromTarget(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (!editor) return; operations.erase(editor); loadOperationsFromTarget(editor);
}
DepthMeshEditor::JsonValue DepthMeshEditor::operationsToJson(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    JsonValue saved(core::settings::SettingsJsonArray{});
    if (operations.findRetained(editor)) {
        const auto list = sourceValue(operations, editor, 89);
        for (const auto operation : list)
            saved.as_array().append(commands::depth::map::ngDepthOpToJson(toExDepthOp(operation)));
    }
    return saved;
}
DepthMeshEditor::JsonValue DepthMeshEditor::targetOperationsToJson(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    JsonValue saved(core::settings::SettingsJsonArray{});
    if (!editor) return saved;
    const auto target = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(compat::ngRequireObject(editor)->getTarget());
    if (!target) return saved;
    for (const auto& operation : compat::ngRequireObject(target)->copyDepthOps())
        saved.as_array().append(commands::depth::map::ngDepthOpToJson(operation));
    return saved;
}
void DepthMeshEditor::applyToTargets() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); core::actionstack::incActionPushGroup();
    const auto retained = editors;
    for (const auto [grid, editor] : retained) {
        const auto context = allocateDepthEditorObject<commands::Context>();
        const auto next = operationsToJson(editor), current = targetOperationsToJson(editor);
        const bool changed = next != current;
        using commands::depth::map::DepthMapCommand;
        using commands::depth::map::ngRunDepthMapCommand;
        if (directDepthDirty.findRetained(editor)) {
            if (!current.as_array().empty()) ngRunDepthMapCommand<DepthMapCommand::SetDepthOps>(context,
                compat::ngRequireObject(editor)->targetNode(), JsonValue(core::settings::SettingsJsonArray{}));
            const auto target = compat::ngRequireObject(editor)->targetNode(); const auto depths = compat::ngRequireObject(editor)->copyEditorDepths();
            ngRunDepthMapCommand<DepthMapCommand::SetDepths>(context, target, depths);
            directDepthDirty.erase(editor);
        } else if (changed) {
            ngRunDepthMapCommand<DepthMapCommand::SetDepthOps>(context, compat::ngRequireObject(editor)->targetNode(), next);
            ngRunDepthMapCommand<DepthMapCommand::ApplyDepthOps>(context, compat::ngRequireObject(editor)->targetNode());
        }
        compat::ngRequireObject(editor)->resetFromTarget(); syncOperationsFromTarget(editor);
    }
    // The original uses an ordinary pop, not scope(exit); exceptions leave its group open.
    core::actionstack::incActionPopGroup();
}
bool DepthMeshEditor::commitOperationAdd(EditorRef editor, OperationRef operation, std::int32_t index) {
    const auto keepAlive = shared_from_this();
    const auto context = allocateDepthEditorObject<commands::Context>();
    const auto result = commands::ngDispatchRegisteredCommand<commands::depth::editor::AddEditorDepthOpCommand>(
        commands::depth::editor::commands, commands::depth::editor::DepthEditorOperationCommand::AddEditorDepthOp,
        context, {"DepthEditorOperationCommand.AddEditorDepthOp", "DepthEditorOperationCommand", "AddEditorDepthOpCommand"},
        keepAlive, editor, operation, index);
    return compat::ngRequireObject(result)->succeeded;
}
bool DepthMeshEditor::commitOperationUpdate(EditorRef editor, std::ptrdiff_t index, OperationRef operation) {
    const auto keepAlive = shared_from_this();
    const auto context = allocateDepthEditorObject<commands::Context>();
    const auto narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index));
    const auto result = commands::ngDispatchRegisteredCommand<commands::depth::editor::UpdateEditorDepthOpCommand>(
        commands::depth::editor::commands, commands::depth::editor::DepthEditorOperationCommand::UpdateEditorDepthOp,
        context, {"DepthEditorOperationCommand.UpdateEditorDepthOp", "DepthEditorOperationCommand", "UpdateEditorDepthOpCommand"},
        keepAlive, editor, narrowed, operation);
    return compat::ngRequireObject(result)->succeeded;
}
bool DepthMeshEditor::commitOperationRemove(EditorRef editor, std::ptrdiff_t index) {
    const auto keepAlive = shared_from_this();
    const auto context = allocateDepthEditorObject<commands::Context>();
    const auto narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index));
    const auto result = commands::ngDispatchRegisteredCommand<commands::depth::editor::RemoveEditorDepthOpCommand>(
        commands::depth::editor::commands, commands::depth::editor::DepthEditorOperationCommand::RemoveEditorDepthOp,
        context, {"DepthEditorOperationCommand.RemoveEditorDepthOp", "DepthEditorOperationCommand", "RemoveEditorDepthOpCommand"},
        keepAlive, editor, narrowed);
    return compat::ngRequireObject(result)->succeeded;
}
DepthMeshEditor::DepthMeshEditor(bool resources) {
    initializeSource(resources);
}
void DepthMeshEditor::initializeSource(bool resources) {
    buildRenderResources = resources;
    viewSession = allocateDepthEditorObject<common::session::DepthViewSession>();
    if (buildRenderResources) renderer = allocateDepthEditorObject<DepthTextureMeshRenderer>();
    actionScope = core::actionstack::ngOpenActionStackScope(core::actionstack::ActionStackScopeUnit::DepthEdit);
}
DepthMeshEditor::~DepthMeshEditor() noexcept(false) { dispose(); }
void DepthMeshEditor::dispose() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    closeStack();
    // Private pending storage has a weak editor backedge; published undo refs
    // retain the actual editor separately. Preserve the source drag fields here.
    const auto retained = editors;
    for (const auto [grid, editor] : retained) compat::ngRequireObject(editor)->dispose();
    editors.clear(); operations.clear(); if (viewSession) compat::ngRequireObject(viewSession)->clear();
}
void DepthMeshEditor::closeStack() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto scope = actionScope; if (scope) { scope->close(); actionScope.reset(); }
}
void DepthMeshEditor::setTargets(compat::Slice<NodeRef> targets) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    decltype(editors) next; compat::Slice<DeformableRef> nextViewTargets;
    for (auto node : targets) {
        if (!editable(node)) continue;
        const auto grid = std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(node); nextViewTargets.append(grid);
        if (editors.findRetained(grid)) next[grid] = sourceValue(editors, grid, 159);
        else { const auto editor = node::DepthMeshEditorOne::createSource([&] {
                return std::tuple{grid, buildRenderResources};
            });
            next[grid] = editor; loadOperationsFromTarget(editor); }
    }
    const auto previous = editors;
    for (const auto [grid, editor] : previous) if (!next.findRetained(grid)) {
        operations.erase(editor); directDepthDirty.erase(editor);
        if (selectedOperationEditor == editor) { selectedOperationEditor.reset(); selectedOperationIndex = -1; }
        compat::ngRequireObject(editor)->dispose();
    }
    editors = next;
    if (viewSession) {
        compat::ngRequireObject(viewSession)->setTargets(nextViewTargets); compat::Slice<DeformableRef> displayTargets;
        const auto puppet = project::incActivePuppet();
        if (puppet) for (auto grid : puppet->findNodesType<nicxlive::core::nodes::GridDeformer>(puppet->root)) {
            const auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(grid);
            if (mapped && !mapped->copyDepths().empty()) displayTargets.append(grid);
        }
        if (displayTargets.empty()) displayTargets = nextViewTargets;
        compat::ngRequireObject(viewSession)->setDepthDisplayScale(common::targetview::ngDepthDisplayScaleForTargetsInNodeSpace(
            puppet ? puppet->root : nullptr, displayTargets));
        const auto retained = editors;
        for (const auto [grid, editor] : retained) {
            compat::ngRequireObject(editor)->bindTargetView(compat::ngRequireObject(viewSession)->targetByGrid(compat::ngRequireObject(grid)->getUUID()));
            if (operations.findRetained(editor)) recompute(editor);
        }
    }
}
DepthMeshEditor::SessionRef DepthMeshEditor::depthViewSession() { return viewSession; }
DepthMeshEditor::ViewRef DepthMeshEditor::targetViewFor(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); if (!editor || !viewSession) return {};
    const auto target = compat::ngRequireObject(editor)->getTarget(); return target ? compat::ngRequireObject(viewSession)->targetByGrid(compat::ngRequireObject(target)->getUUID()) : nullptr;
}
compat::Slice<DepthMeshEditor::GridRef> DepthMeshEditor::getTargets() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto keys = editors.keys();
    return compat::Slice<GridRef>::retainedView(keys.retentionOwner(), keys.data(), keys.size());
}
DepthMeshEditor::EditorRef DepthMeshEditor::getEditorFor(GridRef grid) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (editors.findRetained(grid)) return sourceValue(editors, grid, 214);
    return {};
}
compat::Slice<DepthMeshEditor::EditorRef> DepthMeshEditor::getEditors() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto values = editors.values();
    return compat::Slice<EditorRef>::retainedView(values.retentionOwner(), values.data(), values.size());
}
void DepthMeshEditor::resetFromTargets() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); const auto retained = editors;
    for (const auto [grid, editor] : retained) {
        compat::ngRequireObject(editor)->resetFromTarget(); replaceOperations(editor, {}); directDepthDirty.erase(editor); loadOperationsFromTarget(editor);
    }
    if (viewSession) compat::ngRequireObject(viewSession)->resetFromTargets();
}
void DepthMeshEditor::draw(CameraRef camera, DepthCamera3D& depthCamera) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    EditorRef hotEditor; std::ptrdiff_t hotIndex; Handle hotHandle;
    const auto mouse = core::input::ngInputGetMousePosition();
    findOperationHit({-mouse.x, -mouse.y}, depthCamera, hotEditor, hotIndex, hotHandle);
    const auto retained = editors;
    for (const auto [grid, editor] : retained) {
        if (renderer) compat::ngRequireObject(editor)->draw(camera, depthCamera, renderer);
        if (!operations.findRetained(editor)) continue;
        const auto list = sourceValue(operations, editor, 267);
        for (std::size_t i = 0; i < list.size(); ++i) {
            const auto index = std::ptrdiff_t(i); const bool selected = editor == selectedOperationEditor && index == selectedOperationIndex;
            Handle handle = Handle::None;
            if (draggingOperation && selected) handle = selectedOperationHandle;
            else if (editor == hotEditor && index == hotIndex) handle = hotHandle;
            compat::ngRequireObject(list[i])->draw(editor, depthCamera, selected, handle);
        }
    }
}
bool DepthMeshEditor::update(ImGuiIO*, CameraRef) { return false; }
DepthMeshEditor::EditorRef DepthMeshEditor::findNearestProjectedVertex(Vec2 point, float radius, std::ptrdiff_t& index) {
    index = 0;
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    EditorRef best; std::ptrdiff_t bestIndex = -1; float bestDistance = radius; const auto retained = editors;
    for (const auto [grid, editor] : retained) {
        const auto candidate = compat::ngRequireObject(editor)->nearestProjectedVertex(point, bestDistance); if (candidate < 0) continue;
        const auto& projectedPoints = compat::ngRequireObject(editor)->projectedPoints;
        const auto position = static_cast<std::size_t>(candidate);
        if (position >= projectedPoints.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            position, projectedPoints.size(), compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/depth/mesheditor/editor.d"), 293);
        const auto next = distance(projectedPoints[position], point);
        if (next < bestDistance) { bestDistance = static_cast<float>(next); best = editor; bestIndex = candidate; }
    }
    index = bestIndex; return best;
}
DepthMeshEditor::Operations DepthMeshEditor::copyOperations(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); Operations result;
    if (operations.findRetained(editor)) { const auto list = sourceValue(operations, editor, 308); for (auto op : list) result.append(compat::ngRequireObject(op)->clone()); }
    return result;
}
std::ptrdiff_t DepthMeshEditor::indexOfOperationInstance(EditorRef editor, OperationRef operation) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (!editor || !operation) return -1; if (!operations.findRetained(editor)) return -1;
    const auto list = sourceValue(operations, editor, 315); for (std::size_t i = 0; i < list.size(); ++i) if (list[i] == operation) return std::ptrdiff_t(i);
    return -1;
}
void DepthMeshEditor::selectOperation(EditorRef editor, std::ptrdiff_t index) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    selectedOperationEditor = std::move(editor); selectedOperationIndex = index;
}
void DepthMeshEditor::replaceOperations(EditorRef editor, Operations next) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); if (!editor) return;
    if (next.empty()) operations.erase(editor);
    else { Operations copied; for (auto op : next) copied.append(compat::ngRequireObject(op)->clone()); operations[editor] = copied; }
    recompute(editor);
}
void DepthMeshEditor::markDirectDepthDirty(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (editor) directDepthDirty[editor] = true;
}
bool DepthMeshEditor::isDirectDepthDirty(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    return editor && bool(directDepthDirty.findRetained(editor));
}
void DepthMeshEditor::replaceDirectDepths(EditorRef editor, compat::Slice<float> depths) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (!editor) return; compat::ngRequireObject(editor)->replaceEditorDepths(depths); markDirectDepthDirty(editor);
}
void DepthMeshEditor::beginDirectDepthEdit(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); if (editor) markDirectDepthDirty(editor);
}
void DepthMeshEditor::replaceDepthState(EditorRef editor, compat::Slice<float> depths, compat::Slice<float> baseDepths,
    Operations nextOperations, bool dirty) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); if (!editor) return;
    compat::ngRequireObject(editor)->replaceBaseDepths(baseDepths); replaceOperations(editor, nextOperations); compat::ngRequireObject(editor)->replaceEditorDepths(depths);
    if (dirty) directDepthDirty[editor] = true; else directDepthDirty.erase(editor);
}
void DepthMeshEditor::appendOperation(EditorRef editor, OperationRef operation) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); if (!editor || !operation) return;
    operations[editor].append(operation); recompute(editor);
}
void DepthMeshEditor::recompute(EditorRef editor) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (!editor) return; compat::ngRequireObject(editor)->resetWorkingDepths();
    if (!operations.findRetained(editor)) return;
    compat::Slice<std::shared_ptr<DepthRingOperation>> rings;
    compat::Slice<std::shared_ptr<DepthAttachedPointOperation>> attached;
    compat::Slice<std::shared_ptr<DepthPlaneOperation>> planes;
    const auto list = sourceValue(operations, editor, 389);
    for (auto op : list) {
        if (auto ring = std::dynamic_pointer_cast<DepthRingOperation>(op)) rings.append(ring);
        else if (auto point = std::dynamic_pointer_cast<DepthAttachedPointOperation>(op)) attached.append(point);
        else if (auto plane = std::dynamic_pointer_cast<DepthPlaneOperation>(op)) planes.append(plane);
    }
    applyRingNormalSurfaces(editor, rings);
    for (auto op : attached) compat::ngRequireObject(op)->apply(editor);
    for (auto op : planes) compat::ngRequireObject(op)->apply(editor);
}
DepthMeshEditor::OperationRef DepthMeshEditor::selectedOperation() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (!selectedOperationEditor) return {};
    if (!operations.findRetained(selectedOperationEditor)) return {}; const auto list = sourceValue(operations, selectedOperationEditor, 408);
    if (selectedOperationIndex < 0 || std::size_t(selectedOperationIndex) >= list.size()) return {};
    return list[std::size_t(selectedOperationIndex)];
}
bool DepthMeshEditor::findOperationHit(Vec2 mouse, DepthCamera3D& camera, EditorRef& hitEditor,
    std::ptrdiff_t& hitIndex, Handle& hitHandle, OperationFilter filter) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    hitEditor.reset(); hitIndex = -1; hitHandle = Handle::None;
    float best = std::numeric_limits<float>::max(); const auto retained = editors;
    for (const auto [grid, editor] : retained) {
        if (!operations.findRetained(editor)) continue; const auto list = sourceValue(operations, editor, 427);
        for (std::size_t i = 0; i < list.size(); ++i) {
            const auto op = list[i]; if (filter && !filter(op)) continue;
            float distance = NAN;
            const float denominator = .01f < viewport::base::viewportZoom ? viewport::base::viewportZoom : .01f;
            const auto handle = compat::ngRequireObject(op)->hit(editor, mouse, camera, 14.f / denominator, distance);
            if (handle == Handle::None || distance >= best) continue;
            best = distance; hitEditor = editor; hitIndex = std::ptrdiff_t(i); hitHandle = handle;
        }
    }
    return bool(hitEditor);
}
bool DepthMeshEditor::beginOperationDrag(Vec2 mouse, float mouseY, DepthCamera3D& camera, OperationFilter filter) {
    const auto keepAlive = shared_from_this(); EditorRef hit; std::ptrdiff_t index; Handle handle;
    if (!findOperationHit(mouse, camera, hit, index, handle, filter)) return false;
    selectedOperationEditor = hit; selectedOperationIndex = index; selectedOperationHandle = handle;
    dragStartOperation = compat::ngRequireObject(selectedOperation())->clone(); dragStartLocal = compat::ngRequireObject(hit)->displayWorldToLocal(mouse, camera);
    dragStartMouseY = mouseY; dragOperationAction = action::DepthOperationListChangeAction::createPending(keepAlive, hit);
    draggingOperation = true; return true;
}
bool DepthMeshEditor::updateOperationDrag(Vec2 mouse, float mouseY, DepthCamera3D& camera, bool snap) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    if (!draggingOperation) return false; const auto op = selectedOperation();
    if (!op || !selectedOperationEditor || !dragStartOperation) return false;
    const auto receiver = op;
    const auto handle = selectedOperationHandle;
    const auto editor = selectedOperationEditor;
    const auto start = dragStartOperation;
    const auto startLocal = dragStartLocal;
    const auto endLocal = compat::ngRequireObject(selectedOperationEditor)->displayWorldToLocal(mouse, camera);
    const auto startMouseY = dragStartMouseY;
    compat::ngRequireObject(receiver)->drag(handle, editor, start, startLocal,
        endLocal, startMouseY, mouseY, snap);
    recompute(selectedOperationEditor); return true;
}
bool DepthMeshEditor::endOperationDrag() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); if (!draggingOperation) return false;
    if (dragOperationAction) {
        compat::ngRequireObject(dragOperationAction)->updateNewState();
        core::actionstack::incActionPush(
            action::DepthOperationListChangeAction::retainForPublication(dragOperationAction));
    }
    draggingOperation = false; dragOperationAction.reset(); dragStartOperation.reset(); selectedOperationHandle = Handle::None; return true;
}
void DepthMeshEditor::duplicateSelectedOperation() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock(); const auto op = selectedOperation();
    if (!op) return;
    const auto target = selectedOperationEditor;
    const auto copy = compat::ngRequireObject(op)->clone();
    commitOperationAdd(target, copy);
}
void DepthMeshEditor::deleteSelectedOperation() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto op = selectedOperation();
    if (!op) return;
    commitOperationRemove(selectedOperationEditor, selectedOperationIndex);
}
void DepthMeshEditor::editSelectedFloat(compat::RetainedString id, float* value, float speed, float minimum, float maximum,
    compat::RetainedString format) {
    const auto keepAlive = shared_from_this();
    const auto op = selectedOperation();
    if (!op) return;
    const auto action = action::DepthOperationListChangeAction::createSource([&] {
        return std::tuple{keepAlive, selectedOperationEditor};
    });
    if (widgets::ngDragFloat(compat::RetainedStringSlice(id), value, speed, minimum, maximum,
        compat::RetainedStringSlice(format), ImGuiSliderFlags_NoRoundToFormat)) {
        recompute(selectedOperationEditor); action->updateNewState(); core::actionstack::incActionPush(action);
    }
}
bool DepthMeshEditor::drawFloatInput(compat::RetainedString id, float& value, float speed, float minimum, float maximum,
    compat::RetainedString format) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    return widgets::ngDragFloat(compat::RetainedStringSlice(id), &value, speed, minimum, maximum,
        compat::RetainedStringSlice(format), ImGuiSliderFlags_NoRoundToFormat);
}
void DepthMeshEditor::pushSelectedOperationEdit(ActionRef action) {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    const auto op = selectedOperation();
    if (!op) return;
    recompute(selectedOperationEditor); compat::ngRequireObject(action)->updateNewState(); core::actionstack::incActionPush(action);
}
void DepthMeshEditor::drawSelectedOperationEditor() {
    const auto keepAlive = shared_from_this(); const auto op = selectedOperation(); text("Selected Deformer");
    if (!op) { text("No operation selected."); return; }
    auto label = compat::ngRequireObject(op)->label();
    const std::array<core::translation::TypedFormatArgument, 1> labelArguments{
        core::translation::ngFormatArgument(std::move(label))};
    auto formatted = core::translation::ngFormatTyped("%s", labelArguments);
    const compat::SourceCString formattedCString{std::string_view(formatted)};
    compat::detail::ngSourceCStringPostconditions(formatted, formattedCString.data());
    ImGui::Text(formattedCString.data());
    constexpr float maximum = std::numeric_limits<float>::max();
    if (auto attached = std::dynamic_pointer_cast<DepthAttachedPointOperation>(op)) {
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Amount"), &compat::ngRequireObject(attached)->amount, .01f, -2, 2);
    } else if (auto ring = std::dynamic_pointer_cast<DepthRingOperation>(op)) {
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Amount"), &compat::ngRequireObject(ring)->amount, .01f, -2, 2);
        float x0 = compat::ngRequireObject(ring)->p0.x;
        if (drawFloatInput(compat::RetainedString::fromStaticLiteral("P0 X"), x0, 1, -maximum, maximum, compat::RetainedString::fromStaticLiteral("%.0f"))) {
            auto action = action::DepthOperationListChangeAction::createSource([&] {
                return std::tuple{keepAlive, selectedOperationEditor};
            });
            compat::ngRequireObject(ring)->p0 = {x0, compat::ngRequireObject(ring)->p0.y}; pushSelectedOperationEdit(action);
        }
        float y0 = compat::ngRequireObject(ring)->p0.y;
        if (drawFloatInput(compat::RetainedString::fromStaticLiteral("P0 Y"), y0, 1, -maximum, maximum, compat::RetainedString::fromStaticLiteral("%.0f"))) {
            auto action = action::DepthOperationListChangeAction::createSource([&] {
                return std::tuple{keepAlive, selectedOperationEditor};
            });
            compat::ngRequireObject(ring)->p0 = {compat::ngRequireObject(ring)->p0.x, y0}; pushSelectedOperationEdit(action);
        }
        float x1 = compat::ngRequireObject(ring)->p1.x;
        if (drawFloatInput(compat::RetainedString::fromStaticLiteral("P1 X"), x1, 1, -maximum, maximum, compat::RetainedString::fromStaticLiteral("%.0f"))) {
            auto action = action::DepthOperationListChangeAction::createSource([&] {
                return std::tuple{keepAlive, selectedOperationEditor};
            });
            compat::ngRequireObject(ring)->p1 = {x1, compat::ngRequireObject(ring)->p1.y}; pushSelectedOperationEdit(action);
        }
        float y1 = compat::ngRequireObject(ring)->p1.y;
        if (drawFloatInput(compat::RetainedString::fromStaticLiteral("P1 Y"), y1, 1, -maximum, maximum, compat::RetainedString::fromStaticLiteral("%.0f"))) {
            auto action = action::DepthOperationListChangeAction::createSource([&] {
                return std::tuple{keepAlive, selectedOperationEditor};
            });
            compat::ngRequireObject(ring)->p1 = {compat::ngRequireObject(ring)->p1.x, y1}; pushSelectedOperationEdit(action);
        }
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("P0 Angle"), &compat::ngRequireObject(ring)->p0Angle, 1, -360, 360, compat::RetainedString::fromStaticLiteral("%.0f"));
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("P1 Angle"), &compat::ngRequireObject(ring)->p1Angle, 1, -360, 360, compat::RetainedString::fromStaticLiteral("%.0f"));
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Width"), &compat::ngRequireObject(ring)->width, 1, 1, 400, compat::RetainedString::fromStaticLiteral("%.0f"));
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Falloff"), &compat::ngRequireObject(ring)->hardness, .05f, .1f, 8);
    } else if (auto plane = std::dynamic_pointer_cast<DepthPlaneOperation>(op)) {
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Target"), &compat::ngRequireObject(plane)->targetDepth, .01f, -2, 2);
        float x = compat::ngRequireObject(plane)->center.x;
        if (drawFloatInput(compat::RetainedString::fromStaticLiteral("Center X"), x, 1, -maximum, maximum, compat::RetainedString::fromStaticLiteral("%.0f"))) {
            auto action = action::DepthOperationListChangeAction::createSource([&] {
                return std::tuple{keepAlive, selectedOperationEditor};
            });
            compat::ngRequireObject(plane)->center = {x, compat::ngRequireObject(plane)->center.y}; pushSelectedOperationEdit(action);
        }
        float y = compat::ngRequireObject(plane)->center.y;
        if (drawFloatInput(compat::RetainedString::fromStaticLiteral("Center Y"), y, 1, -maximum, maximum, compat::RetainedString::fromStaticLiteral("%.0f"))) {
            auto action = action::DepthOperationListChangeAction::createSource([&] {
                return std::tuple{keepAlive, selectedOperationEditor};
            });
            compat::ngRequireObject(plane)->center = {compat::ngRequireObject(plane)->center.x, y}; pushSelectedOperationEdit(action);
        }
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Radius X"), &compat::ngRequireObject(plane)->radiusX, 1, 1, 1000, compat::RetainedString::fromStaticLiteral("%.0f"));
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Radius Y"), &compat::ngRequireObject(plane)->radiusY, 1, 1, 1000, compat::RetainedString::fromStaticLiteral("%.0f"));
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Angle"), &compat::ngRequireObject(plane)->angle, 1, -180, 180, compat::RetainedString::fromStaticLiteral("%.0f"));
        editSelectedFloat(compat::RetainedString::fromStaticLiteral("Flatten"), &compat::ngRequireObject(plane)->flattenStrength, .01f, 0, 1);
    }
    const auto duplicate = core::translation::ngTranslateCString("Duplicate");
    if (ImGui::Button(duplicate.data(), ImVec2(0, 0))) duplicateSelectedOperation();
    ImGui::SameLine();
    const auto remove = core::translation::ngTranslateCString("Delete");
    if (ImGui::Button(remove.data(), ImVec2(0, 0))) deleteSelectedOperation();
}
void DepthMeshEditor::drawOperationOptions() {
    [[maybe_unused]] const auto keepAlive = weak_from_this().lock();
    text("Depth Deformers"); bool hasOperations = false;
    const auto retained = editors;
    for (const auto [grid, editor] : retained) {
        if (!operations.findRetained(editor)) continue;
        const auto list = sourceValue(operations, editor, 591); if (list.empty()) continue; hasOperations = true;
        const auto name = compat::ngRequireObject(compat::ngRequireObject(editor)->getTarget())->getName();
        const std::array<core::translation::TypedFormatArgument, 1> nameArguments{
            core::translation::ngFormatArgument(name)};
        auto formattedName = core::translation::ngFormatTyped("%s", nameArguments);
        const compat::SourceCString nameCString{std::string_view(formattedName)};
        compat::detail::ngSourceCStringPostconditions(formattedName, nameCString.data());
        ImGui::Text(nameCString.data());
        for (std::size_t i = 0; i < list.size(); ++i) {
            const auto op = list[i]; const bool selected = editor == selectedOperationEditor && std::ptrdiff_t(i) == selectedOperationIndex;
            auto operationLabel = compat::ngRequireObject(op)->label();
            auto valueLabel = compat::ngRequireObject(op)->valueLabel();
            const std::array<core::translation::TypedFormatArgument, 4> values{
                core::translation::ngFormatArgument(std::move(operationLabel)),
                core::translation::ngFormatArgument(std::move(valueLabel)),
                core::translation::ngFormatArgument(static_cast<void*>(editor.get())), core::translation::ngFormatArgument(i)};
            auto label = core::translation::ngFormatTyped("%s  %s###depth_operation_%s_%s", values);
            const compat::SourceCString labelCString{std::string_view(label)};
            compat::detail::ngSourceCStringPostconditions(label, labelCString.data());
            if (ImGui::Selectable(labelCString.data(), selected, ImGuiSelectableFlags_None, ImVec2(0, 20))) {
                selectedOperationEditor = editor; selectedOperationIndex = std::ptrdiff_t(i);
            }
        }
    }
    if (!hasOperations) text("No depth deformers."); ImGui::Separator(); drawSelectedOperationEditor();
}
}
