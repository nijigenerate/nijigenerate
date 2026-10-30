#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <nijigenerate/viewport/common/spline.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/base.hpp>
#include <param/binding_impl.hpp>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>
#include <vector>

namespace nijigenerate::viewport::model::mesheditor::drawable {
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
} // namespace detail

template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
IncMeshEditorOneFor<T, mode>::IncMeshEditorOneFor() : Base(true) {}
// Topology edits and immediate application are intentionally empty in the source specialization.
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::substituteMeshVertices(MeshVertex*) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::updateTarget() {
    const auto keepAlive = weak_from_this().lock();
    const auto drawable = target;
    transform = compat::ngRequireObject(drawable)->getDynamicMatrix();
    vertices.resize(drawable->getVertices().size());
    const auto deformCount = drawable->deformation.size();
    auto& baseVertices = drawable->getVertices();
    baseVertices.forEachValue([&](std::size_t i, Vec2 vertex) {
        const Vec2 offset = i < deformCount ? drawable->deformation[i].toVector() : Vec2{0, 0};
        vertices.set(i, {vertex.x + offset.x, vertex.y + offset.y});
        return 0;
    });
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::importDeformation() {
    const auto keepAlive = weak_from_this().lock();
    const auto drawable = target;
    if (!drawable) return;
    deformation = drawable->deformation.dup();
    const auto parameter = project::incArmedParameter();
    const auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
        parameter ? parameter->getBinding(drawable,
            nicxlive::core::common::NullableString::fromStaticLiteral("deform")) : nullptr);
    if (!binding) deformation = drawable->deformation.dup();
    else {
        const auto index = parameter->findClosestKeypoint();
        const auto deform = binding->getValue(index);
        if (drawable->deformation.size() == deform.vertexOffsets.size()) {
            deformation.resize(drawable->deformation.size());
            drawable->deformation.forEachValue([&](std::size_t i, Vec2 value) {
                const auto offset = deform.vertexOffsets[i].toVector();
                deformation.set(i, {value.x - offset.x, value.y - offset.y});
                return 0;
            });
        }
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::setTarget(NodeRef value) {
    const auto keepAlive = weak_from_this().lock();
    const auto drawable = std::dynamic_pointer_cast<Drawable>(value);
    if (!drawable) return;
    // The source deliberately imports the previous target before switching.
    importDeformation();
    Base::setTarget(value);
    updateTarget();
    auto& sourceMesh = drawable->Drawable::getMesh();
    const auto sourceMeshOwner = drawable->mesh;
    mesh = common::mesh::IncMesh::create(sourceMesh, sourceMeshOwner);
    refreshMesh();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::resetMesh() { compat::ngRequireObject(mesh)->reset(); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::refreshMesh() {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(mesh)->refresh();
    updateMirrorSelected();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::importMesh(MeshData& data) {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(mesh)->import_(data);
    compat::ngRequireObject(mesh)->refresh();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::mergeMesh(MeshData& data, Mat4 matrix) {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(mesh)->merge_(data, matrix);
    compat::ngRequireObject(mesh)->refresh();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyOffsets(Vec2Array offsets) { compat::ngRequireObject(mesh)->applyOffsets(offsets); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Vec2Array IncMeshEditorOneFor<T, mode>::getOffsets() { return compat::ngRequireObject(mesh)->getOffsets(); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyToTarget() {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::applyPreview() {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::pushDeformAction() {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction && compat::ngRequireObject(editorAction->action)->dirty) {
        compat::ngRequireObject(editorAction)->updateNewState();
        core::actionstack::incActionPush(editorAction);
        editorAction.reset();
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
std::uint64_t IncMeshEditorOneFor<T, mode>::getVertexFromPoint(Vec2 position) {
    const auto count = vertices.size();
    for (std::size_t i = 0; i < count; ++i) {
        const auto vertex = vertices[i].toVector();
        const auto distance = std::abs(common::spline::detail::distance(vertex, position));
        if (distance < compat::ngRequireObject(mesh)->selectRadius / viewport::base::viewportZoom) return i;
    }
    return UINT64_MAX;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
compat::Slice<float> IncMeshEditorOneFor<T, mode>::getVerticesInBrush(Vec2 position, BrushRef brush) {
    const auto keepAlive = weak_from_this().lock();
    return compat::ngRequireObject(brush)->weightsAt(position, vertices);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::removeVertexAt(Vec2) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::removeVertex(ImGuiIO*, bool) { return false; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::addVertex(ImGuiIO*) { return false; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::updateChanged(bool changed) { return changed; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::addMeshVertex(MeshVertex*) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::insertMeshVertex(std::int32_t, MeshVertex*) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::removeMeshVertex(MeshVertex*) {}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::moveMeshVertex(MeshVertex* vertex, Vec2 position) {
    if (!vertex) compat::ngFailNullAccess(true);
    vertex->position = position;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::isPointOver(Vec2 position) {
    bool found = false;
    vertices.forEachValue([&](std::size_t, Vec2 vertex) {
        const auto distance = std::abs(common::spline::detail::distance(vertex, position));
        found = distance < compat::ngRequireObject(mesh)->selectRadius / viewport::base::viewportZoom;
        return found ? 1 : 0;
    });
    return found;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Indices IncMeshEditorOneFor<T, mode>::getInRect(Vec2 min, Vec2 max, std::uint32_t) {
    if (min.x > max.x) std::swap(min.x, max.x);
    if (min.y > max.y) std::swap(min.y, max.y);
    Indices matching;
    vertices.forEachValue([&](std::size_t i, Vec2 vertex) {
        if (min.x > vertex.x || min.y > vertex.y || max.x < vertex.x || max.y < vertex.y) return 0;
        matching.append(i);
        return 0;
    });
    return matching;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Indices IncMeshEditorOneFor<T, mode>::filterVertices(VertexFilter filter) {
    const auto keepAlive = weak_from_this().lock();
    Indices matching;
    MeshVertex vertex;
    vertices.forEachValue([&](std::size_t i, Vec2 position) {
        vertex.position = position;
        if (!filter || !*filter) compat::ngFailNullAccess(false);
        if ((*filter)(&vertex)) matching.append(i);
        return 0;
    });
    return matching;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::VertexPointers IncMeshEditorOneFor<T, mode>::getVerticesByIndex(Indices indices, bool removeNull) {
    VertexPointers result;
    for (auto index : indices) {
        if (index < compat::ngRequireObject(mesh)->vertices.size())
            result.append(compat::ngRequireObject(mesh)->vertices[index]);
        else if (!removeNull) result.append(nullptr);
    }
    return result;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::createPathTarget() {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(getPath())->createTarget(mesh, Mat4::identity(), &vertices);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::Mat4 IncMeshEditorOneFor<T, mode>::updatePathTarget() {
    const auto keepAlive = weak_from_this().lock();
    return compat::ngRequireObject(getPath())->updateTarget(mesh, selected, Mat4::identity(), deformation);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::resetPathTarget() {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(getPath())->resetTarget(mesh);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::remapPathTarget(PathRef& path, Mat4 matrix) {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(path)->remapTarget(mesh, matrix, &vertices);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
bool IncMeshEditorOneFor<T, mode>::hasAction() { return editorAction != nullptr; }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::updateAddVertexAction(MeshVertex* vertex) {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction) compat::ngRequireObject(editorAction->action)->addVertex(vertex);
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::clearAction() {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction) compat::ngRequireObject(editorAction->action)->clear();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::markActionDirty() {
    const auto keepAlive = weak_from_this().lock();
    if (editorAction) compat::ngRequireObject(editorAction->action)->markAsDirty();
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getDeformActionImpl(bool reset) {
    const auto keepAlive = weak_from_this().lock();
    if (reset) pushDeformAction();
    if (!editorAction || !compat::ngRequireObject(editorAction->action)->isApplyable()) {
        const auto action = DeformationAction::createSource([&] {
            const auto name = compat::ngRequireObject(target)->getName();
            const auto label = detail::allocateBytes([&] { return compat::RetainedString(name); });
            const auto actionTarget = target;
            return std::tuple{actions::ActionText(label), actionTarget, compat::SharedDelegate<void()>{}};
        });
        const auto currentTools = tools;
        const auto currentMode = toolMode;
        const auto found = currentTools.find(currentMode);
        if (found == currentTools.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/model/mesheditor/drawable.d"), 277);
        editorAction = compat::ngRequireObject(found->second)->editorAction(target, action);
    } else if (reset) compat::ngRequireObject(editorAction)->clear();
    return editorAction;
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getDeformAction() { return getDeformActionImpl(false); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
typename IncMeshEditorOneFor<T, mode>::ActionRef IncMeshEditorOneFor<T, mode>::getCleanDeformAction() { return getDeformActionImpl(true); }
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::forceResetAction() { editorAction.reset(); }

template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::draw(CameraRef camera) {
    const auto keepAlive = shared_from_this();
    const auto drawable = target;
    updateTarget();
    const auto trans = transform;
    const auto displayVertices = [this](Indices indices) {
        VertexPointers result;
        for (auto index : indices) if (index < vertices.size())
            result.append(core::math::mesh::ngCreateMeshVertexSource([&] {
                return vertices[index].toVector();
            }));
        return result;
    };
    compat::ngRequireObject(drawable)->drawMeshLines(edgeColor);
    Vec3Array points(vertices.size());
    // This local array owns its three real SDK lanes. Lower the source vecv
    // accesses through those lanes and the existing source Error providers;
    // Vec3Array::at/set currently use a C assertion instead of that protocol.
    const auto readPoint = [&points](std::size_t index) {
        nicxlive::core::math::detail::ngAssertVec2SourceIndex(index < points.size(), 139);
        const auto readLane = [index](const auto& lane) {
            if (index >= lane.size())
                throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::ArrayIndexError>(
                    index, lane.size(), nicxlive::core::d_runtime::RetainedString::fromStaticLiteral(
                        "nijilive/math/veca.d"), 467);
            return lane[index];
        };
        const auto x = readLane(points.x);
        const auto y = readLane(points.y);
        const auto z = readLane(points.z);
        return nicxlive::core::math::Vec3{x, y, z};
    };
    const auto writePoint = [&points](std::size_t index, nicxlive::core::math::Vec3 value) {
        nicxlive::core::math::detail::ngAssertVec2SourceIndex(index < points.size(), 139);
        const auto writeLane = [index](auto& lane, float component) {
            if (index >= lane.size())
                throw nicxlive::core::d_runtime::ngStaticError<nicxlive::core::d_runtime::ArrayIndexError>(
                    index, lane.size(), nicxlive::core::d_runtime::RetainedString::fromStaticLiteral(
                        "nijilive/math/veca.d"), 475);
            lane[index] = component;
        };
        writeLane(points.x, value.x);
        writeLane(points.y, value.y);
        writeLane(points.z, value.z);
    };
    const auto pointCount = vertices.size();
    for (std::size_t i = 0; i < pointCount; ++i) {
        const auto point = vertices[i].toVector();
        writePoint(i, {point.x, point.y, 0});
    }
    if (!points.empty()) {
        inDbgSetBuffer(points);
        inDbgPointsSize(10);
        inDbgDrawPoints({0, 0, 0, 1}, trans);
        std::vector<Indices> markers;
        if (drawable) {
            const auto links = drawable->weldedLinks;
            for (const auto& welded : links) {
                Indices indices;
                for (std::size_t i = 0; i < welded.indices.size(); ++i)
                    if (welded.indices[i] != std::ptrdiff_t(-1)) indices.append(i);
                if (!indices.empty()) detail::allocateBytes([&] { markers.push_back(indices); });
            }
        }
        for (const auto& indices : markers) {
            std::vector<nicxlive::core::math::Vec3> markerPoints;
            detail::allocateBytes([&] { markerPoints.reserve(indices.size()); });
            for (auto index : indices) {
                const auto point = readPoint(index);
                detail::allocateBytes([&] { markerPoints.push_back(point); });
            }
            Vec3Array marker;
            marker.assign(markerPoints);
            inDbgSetBuffer(marker);
            inDbgPointsSize(10);
            inDbgDrawPoints({0, 0.7f, 0.7f, 1}, trans);
        }
        inDbgSetBuffer(points);
        inDbgPointsSize(6);
        inDbgDrawPoints(vertexColor, trans);
    }
    if (groupId != 0) {
        VertexPointers subset;
        const auto groupVertices = compat::ngRequireObject(mesh)->vertices;
        for (auto* vertex : groupVertices)
            if (compat::ngRequireObject(vertex)->groupId != groupId) subset.append(vertex);
        compat::ngRequireObject(mesh)->drawPointSubset(subset, {0.6f, 0.6f, 0.6f, 1}, trans);
    }
    if (vtxAtMouse != UINT64_MAX && !isSelecting) {
        const auto one = displayVertices(Indices{vtxAtMouse});
        compat::ngRequireObject(mesh)->drawPointSubset(one, {1, 1, 1, 0.3f}, trans, 15);
    }
    if (!selected.empty()) {
        const Vec4 selectedColor = isSelecting && !mutateSelection ?
            Vec4{0.6f, 0, 0, 1} : Vec4{1, 0, 0, 1};
        const auto selectedVertices = displayVertices(selected);
        compat::ngRequireObject(mesh)->drawPointSubset(selectedVertices, selectedColor, trans);
    }
    if (!mirrorSelected.empty()) {
        const auto mirrorVertices = displayVertices(mirrorSelected);
        compat::ngRequireObject(mesh)->drawPointSubset(mirrorVertices, {1, 0, 1, 1}, trans);
    }
    if (isSelecting) {
        auto rect = common::ngCreateRectBuffer(selectOrigin, mousePos);
        inDbgSetBuffer(rect);
        inDbgDrawLines(!mutateSelection ? Vec4{1, 0, 0, 1} :
            invertSelection ? Vec4{0, 1, 1, 0.8f} : Vec4{0, 1, 0, 0.8f}, trans);
        if (!newSelected.empty()) {
            const auto newVertices = displayVertices(newSelected);
            compat::ngRequireObject(mesh)->drawPointSubset(newVertices,
                mutateSelection && invertSelection ? Vec4{1, 0, 1, 1} : Vec4{1, 0, 0, 1}, trans);
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
        const auto currentTools = tools;
        const auto currentMode = toolMode;
        const auto found = currentTools.find(currentMode);
        if (found == currentTools.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/model/mesheditor/drawable.d"), 419);
        compat::ngRequireObject(found->second)->draw(camera, keepAlive);
    }
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit && std::derived_from<T, nicxlive::core::nodes::Drawable>)
void IncMeshEditorOneFor<T, mode>::adjustPathTransform() {
    const auto keepAlive = weak_from_this().lock();
    const auto drawable = target;
    const auto basis = target ? compat::ngRequireObject(drawable)->getDynamicMatrix() : transform;
    const auto inverse = core::math::source_matrix::inverse(basis);
    const auto trans = Mat4::multiply(inverse, transform);
    importDeformation();
    const auto adjust = [this, trans](PathRef& path) -> PathRef& {
        compat::ngRequireObject(path)->update();
        remapPathTarget(path, trans);
        return path;
    };
    if (getPath()) {
        if (compat::ngRequireObject(getPath())->target) {
            // Assignment operand order is implementation-defined in D. Keep
            // this lowering's destination-first order, with two real getters.
            const auto destination = compat::ngRequireObject(getPath());
            const auto source = compat::ngRequireObject(getPath());
            const PathRef adjusted = adjust(source->target);
            destination->target = adjusted;
        }
        auto path = getPath();
        setPath(adjust(path));
    }
    const auto transformed = core::math::ngTransformHomogeneousPoint(trans, {lastMousePos.x, lastMousePos.y, 0});
    lastMousePos = {transformed.x, transformed.y};
    updateTarget();
    forceResetAction();
}
}

