#pragma once
#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/math/mesh.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/ext/nodes/excamera_math.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/compat/phobos_index_sort.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <nijigenerate/viewport/common/spline.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/base.hpp>
#include <algorithm>
#include <bit>
#include <tuple>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace nijigenerate::viewport::vertex::mesheditor::drawable {
using nicxlive::core::math::Vec3Array;
using nicxlive::core::math::Vec4;
using nicxlive::core::nodes::Drawable;
using nicxlive::core::nodes::MeshData;
using actions::mesheditor::DeformationAction;
using namespace core::dbg;

namespace detail {
template<class Operation> decltype(auto) allocateBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

template<class Callback> auto retainMirrorCallback(Callback callback) {
    static_assert(std::is_nothrow_move_constructible_v<Callback>);
    static_assert(std::is_nothrow_copy_constructible_v<Callback>);
    using Function = std::function<void(std::uint32_t)>;
    return allocateBytes([&] {
        // These two callbacks only capture this and its actual shared owner.
        // Constructing the box never invokes their Source bodies.
        return std::allocate_shared<const Function>(
            nicxlive::core::d_runtime::SourceAllocator<Function>{}, std::move(callback));
    });
}
} // namespace detail

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::substituteMeshVertices(MeshVertex* vertex) { compat::ngRequireObject(mesh)->vertices.append(vertex); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::setTarget(NodeRef value) {
    const auto keepAlive = weak_from_this().lock();
    const auto drawable = std::dynamic_pointer_cast<Drawable>(value);
    if (!drawable) return;
    Base::setTarget(value);
    transform = value ? value->transform().matrix() : Mat4::identity();
    auto& sourceMesh = drawable->Drawable::getMesh();
    const auto sourceMeshOwner = drawable->mesh;
    mesh = common::mesh::IncMesh::create(sourceMesh, sourceMeshOwner);
    refreshMesh();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::resetMesh() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto currentMesh = mesh;
    compat::ngRequireObject(currentMesh)->reset();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::refreshMesh() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    {
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->refresh();
    }
    if (previewingTriangulation()) {
        const auto currentMesh = mesh;
        previewMesh = compat::ngRequireObject(currentMesh)->autoTriangulate();
    } else previewMesh.reset();
    updateMirrorSelected();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::importMesh(MeshData& data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    {
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->import_(data);
    }
    const auto currentMesh = mesh;
    compat::ngRequireObject(currentMesh)->refresh();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::mergeMesh(MeshData& data, Mat4 matrix) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    {
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->merge_(data, matrix);
    }
    const auto currentMesh = mesh;
    compat::ngRequireObject(currentMesh)->refresh();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyOffsets(Vec2Array) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Vec2Array IncMeshEditorOneFor<T, mode>::getOffsets() { return {}; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyToTarget() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    common::mesh::GridAxes axes;
    if (common::mesh::isGrid(compat::ngRequireObject(mesh)->vertices, axes)) {
        const auto count = compat::ngRequireObject(mesh)->vertices.size();
        auto sorted = detail::allocateBytes([&] { return std::vector<std::size_t>(count); });
        for (std::size_t i = 0; i < sorted.size(); ++i) sorted[i] = i;
        const auto original = compat::ngRequireObject(mesh)->vertices;
        compat::phobos_index_sort::sort<16>(sorted, [&original](std::size_t ai, std::size_t bi) {
            auto* a = original[ai]; auto* b = original[bi];
            return compat::ngRequireObject(a)->position.y < compat::ngRequireObject(b)->position.y ||
                (compat::ngRequireObject(a)->position.y == compat::ngRequireObject(b)->position.y &&
                    compat::ngRequireObject(a)->position.x < compat::ngRequireObject(b)->position.x);
        }, "Failed to sort range of type Tuple!(ulong, \"index\", MeshVertex*, \"value\")[]");
        // Keep the source enumerate(...).assocArray evaluation even though its
        // result is not subsequently consumed by this original.
        core::selector::SelectorMap<std::size_t, std::size_t> indexMap;
        for (std::size_t i = 0; i < sorted.size(); ++i) indexMap[i] = sorted[i];
        VertexPointers reordered;
        for (auto index : sorted) reordered.append(original[index]);
        compat::ngRequireObject(mesh)->vertices = std::move(reordered);
    }
    core::math::mesh::applyMeshToTarget(target, compat::ngRequireObject(mesh)->vertices, &mesh);
    if (!axes.empty()) compat::ngRequireObject(target)->Drawable::getMesh().gridAxes = axes;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyPreview() {
    mesh = previewMesh;
    previewMesh.reset();
    previewTriangulate = false;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::pushDeformAction() {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction && compat::ngRequireObject(compat::ngRequireObject(editorAction)->action)->dirty) {
        {
            const auto currentAction = editorAction;
            compat::ngRequireObject(currentAction)->updateNewState();
        }
        core::actionstack::incActionPush(editorAction);
        editorAction.reset();
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
std::uint64_t IncMeshEditorOneFor<T, mode>::getVertexFromPoint(Vec2 point) {
    const auto currentMesh = mesh;
    return compat::ngRequireObject(currentMesh)->getVertexFromPoint(point, viewport::base::viewportZoom);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
compat::Slice<float> IncMeshEditorOneFor<T, mode>::getVerticesInBrush(Vec2 point, BrushRef brush) {
    const auto currentMesh = mesh;
    return compat::ngRequireObject(currentMesh)->getVerticesInBrush(point, brush);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::removeVertexAt(Vec2 point) {
    const auto currentMesh = mesh;
    compat::ngRequireObject(currentMesh)->removeVertexAt(point, viewport::base::viewportZoom);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::removeVertex(ImGuiIO*, bool selectedOnly) {
    const auto keepAlive = shared_from_this();
    if (!selectedOnly || isSelected(vtxAtMouse)) {
        foreachMirror(detail::retainMirrorCallback(
            [this, keepAlive](std::uint32_t axis) { removeVertexAt(mirror(axis, mousePos)); }));
        refreshMesh();
        vertexMapDirty = true;
        selected.resize(0);
        updateMirrorSelected();
        maybeSelectOne = UINT64_MAX;
        vtxAtMouse = UINT64_MAX;
        return true;
    }
    return false;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::addVertex(ImGuiIO* io) {
    const auto keepAlive = shared_from_this();
    const auto offset = compat::ngRequireObject(mesh)->vertices.size();
    if (isOnMirror(mousePos, meshEditAOE)) placeOnMirror(mousePos, meshEditAOE);
    else foreachMirror(detail::retainMirrorCallback([this, keepAlive](std::uint32_t axis) {
        auto vertex = core::math::mesh::ngCreateMeshVertexSource([&] {
            return mirror(axis, mousePos);
        });
        substituteMeshVertices(vertex.get());
    }));
    refreshMesh();
    vertexMapDirty = true;
    if (compat::ngRequireObject(io)->KeyCtrl) selectOne(compat::ngRequireObject(mesh)->vertices.size() - 1);
    else selectOne(offset);
    return true;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::updateChanged(bool changed) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (changed) compat::ngRequireObject(mesh)->changed = true;
    if (compat::ngRequireObject(mesh)->changed) {
        if (previewingTriangulation()) {
            const auto currentMesh = mesh;
            previewMesh = compat::ngRequireObject(currentMesh)->autoTriangulate();
        }
        compat::ngRequireObject(mesh)->changed = false;
    }
    return changed;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::addMeshVertex(MeshVertex* vertex) { compat::ngRequireObject(mesh)->vertices.append(vertex); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
std::int32_t IncMeshEditorOneFor<T, mode>::indexOfMesh(MeshVertex* vertex) {
    for (std::size_t i = 0; i < compat::ngRequireObject(mesh)->vertices.size(); ++i)
        if (compat::ngRequireObject(mesh)->vertices[i] == vertex) return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i));
    return -1;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::insertMeshVertex(std::int32_t index, MeshVertex* vertex) {
    const auto position = static_cast<std::size_t>(index);
    const auto size = compat::ngRequireObject(mesh)->vertices.size();
    // Phobos std.array.insertInPlace grows first, then checks array[pos..oldLen].
    compat::ngRequireObject(mesh)->vertices.resize(size + 1);
    const auto grownLength = compat::ngRequireObject(mesh)->vertices.size();
    if (position > size || size > grownLength)
        throw compat::ngStaticError<compat::ArraySliceError>(position, size, grownLength,
            compat::RetainedString::fromStaticLiteral("std/array.d"), 1463);
    const auto tail = compat::ngRequireObject(mesh)->vertices.slice(position, size);
    for (auto i = tail.size(); i; --i) compat::ngRequireObject(mesh)->vertices.set(position + i, tail[i - 1]);
    compat::ngRequireObject(mesh)->vertices.set(position, vertex);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::removeMeshVertex(MeshVertex* vertex) {
    const auto currentMesh = mesh;
    compat::ngRequireObject(currentMesh)->remove(vertex);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::moveMeshVertex(MeshVertex* vertex, Vec2 position) { compat::ngRequireObject(vertex)->position = position; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::isPointOver(Vec2 point) {
    const auto currentMesh = mesh;
    return compat::ngRequireObject(currentMesh)->isPointOverVertex(point, viewport::base::viewportZoom);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Indices IncMeshEditorOneFor<T, mode>::getInRect(Vec2, Vec2, std::uint32_t group) {
    auto indices = [this, group] {
        const auto currentMesh = mesh;
        return compat::ngRequireObject(currentMesh)->getInRect(selectOrigin, mousePos, group);
    }();
    return Indices::retainedView(indices.retentionOwner(), indices.data(), indices.size());
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Indices IncMeshEditorOneFor<T, mode>::filterVertices(VertexFilter filter) {
    const auto keepAlive = weak_from_this().lock();
    Indices result;
    const auto input = compat::ngRequireObject(mesh)->vertices;
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto* vertex = input[i];
        const auto callable = compat::ngRequireObject(filter);
        if (!*callable) compat::ngFailNullAccess(false);
        if ((*callable)(vertex)) result.append(i);
    }
    return result;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::VertexPointers IncMeshEditorOneFor<T, mode>::getVerticesByIndex(Indices indices, bool removeNull) {
    VertexPointers result;
    for (auto index : indices) {
        if (index < compat::ngRequireObject(mesh)->vertices.size()) result.append(compat::ngRequireObject(mesh)->vertices[index]);
        else if (!removeNull) result.append(nullptr);
    }
    return result;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::createPathTarget() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::ngRequireObject(getPath())->createTarget(mesh, Mat4::identity());
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Mat4 IncMeshEditorOneFor<T, mode>::updatePathTarget() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return compat::ngRequireObject(getPath())->updateTarget(mesh, selected);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::resetPathTarget() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::ngRequireObject(getPath())->resetTarget(mesh);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::remapPathTarget(PathRef& path, Mat4) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto currentPath = path;
    compat::ngRequireObject(currentPath)->remapTarget(mesh, Mat4::identity());
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::hasAction() { return editorAction != nullptr; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::updateAddVertexAction(MeshVertex* vertex) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (editorAction) {
        const auto currentAction = compat::ngRequireObject(editorAction)->action;
        compat::ngRequireObject(currentAction)->addVertex(vertex);
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::clearAction() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (editorAction) {
        const auto currentAction = compat::ngRequireObject(editorAction)->action;
        compat::ngRequireObject(currentAction)->clear();
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::markActionDirty() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (editorAction) {
        const auto currentAction = compat::ngRequireObject(editorAction)->action;
        compat::ngRequireObject(currentAction)->markAsDirty();
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getDeformActionImpl(bool reset) {
    const auto keepAlive = weak_from_this().lock();
    if (reset) pushDeformAction();
    bool create = !editorAction;
    if (!create) {
        const auto currentAction = compat::ngRequireObject(editorAction)->action;
        create = !compat::ngRequireObject(currentAction)->isApplyable();
    }
    if (create) {
        auto action = DeformationAction::createSource([&] {
            const auto name = compat::ngRequireObject(target)->getName();
            const auto label = detail::allocateBytes([&] {
                return actions::ActionText(compat::RetainedString(name));
            });
            const auto actionTarget = target;
            return std::tuple{label, actionTarget, compat::SharedDelegate<void()>{}};
        });
        const auto currentTool = [this] {
            const auto currentTools = tools;
            const auto entry = currentTools.find(toolMode);
            if (entry == currentTools.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/vertex/mesheditor/drawable.d"), 300);
            return entry->second;
        }();
        const auto wrapperTarget = target;
        editorAction = compat::ngRequireObject(currentTool)->editorAction(wrapperTarget, action);
    } else if (reset) {
        const auto currentAction = editorAction;
        compat::ngRequireObject(currentAction)->clear();
    }
    return editorAction;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getDeformAction() { return getDeformActionImpl(false); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getCleanDeformAction() { return getDeformActionImpl(true); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::forceResetAction() { editorAction.reset(); }

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::draw(CameraRef camera) {
    const auto keepAlive = shared_from_this();
    const auto trans = Mat4::identity();
    if (vtxAtMouse != UINT64_MAX && !isSelecting) {
        const auto one = getVerticesByIndex(Indices{vtxAtMouse}, true);
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->drawPointSubset(one, {1, 1, 1, 0.3f}, trans, 15);
    }
    if (previewMesh) {
        {
            const auto currentPreview = previewMesh;
            compat::ngRequireObject(currentPreview)->drawLines(trans, {0.7f, 0.7f, 0, 1});
        }
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->drawPoints(trans);
    } else {
        common::mesh::Markers markers;
        if (const auto drawable = std::dynamic_pointer_cast<Drawable>(getTarget())) {
            const auto links = drawable->weldedLinks;
            for (const auto& welded : links) {
                if (welded.indices.size() == compat::ngRequireObject(mesh)->vertices.size()) {
                    compat::Slice<std::ptrdiff_t> indices;
                    for (std::size_t i = 0; i < welded.indices.size(); ++i)
                        if (welded.indices[i] != std::ptrdiff_t(-1)) indices.append(static_cast<std::ptrdiff_t>(i));
                    if (!indices.empty()) markers.append({indices, {0, 0.7f, 0.7f, 1}});
                }
            }
        }
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->draw(trans, vertexColor, edgeColor, markers);
    }
    if (groupId != 0) {
        VertexPointers subset;
        for (auto* vertex : compat::ngRequireObject(mesh)->vertices) if (compat::ngRequireObject(vertex)->groupId != groupId) subset.append(vertex);
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->drawPointSubset(subset, {0.5f, 0.5f, 0.5f, 1}, trans);
    }
    if (!selected.empty()) {
        // The source chooses the branch before the virtual selection getter,
        // then reads mesh after that getter has returned.
        const bool selecting = isSelecting && !mutateSelection;
        const auto selectedVertices = getVerticesByIndex(selected, true);
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->drawPointSubset(selectedVertices,
            selecting ? Vec4{0.6f, 0, 0, 1} : Vec4{1, 0, 0, 1}, trans);
    }
    if (!mirrorSelected.empty()) {
        const auto mirroredVertices = getVerticesByIndex(mirrorSelected, true);
        const auto currentMesh = mesh;
        compat::ngRequireObject(currentMesh)->drawPointSubset(mirroredVertices, {1, 0, 1, 1}, trans);
    }
    if (isSelecting) {
        inDbgSetBuffer(common::ngCreateRectBuffer(selectOrigin, mousePos));
        inDbgDrawLines(!mutateSelection ? Vec4{1, 0, 0, 1} :
            invertSelection ? Vec4{0, 1, 1, 0.8f} : Vec4{0, 1, 0, 0.8f}, trans);
        if (!newSelected.empty()) {
            const bool invert = mutateSelection && invertSelection;
            const auto newSelectedVertices = getVerticesByIndex(newSelected, true);
            const auto currentMesh = mesh;
            compat::ngRequireObject(currentMesh)->drawPointSubset(newSelectedVertices,
                invert ? Vec4{1, 0, 1, 1} : Vec4{1, 0, 0, 1}, trans);
        }
    }
    const Vec2 size = compat::ngRequireObject(camera)->getRealSize();
    const Vec2 position = compat::ngRequireObject(camera)->position;
    Vec3Array axes;
    if (mirrorHoriz) axes.append(common::ngCreateLineBuffer(
        {mirrorOrigin.x, -size.y - position.y}, {mirrorOrigin.x, size.y - position.y}));
    if (mirrorVert) axes.append(common::ngCreateLineBuffer(
        {-size.x - position.x, mirrorOrigin.y}, {size.x - position.x, mirrorOrigin.y}));
    if (!axes.empty()) {
        inDbgSetBuffer(axes);
        inDbgDrawLines({0.8f, 0, 0.8f, 1}, trans);
    }
    if (tools.contains(toolMode)) {
        const auto retainedTools = tools;
        const auto entry = retainedTools.find(toolMode);
        if (entry == retainedTools.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/vertex/mesheditor/drawable.d"), 417);
        const auto tool = entry->second;
        compat::ngRequireObject(tool)->draw(camera, keepAlive);
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::adjustPathTransform() {
    const auto keepAlive = weak_from_this().lock();
    const auto trans = Mat4::multiply(ext::nodes::excamera_detail::inverse(
        target ? compat::ngRequireObject(target)->transform().matrix() : transform), transform);
    const auto adjust = [this, trans](PathRef& path) -> PathRef& {
        for (std::int32_t i = 0; static_cast<std::size_t>(i) < compat::ngRequireObject(path)->points.size();
                i = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(i) + 1u)) {
            const auto index = static_cast<std::size_t>(i);
            const auto point = compat::ngRequireObject(path)->points[index].position;
            const auto moved = core::math::ngTransformHomogeneousPoint(trans, {point.x, point.y, 0});
            compat::ngRequireObject(path)->points[index].position = {moved.x, moved.y};
        }
        {
            const auto currentPath = path;
            compat::ngRequireObject(currentPath)->update();
        }
        remapPathTarget(path, Mat4::identity());
        return path;
    };
    if (getPath()) {
        // LDC 1.41 gen/toir.cpp:538-539 forms the ordinary field destination
        // before evaluating the rhs at 581. Retain both actual field owners.
        if (compat::ngRequireObject(getPath())->target) {
            const auto destinationOwner = compat::ngRequireObject(getPath());
            auto& destination = destinationOwner->target;
            const auto sourceOwner = compat::ngRequireObject(getPath());
            auto& source = sourceOwner->target;
            destination = adjust(source);
        }
        auto path = getPath();
        setPath(adjust(path));
    }
    const auto moved = core::math::ngTransformHomogeneousPoint(trans, {lastMousePos.x, lastMousePos.y, 0});
    lastMousePos = {moved.x, moved.y};
    transform = compat::ngRequireObject(target)->transform().matrix();
    forceResetAction();
}
}
