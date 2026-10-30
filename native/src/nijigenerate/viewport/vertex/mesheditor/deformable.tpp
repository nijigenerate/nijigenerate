#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/math/mesh.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/ext/nodes/path_vertices.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/compat/phobos_float_sort.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <nijigenerate/viewport/common/spline.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/curve.hpp>
#include <bit>
#include <tuple>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace nijigenerate::viewport::vertex::mesheditor::deformable {
using nicxlive::core::math::Vec3Array;
using nicxlive::core::math::Vec4;
using nicxlive::core::nodes::Deformable;
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
    try {
        return std::allocate_shared<const Function>(
            nicxlive::core::d_runtime::SourceAllocator<Function>{}, std::move(callback));
    } catch (const std::bad_alloc&) {
        // The two source callbacks capture only a pointer and its shared owner.
        // Their construction cannot invoke the callback or throw a user exception.
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline void ngRemoveFirst(core::math::mesh::MeshVertexArray& vertices, core::math::mesh::MeshVertex* vertex) {
    for (std::size_t i = 0; i < vertices.size(); ++i) if (vertices[i] == vertex) {
        for (std::size_t j = i + 1; j < vertices.size(); ++j) vertices.set(j - 1, vertices[j]);
        vertices.resize(vertices.size() - 1);
        return;
    }
}
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::substituteMeshVertices(MeshVertex* vertex) {
    const auto receiver = compat::ngRequireObject(deformable());
    ext::nodes::ngAppendDeformableVertex(*receiver, compat::ngRequireObject(vertex)->position);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::setTarget(NodeRef value) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto deform = std::dynamic_pointer_cast<Deformable>(value);
    if (!deform) return;
    Base::setTarget(value);
    vertices = common::mesheditor::operations::impl::toMVertices(
        common::mesheditor::operations::impl::getVertices(getTarget()));
    transform = value ? value->transform().matrix() : Mat4::identity();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::resetMesh() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    vertices = common::mesheditor::operations::impl::toMVertices(
        common::mesheditor::operations::impl::getVertices(getTarget()));
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::refreshMesh() { updateMirrorSelected(); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::importMesh(MeshData& data) {
    vertices.resize(0);
    const auto points = data.vertices.toArray();
    core::math::mesh::MeshVertexArray imported;
    imported.resize(points.size());
    for (std::size_t i = 0; i < points.size(); ++i)
        imported.set(i, core::math::mesh::ngCreateMeshVertex(points[i]));
    // Commit the original single array-append descriptor only after the entire
    // RHS is materialized and the genuine pointer-array append has succeeded.
    auto combined = vertices;
    combined.append(imported);
    vertices = combined;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::mergeMesh(MeshData& data, Mat4) {
    const auto points = data.vertices.toArray();
    core::math::mesh::MeshVertexArray imported;
    imported.resize(points.size());
    for (std::size_t i = 0; i < points.size(); ++i)
        imported.set(i, core::math::mesh::ngCreateMeshVertex(points[i]));
    auto combined = vertices;
    combined.append(imported);
    vertices = combined;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyOffsets(Vec2Array) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Vec2Array IncMeshEditorOneFor<T, mode>::getOffsets() { return {}; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyToTarget() {
    core::math::mesh::applyMeshToTarget(target, vertices, static_cast<MeshRef*>(nullptr));
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyPreview() {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
std::uint64_t IncMeshEditorOneFor<T, mode>::getVertexFromPoint(Vec2 point) {
    return core::math::vertex::getVertexFromPoint(vertices, point, viewport::base::viewportZoom);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
compat::Slice<float> IncMeshEditorOneFor<T, mode>::getVerticesInBrush(Vec2 point, BrushRef brush) {
    return core::math::vertex::getVerticesInBrush(vertices, point, brush);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::removeVertexAt(Vec2 point) {
    core::math::vertex::removeVertexAt(vertices, point, viewport::base::viewportZoom,
        [this](MeshVertex* vertex) { detail::ngRemoveFirst(vertices, vertex); });
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::updateChanged(bool changed) {
    if (changed) this->changed = true;
    return changed;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::addMeshVertex(MeshVertex* vertex) { vertices.append(vertex); vertexMapDirty = true; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::removeMeshVertex(MeshVertex* vertex) { detail::ngRemoveFirst(vertices, vertex); vertexMapDirty = true; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::isPointOver(Vec2 point) {
    return core::math::vertex::isPointOverVertex(vertices, point, viewport::base::viewportZoom);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Indices IncMeshEditorOneFor<T, mode>::getInRect(Vec2, Vec2, std::uint32_t group) {
    auto result = core::math::vertex::getInRect(vertices, selectOrigin, mousePos, group);
    return Indices::retainedView(result.retentionOwner(), result.data(), result.size());
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::createPathTarget() {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Mat4 IncMeshEditorOneFor<T, mode>::updatePathTarget() { return Mat4::identity(); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::resetPathTarget() {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::remapPathTarget(PathRef&, Mat4) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::draw(CameraRef camera) {
    const auto keepAlive = shared_from_this();
    const auto trans = Mat4::identity();
    using common::mesheditor::operations::impl::drawPointSubset;
    Vec3Array points;
    points.append({0, 0, 0});
    inDbgSetBuffer(points);
    inDbgPointsSize(10); inDbgDrawPoints({0, 0, 0, 1}, trans);
    inDbgPointsSize(6); inDbgDrawPoints({0.5f, 1, 0.5f, 1}, trans);
    points.resize(vertices.size());
    const auto pointCount = vertices.size();
    for (std::size_t i = 0; i < pointCount; ++i) {
        const auto point = compat::ngRequireObject(vertices[i])->position;
        points.set(i, {point.x, point.y, 0});
    }
    if (!points.empty()) {
        if (auto path = std::dynamic_pointer_cast<nicxlive::core::nodes::PathDeformer>(target)) {
            const auto input = vertices;
            auto positionsAoS = detail::allocateBytes([&] { return std::vector<Vec2>(input.size()); });
            for (std::size_t i = 0; i < input.size(); ++i)
                positionsAoS[i] = compat::ngRequireObject(input[i])->position;
            Vec2Array positions;
            positions.assign(positionsAoS);
            auto curve = path->createCurve(positions);
            const auto color = edgeColor;
            const auto sourcePoint = [curve](float position) {
                if (auto* source = dynamic_cast<nicxlive::core::nodes::SourceCurveEvaluation*>(curve.get()))
                    return source->pointSource(position);
                return compat::ngRequireObject(curve)->point(position);
            };
            if (!compat::ngRequireObject(curve)->controlPoints().empty()) {
                Vec3Array lines;
                for (int i = 1; i < 100; ++i) {
                    const auto a = sourcePoint(static_cast<float>((i - 1) / 100.0));
                    lines.append({a.x, a.y, 0});
                    const auto b = sourcePoint(static_cast<float>(i / 100.0));
                    lines.append({b.x, b.y, 0});
                }
                if (!lines.empty()) { inDbgSetBuffer(lines); inDbgDrawLines(color, trans); }
            }
        } else if (std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(target)) {
            const auto input = vertices;
            auto positionsAoS = detail::allocateBytes([&] { return std::vector<Vec2>(input.size()); });
            for (std::size_t i = 0; i < input.size(); ++i)
                positionsAoS[i] = compat::ngRequireObject(input[i])->position;
            Vec2Array base;
            base.assign(positionsAoS);
            if (base.size() >= 4) {
                const auto aos = base.toArray();
                auto xs = detail::allocateBytes([&] { return std::vector<float>(aos.size()); });
                for (std::size_t i = 0; i < aos.size(); ++i) xs[i] = aos[i].x;
                auto ys = detail::allocateBytes([&] { return std::vector<float>(aos.size()); });
                for (std::size_t i = 0; i < aos.size(); ++i) ys[i] = aos[i].y;
                compat::phobos_float_sort::sort(xs); compat::phobos_float_sort::sort(ys);
                const auto uniq = [](std::vector<float>& values) {
                    std::vector<float> result;
                    for (float value : values) if (result.empty() || !(result.back() == value))
                        detail::allocateBytes([&] { result.push_back(value); });
                    values = std::move(result);
                };
                uniq(xs); uniq(ys);
                const auto columns = xs.size(), rows = ys.size();
                if (columns >= 2 && rows >= 2 && columns * rows == base.size()) {
                    Vec3Array lines;
                    for (std::size_t y = 0; y < rows; ++y) for (std::size_t x = 0; x < columns; ++x) {
                        const auto index = y * columns + x;
                        const auto point = aos[index];
                        if (x + 1 < columns) {
                            const auto right = aos[index + 1];
                            lines.append({point.x, point.y, 0}); lines.append({right.x, right.y, 0});
                        }
                        if (y + 1 < rows) {
                            const auto down = aos[index + columns];
                            lines.append({point.x, point.y, 0}); lines.append({down.x, down.y, 0});
                        }
                    }
                    if (!lines.empty()) { inDbgSetBuffer(lines); inDbgDrawLines(edgeColor, trans); }
                }
            }
        }
        inDbgSetBuffer(points);
        inDbgPointsSize(10); inDbgDrawPoints({0, 0, 0, 1}, trans);
        inDbgPointsSize(6); inDbgDrawPoints(vertexColor, trans);
    }
    if (vtxAtMouse != UINT64_MAX && !isSelecting)
        drawPointSubset(getVerticesByIndex(Indices{vtxAtMouse}, true), {1, 1, 1, 0.3f}, trans, 15);
    if (!selected.empty()) {
        const auto color = isSelecting && !mutateSelection ? Vec4{0.6f, 0, 0, 1} : Vec4{1, 0, 0, 1};
        const auto subset = getVerticesByIndex(selected, true);
        drawPointSubset(subset, color, trans);
    }
    if (!mirrorSelected.empty()) drawPointSubset(getVerticesByIndex(mirrorSelected, true), {1, 0, 1, 1}, trans);
    if (isSelecting) {
        inDbgSetBuffer(common::ngCreateRectBuffer(selectOrigin, mousePos));
        inDbgDrawLines(!mutateSelection ? Vec4{1, 0, 0, 1} :
            invertSelection ? Vec4{0, 1, 1, 0.8f} : Vec4{0, 1, 0, 0.8f}, trans);
        if (!newSelected.empty()) {
            const auto color = mutateSelection && invertSelection ? Vec4{1, 0, 1, 1} : Vec4{1, 0, 0, 1};
            const auto subset = getVerticesByIndex(newSelected, true);
            drawPointSubset(subset, color, trans);
        }
    }
    const Vec2 size = compat::ngRequireObject(camera)->getRealSize();
    const Vec2 position = compat::ngRequireObject(camera)->position;
    Vec3Array axes;
    if (mirrorHoriz) axes.append(common::ngCreateLineBuffer(
        {mirrorOrigin.x, -size.y - position.y}, {mirrorOrigin.x, size.y - position.y}));
    if (mirrorVert) axes.append(common::ngCreateLineBuffer(
        {-size.x - position.x, mirrorOrigin.y}, {size.x - position.x, mirrorOrigin.y}));
    if (!axes.empty()) { inDbgSetBuffer(axes); inDbgDrawLines({0.8f, 0, 0.8f, 1}, trans); }
    if (tools.contains(toolMode)) {
        const auto retainedTools = tools;
        const auto entry = retainedTools.find(toolMode);
        if (entry == retainedTools.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/vertex/mesheditor/deformable.d"), 438);
        const auto tool = entry->second;
        compat::ngRequireObject(tool)->draw(camera, keepAlive);
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::pushDeformAction() {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction && compat::ngRequireObject(editorAction->action)->dirty) {
        compat::ngRequireObject(editorAction)->updateNewState();
        core::actionstack::incActionPush(editorAction);
        editorAction.reset();
    }
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
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
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::addVertex(ImGuiIO* io) {
    const auto keepAlive = shared_from_this();
    const auto offset = vertices.size();
    if (isOnMirror(mousePos, meshEditAOE)) placeOnMirror(mousePos, meshEditAOE);
    else foreachMirror(detail::retainMirrorCallback([this, keepAlive](std::uint32_t axis) {
        auto vertex = core::math::mesh::ngCreateMeshVertexSource([&] {
            return mirror(axis, mousePos);
        });
        substituteMeshVertices(vertex.get());
    }));
    refreshMesh();
    vertexMapDirty = true;
    if (compat::ngRequireObject(io)->KeyCtrl) selectOne(vertices.size() - 1);
    else selectOne(offset);
    return true;
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
std::int32_t IncMeshEditorOneFor<T, mode>::indexOfMesh(MeshVertex* vertex) {
    for (std::size_t i = 0; i < vertices.size(); ++i)
        if (vertices[i] == vertex) return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i));
    return -1;
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::insertMeshVertex(std::int32_t index, MeshVertex* vertex) {
    const auto position = static_cast<std::size_t>(index);
    const auto size = vertices.size();
    vertices.resize(size + 1);
    if (position > size) throw compat::ngStaticError<compat::ArraySliceError>(
        position, size, vertices.size(),
        compat::RetainedString::fromStaticLiteral("std/array.d"), 1463);
    for (std::size_t i = size; i > position; --i) vertices.set(i, vertices[i - 1]);
    vertices.set(position, vertex);
    vertexMapDirty = true;
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::moveMeshVertex(MeshVertex* vertex, Vec2 position) {
    if (!vertex) compat::ngFailNullAccess(true);
    vertex->position = position;
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Indices IncMeshEditorOneFor<T, mode>::filterVertices(VertexFilter filter) {
    const auto keepAlive = weak_from_this().lock();
    Indices result;
    const auto input = vertices;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto vertex = input[i];
        if (!filter || !*filter) compat::ngFailNullAccess(false);
        if ((*filter)(vertex)) result.append(i);
    }
    return result;
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::hasAction() { return editorAction != nullptr; }

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::updateAddVertexAction(MeshVertex* vertex) {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction) compat::ngRequireObject(editorAction->action)->addVertex(vertex);
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::clearAction() {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction) compat::ngRequireObject(editorAction->action)->clear();
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::markActionDirty() {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction) compat::ngRequireObject(editorAction->action)->markAsDirty();
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getDeformActionImpl(bool reset) {
    const auto keepAlive = weak_from_this().lock();
    if (reset) pushDeformAction();
    if (!editorAction || !compat::ngRequireObject(editorAction->action)->isApplyable()) {
        auto action = DeformationAction::createSource([&] {
            const auto label = compat::RetainedString(compat::ngRequireObject(target)->getName());
            const auto actionTarget = target;
            return std::tuple{actions::ActionText(label), actionTarget, compat::SharedDelegate<void()>{}};
        });
        const auto tool = [this] {
            const auto retainedTools = tools;
            const auto entry = retainedTools.find(toolMode);
            if (entry == retainedTools.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/vertex/mesheditor/deformable.d"), 269);
            return entry->second;
        }();
        const auto actionTarget = target;
        editorAction = compat::ngRequireObject(tool)->editorAction(actionTarget, action);
    } else if (reset) compat::ngRequireObject(editorAction)->clear();
    return editorAction;
}

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getDeformAction() { return getDeformActionImpl(false); }

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getCleanDeformAction() { return getDeformActionImpl(true); }

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::forceResetAction() { editorAction.reset(); }

template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit && std::derived_from<T, nicxlive::core::nodes::Deformable> && !std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::adjustPathTransform() {
    const auto keepAlive = weak_from_this().lock();
    const auto trans = Mat4::multiply(core::math::source_matrix::inverse(
        target ? target->transform().matrix() : transform), transform);
    const auto adjust = [this, trans](PathRef& path) -> PathRef& {
        for (std::int32_t i = 0; static_cast<std::size_t>(i) < compat::ngRequireObject(path)->points.size();
                i = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) + 1u)) {
            const auto index = static_cast<std::size_t>(i);
            const auto point = compat::ngRequireObject(path)->points[index].position;
            const auto moved = core::math::ngTransformHomogeneousPoint(trans, {point.x, point.y, 0});
            compat::ngRequireObject(path)->points[index].position = {moved.x, moved.y};
        }
        compat::ngRequireObject(path)->update();
        remapPathTarget(path, Mat4::identity());
        return path;
    };
    if (getPath()) {
        if (compat::ngRequireObject(getPath())->target) {
            const auto destination = compat::ngRequireObject(getPath());
            const auto source = compat::ngRequireObject(getPath());
            const PathRef adjusted = adjust(source->target);
            destination->target = adjusted;
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

