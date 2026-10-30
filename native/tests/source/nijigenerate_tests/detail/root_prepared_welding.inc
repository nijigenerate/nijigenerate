// Faithful body of regression.d1742-1800; include once outside namespaces.
#include "root_prepared_welding.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/node/welding_commands.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <fmt/serialize.hpp>
#include <common/shared_array.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <cstdint>
#include <memory>

namespace nijigenerate_tests::regression::detail {
void testPreparedWeldingMesh() {
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::commands::node::welding::AddWeldingCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::core::actionstack::incActionUndo;
    using nijigenerate::project::incActivePuppet;
    using nijigenerate::viewport::common::mesheditor::operations::impl::ngRefineWeldingSeams;
    using nicxlive::core::common::SharedArray;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::fmt::inToJson;
    using nicxlive::core::math::Vec2;
    using nicxlive::core::math::Vec2Array;
    using nicxlive::core::nodes::MeshData;
    using nicxlive::core::nodes::Node;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    // Lower the original scalar-array equality without a tolerance conversion.
    const auto equalIndices = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    resetCase();
    auto source = newMeshPart(text("prepared-seam"));
    auto target = newMeshPart(text("dense-seam"));
    MeshData coarse;
    coarse.vertices = Vec2Array(SharedArray<Vec2>{{0, 0}, {20, 0}, {0, 20}});
    coarse.uvs = Vec2Array(SharedArray<Vec2>{{0, 0}, {1, 0}, {0, 1}});
    coarse.indices = SharedArray<std::uint16_t>{0, 1, 2};
    source->rebuffer(coarse);
    MeshData dense;
    dense.vertices = Vec2Array(SharedArray<Vec2>{{0, 0}, {10, 0}, {20, 0}, {0, 20}});
    dense.uvs = Vec2Array(SharedArray<Vec2>{{0, 0}, {0.5f, 0}, {1, 0}, {0, 1}});
    dense.indices = SharedArray<std::uint16_t>{0, 1, 3, 1, 2, 3};
    target->rebuffer(dense);
    // inToJson(T)(T item) receives a shallow MeshData value in the original.
    auto sourceMeshValue = source->getMesh();
    auto sourceMeshBefore = inToJson(sourceMeshValue);
    auto targetMeshValue = target->getMesh();
    auto targetMeshBefore = inToJson(targetMeshValue);
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(incActivePuppet());
    ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(source)});
    require(constructSourceCommand<AddWeldingCommand>(target, 0)->run(ctx)->succeeded,
        text("Welding on a prepared mesh must succeed"));
    require(source->getVertices().size() == 3 && equalIndices(source->getMesh().indices, coarse.indices),
        text("Prepared welding must not insert vertices after correspondence prediction"));
    auto authoredIndices = source->weldedLinks[0].indices;
    auto expectedIndices = SharedArray<int>{0, 2, 3};
    require(equalIndices(authoredIndices, expectedIndices),
        text("Prepared welding must retain predicted correspondences"));
    incActionUndo();
    require(source->weldedLinks.empty() && target->weldedLinks.empty(),
        text("Prepared welding must remain undoable"));
    require(constructSourceCommand<AddWeldingCommand>(target, 0)->run(ctx)->succeeded,
        text("Ordinary welding must register existing mesh correspondences"));
    auto currentSourceMesh = source->getMesh();
    const auto sourceUnchanged = inToJson(currentSourceMesh) == sourceMeshBefore;
    const auto targetUnchanged = sourceUnchanged && [&] {
        auto currentTargetMesh = target->getMesh();
        return inToJson(currentTargetMesh) == targetMeshBefore;
    }();
    require(sourceUnchanged && targetUnchanged,
        text("Ordinary welding must preserve both meshes, including missing seam samples"));
    require(ngRefineWeldingSeams(source, target) > 0,
        text("AutoRig mesh preparation must explicitly insert missing seam samples"));
    require(source->getVertices().size() == 4, text("Explicit mesh preparation must refine the seam"));

    // A second shoulder can refine the shared body and invalidate a previously recorded link size.
    auto left = newMeshPart(text("second-shoulder"));
    MeshData leftMesh;
    leftMesh.vertices = Vec2Array(SharedArray<Vec2>{{0, 0}, {5, 0}, {20, 0}, {0, 20}});
    leftMesh.uvs = Vec2Array(SharedArray<Vec2>{{0, 0}, {0.25f, 0}, {1, 0}, {0, 1}});
    leftMesh.indices = SharedArray<std::uint16_t>{0, 1, 3, 1, 2, 3};
    left->rebuffer(leftMesh);
    auto previousCount = source->weldedLinks[0].indices.size();
    require(ngRefineWeldingSeams(source, left) > 0, text("Second shoulder must add a shared body sample"));
    require(source->weldedLinks[0].indices.size() > previousCount,
        text("Shared body refinement must update existing welding indices"));
    auto preparedCount = source->getVertices().size();
    require(constructSourceCommand<AddWeldingCommand>(target, 0)->run(ctx)->succeeded,
        text("First shoulder must match the fully prepared shared mesh"));
    auto predicted = source->weldedLinks[0].indices.duplicate();
    require(constructSourceCommand<AddWeldingCommand>(left, 0)->run(ctx)->succeeded,
        text("Second prepared shoulder must succeed"));
    require(source->getVertices().size() == preparedCount && equalIndices(source->weldedLinks[0].indices, predicted),
        text("Creating the second prepared link must preserve the first shoulder prediction"));
}
}
