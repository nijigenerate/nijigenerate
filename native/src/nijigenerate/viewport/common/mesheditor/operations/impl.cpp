#include "impl.hpp"
#include <nijigenerate/actions/mesh.hpp>
#include <nijigenerate/core/math/mesh_apply.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/core/math/welding.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/viewport/common/mesheditor/package.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/pathdeform.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform_types.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nodes/part.hpp>
#include <functional>
#include <tuple>

namespace nijigenerate::viewport::common::mesheditor::operations::impl {
namespace detail {
base::IncMeshEditorOne::PathRef getPath(const std::shared_ptr<tools::base::Tool>& tool) {
    const auto pathTool = std::dynamic_pointer_cast<tools::pathdeform::PathDeformTool>(tool);
    return compat::ngRequireObject(pathTool)->path;
}
void setPath(const std::shared_ptr<tools::base::Tool>& tool, base::IncMeshEditorOne::PathRef path) {
    const auto pathTool = std::dynamic_pointer_cast<tools::pathdeform::PathDeformTool>(tool);
    compat::ngRequireObject(pathTool)->setPath(path);
}
}

base::IncMeshEditorOne::VertexPointers IncMeshEditorOneDrawable::vertices() {
    return compat::ngRequireObject(mesh)->vertices;
}
void IncMeshEditorOneDrawable::forEachEdge(EdgeVisitor visitor) {
    const auto keepAlive = shared_from_this();
    const auto input = compat::ngRequireObject(mesh)->vertices;
    for (auto* vertex : input) {
        const auto connections = compat::ngRequireObject(vertex)->connections.duplicate();
        for (auto* other : connections) {
#ifndef NDEBUG
            if (other == vertex) throw compat::ngStaticError<compat::AssertError>("Assertion failure",
                "source/nijigenerate/viewport/common/mesheditor/operations/impl.d", 328);
#endif
            if (std::less<MeshVertex*>{}(other, vertex)) continue;
            const auto callable=compat::ngRequireObject(visitor);
            if(!*callable)compat::ngFailNullAccess(false);
            (*callable)(vertex, other);
        }
    }
}
std::shared_ptr<actions::mesh::MeshDisconnectAction> IncMeshEditorOneDrawable::newMeshDisconnectAction() {
    const auto keepAlive = shared_from_this();
    return actions::mesh::MeshDisconnectAction::createSource([&] {
        const auto name = compat::ngRequireObject(getTarget())->getName();
        const auto label = [&] {
            try {
                return actions::ActionText(compat::RetainedString(name));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        return std::tuple{
            label,
            keepAlive, mesh, actions::mesh::MeshUpdate{}};
    });
}

namespace {
thread_local bool refiningWeldingMesh = false;
void applyWeldingRefinement(const std::shared_ptr<nicxlive::core::nodes::Drawable>& drawable,
    const core::math::welding::WeldingMeshRefinement& refinement) {
    if (!refinement.addedVertices && !refinement.topologyChanged) return;
    auto& sourceMesh = drawable->nicxlive::core::nodes::Drawable::getMesh();
    auto mesh = common::mesh::IncMesh::create(sourceMesh, drawable->mesh);
    mesh->vertices.resize(0);
    compat::Slice<compat::Vec3u> triangles;
    for (std::size_t i = 0; i < refinement.indices.size(); i += 3)
        triangles.append({refinement.indices[i], refinement.indices[i + 1], refinement.indices[i + 2]});
    nicxlive::core::math::Vec2Array positions(refinement.vertices.size());
    for (std::size_t i = 0; i < refinement.vertices.size(); ++i) positions.set(i, refinement.vertices[i]);
    mesh->importVertsAndTris(positions, triangles);
    for (std::size_t i = 0, count = drawable->getVertices().size(); i < count; ++i)
        compat::ngRequireObject(mesh->vertices[i])->originalIndex = i;
    mesh->refresh();
    core::math::mesh::applyMeshToTarget(drawable, mesh->vertices, &mesh, true);
}

void applyWeldingSurface(const std::shared_ptr<nicxlive::core::nodes::Drawable>& drawable,
    const core::math::welding::WeldingMeshRefinement& refinement) {
    if (!refinement.addedVertices && !refinement.topologyChanged) return;
    using nicxlive::core::nodes::MeshData;
    using core::math::mesh::MeshVertex;
    // Keep the triangles produced by inserting the missing seam vertices.
    auto& source = drawable->nicxlive::core::nodes::Drawable::getMesh();
    struct ExplicitFaces {
        MeshData* source;
        compat::Slice<MeshVertex*> vertices;
        MeshData data;
        MeshData export_() const { return data; }
        bool isBasedOn(MeshData& mesh) const { return source == &mesh; }
    } prepared{&source, {}, {}};
    std::vector<MeshVertex> samples(refinement.vertices.size());
    prepared.vertices.resize(samples.size());
    prepared.data.origin = source.origin;
    prepared.data.vertices.resize(samples.size());
    prepared.data.uvs.resize(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        auto& vertex = samples[i];
        vertex.position = refinement.vertices[i];
        if (i < source.vertices.size()) vertex.originalIndex = i;
        prepared.vertices[i] = &vertex;
        prepared.data.vertices.set(i, vertex.position);
        prepared.data.uvs.set(i, vertex.position);
    }
    prepared.data.indices.resize(refinement.indices.size());
    std::copy(refinement.indices.begin(), refinement.indices.end(), prepared.data.indices.begin());
    core::math::mesh::applyMeshToTarget(drawable, prepared.vertices, &prepared, true);
}
}
std::size_t ngRefineWeldingSeams(std::shared_ptr<nicxlive::core::nodes::Drawable> first,
    std::shared_ptr<nicxlive::core::nodes::Drawable> second) {
    if (refiningWeldingMesh || !first || !second || first == second) return 0;
    const auto coarse = first->getVertices().size() <= second->getVertices().size() ? first : second;
    const auto fine = coarse == first ? second : first;
    if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(coarse) ||
        !std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(fine)) return 0;
    using Vec2 = nicxlive::core::math::Vec2;
    compat::Slice<Vec2> coarseWorld, coarseLocal, fineWorld;
    const auto coarseVertices = coarse->getVertices();
    for (std::size_t i = 0; i < coarseVertices.size(); ++i) {
        const auto vertex = coarseVertices.at(i);
        coarseLocal.append(vertex);
        const auto world = core::math::ngTransformHomogeneousPoint(coarse->transform().matrix(),
            {vertex.x, vertex.y, 0});
        const Vec2 coarseWorldPoint{world.x, world.y};
        coarseWorld.append(coarseWorldPoint);
    }
    const auto fineVertices = fine->getVertices();
    for (std::size_t i = 0; i < fineVertices.size(); ++i) {
        const auto vertex = fineVertices.at(i);
        const auto world = core::math::ngTransformHomogeneousPoint(fine->transform().matrix(),
            {vertex.x, vertex.y, 0});
        const Vec2 fineWorldPoint{world.x, world.y};
        fineWorld.append(fineWorldPoint);
    }
    const auto& fineIndices = fine->nicxlive::core::nodes::Drawable::getMesh().indices;
    const auto& coarseIndices = coarse->nicxlive::core::nodes::Drawable::getMesh().indices;
    const auto refinement = core::math::welding::ngRefineWeldingMesh({fineWorld.data(), fineWorld.size()},
        {fineIndices.data(), fineIndices.size()}, {coarseWorld.data(), coarseWorld.size()},
        {coarseLocal.data(), coarseLocal.size()},
        {coarseIndices.data(), coarseIndices.size()}, core::math::source_matrix::inverse(coarse->transform().matrix()));
    if (!refinement.addedVertices) return 0;
    refiningWeldingMesh = true;
    struct Reset { ~Reset() { refiningWeldingMesh = false; } } reset;
    applyWeldingRefinement(coarse, refinement);
    return refinement.addedVertices;
}

WeldingOverlap ngPrepareWeldingOverlap(
    const std::shared_ptr<nicxlive::core::nodes::Drawable>& first,
    const std::shared_ptr<nicxlive::core::nodes::Drawable>& second,
    const nicxlive::core::math::Mat4& rootToWorld,
    std::optional<std::array<float, 4>> bounds, std::array<std::size_t, 2> originalVertexCounts) {
    using nicxlive::core::math::Vec2;
    const auto worldToRoot = core::math::source_matrix::inverse(rootToWorld);
    const auto firstToRoot = worldToRoot * first->transform().matrix();
    const auto secondToRoot = worldToRoot * second->transform().matrix();
    const auto inRoot = [](const auto& drawable, const auto& transform) {
        std::vector<Vec2> points;
        const auto& vertices = drawable->getVertices();
        points.reserve(vertices.size());
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto v = vertices.at(i);
            const auto p = core::math::ngTransformHomogeneousPoint(transform, {v.x, v.y, 0});
            points.push_back({p.x, p.y});
        }
        return points;
    };
    const auto a = inRoot(first, firstToRoot), b = inRoot(second, secondToRoot);
    const auto& ai = first->nicxlive::core::nodes::Drawable::getMesh().indices;
    const auto& bi = second->nicxlive::core::nodes::Drawable::getMesh().indices;
    auto shared = core::math::welding::ngRefineWeldingOverlap(a, {ai.data(), ai.size()},
        b, {bi.data(), bi.size()}, bounds, originalVertexCounts);
    const auto refine = [](const auto& drawable, const auto& toRoot, const auto& rootVertices, auto& refinement) {
        const auto toLocal = core::math::source_matrix::inverse(toRoot);
        const auto& original = drawable->getVertices();
        for (std::size_t i = 0; i < refinement.vertices.size(); ++i) {
            const auto v = refinement.vertices[i];
            if (i < rootVertices.size() && v.x == rootVertices[i].x && v.y == rootVertices[i].y) {
                refinement.vertices[i] = original.at(i);
                continue;
            }
            const auto p = core::math::ngTransformHomogeneousPoint(toLocal, {v.x, v.y, 0});
            refinement.vertices[i] = {p.x, p.y};
        }
        applyWeldingSurface(drawable, refinement);
    };
    refine(first, firstToRoot, a, shared.first);
    refine(second, secondToRoot, b, shared.second);
    return {std::move(shared.correspondence), shared.first.addedVertices + shared.second.addedVertices,
        shared.first.topologyChanged || shared.second.topologyChanged};
}

std::shared_ptr<base::IncMeshEditorOne> ngGetEditorFor(std::shared_ptr<nicxlive::core::nodes::Node> target) {
    const auto editor = viewport::model::deform::ngViewportModelDeformGetEditor();
    auto targetEditor = editor ? editor->getEditorFor(target) : nullptr;
    if (!targetEditor) targetEditor = tools::onetimedeform::ngGetArmedParameterEditorFor(target);
    return targetEditor;
}
}
