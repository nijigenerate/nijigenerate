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
}
std::size_t ngRefineWeldingSeams(std::shared_ptr<nicxlive::core::nodes::Drawable> first,
    std::shared_ptr<nicxlive::core::nodes::Drawable> second) {
    if (refiningWeldingMesh || !first || !second || first == second) return 0;
    const auto coarse = first->getVertices().size() <= second->getVertices().size() ? first : second;
    const auto fine = coarse == first ? second : first;
    if (!std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(coarse) ||
        !std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(fine)) return 0;
    using Vec2 = nicxlive::core::math::Vec2;
    using Vec2Array = nicxlive::core::math::Vec2Array;
    compat::Slice<Vec2> coarseWorld, coarseLocal, fineWorld;
    const auto coarseVertices = coarse->getVertices();
    for (std::size_t i = 0; i < coarseVertices.size(); ++i) {
        const auto vertex = coarseVertices.at(i);
        coarseLocal.append(vertex);
        const auto world = core::math::ngTransformHomogeneousPoint(coarse->transform().matrix(),
            {vertex.x, vertex.y, 0});
        coarseWorld.append({world.x, world.y});
    }
    const auto fineVertices = fine->getVertices();
    for (std::size_t i = 0; i < fineVertices.size(); ++i) {
        const auto vertex = fineVertices.at(i);
        const auto world = core::math::ngTransformHomogeneousPoint(fine->transform().matrix(),
            {vertex.x, vertex.y, 0});
        fineWorld.append({world.x, world.y});
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
    auto& sourceMesh = coarse->nicxlive::core::nodes::Drawable::getMesh();
    const auto sourceMeshOwner = coarse->mesh;
    auto mesh = common::mesh::IncMesh::create(sourceMesh, sourceMeshOwner);
    mesh->vertices.resize(0);
    compat::Slice<compat::Vec3u> triangles;
    for (std::size_t i = 0; i < refinement.indices.size(); i += 3)
        triangles.append({refinement.indices[i], refinement.indices[i + 1], refinement.indices[i + 2]});
    Vec2Array positions(refinement.vertices.size());
    for (std::size_t i = 0; i < refinement.vertices.size(); ++i) positions.set(i, refinement.vertices[i]);
    mesh->importVertsAndTris(positions, triangles);
    for (std::size_t i = 0,count=coarse->getVertices().size(); i < count; ++i)
        compat::ngRequireObject(mesh->vertices[i])->originalIndex = i;
    mesh->refresh();
    core::math::mesh::applyMeshToTarget(coarse, mesh->vertices, &mesh, true);
    return refinement.addedVertices;
}

std::shared_ptr<base::IncMeshEditorOne> ngGetEditorFor(std::shared_ptr<nicxlive::core::nodes::Node> target) {
    const auto editor = viewport::model::deform::ngViewportModelDeformGetEditor();
    auto targetEditor = editor ? editor->getEditorFor(target) : nullptr;
    if (!targetEditor) targetEditor = tools::onetimedeform::ngGetArmedParameterEditorFor(target);
    return targetEditor;
}
}
