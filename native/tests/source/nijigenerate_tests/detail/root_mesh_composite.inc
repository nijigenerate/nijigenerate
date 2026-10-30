// Faithful body of regression.d12352-12387; include once outside namespaces.
#include "root_mesh_composite.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/part.hpp>
#include <cstdint>
#include <memory>

namespace nijigenerate_tests::regression::detail {
void testMeshCompositeTopologyScopeWorkflow() {
    using namespace nijigenerate::commands::vertex::define_mesh;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::PathDeformer;
    resetCase();

    auto editScope = ngOpenActionStackScope(ActionStackScopeUnit::VertexEdit);
    nijigenerate::compat::ngRunWithScopeExit([&] {
        require(ngActionStackScopeActive(ActionStackScopeUnit::VertexEdit),
            RetainedString::fromStaticLiteral("mesh composite should open VertexEdit scope"));
        auto part = newMeshPart(RetainedString::fromStaticLiteral("mesh-composite-part"));
        auto grid = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        grid->setName(NullableString::fromStaticLiteral("mesh-composite-grid"));
        auto path = PathDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
        path->setName(NullableString::fromStaticLiteral("mesh-composite-path"));

        auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
        ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(part)});
        struct MeshLiteralFrame {
            Slice<float> vertices;
            Slice<std::uint16_t> indices;
            void prepare(bool includeIndices) {
                if (vertices.isNull()) vertices = Slice<float>{0, 0, 30, 0, 30, 30, 0, 30};
                if (includeIndices && indices.isNull()) indices = Slice<std::uint16_t>{0, 1, 2, 0, 2, 3};
            }
        } mesh;
        struct MeshVerticesArgument {
            MeshLiteralFrame& frame;
            operator Slice<float>() && { frame.prepare(false); return frame.vertices; }
        };
        struct MeshIndicesArgument {
            MeshLiteralFrame& frame;
            operator Slice<std::uint16_t>() && { frame.prepare(true); return frame.indices; }
        };
        require(std::allocate_shared<DefineMeshCommand>(SourceAllocator<DefineMeshCommand>{},
            MeshVerticesArgument{mesh}, MeshIndicesArgument{mesh})->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("mesh composite should define Part mesh"));
        ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(grid)});
        struct GridLiteralFrame {
            Slice<float> x;
            Slice<float> y;
            void prepare(bool includeY) {
                if (x.isNull()) x = Slice<float>{-20, 0, 20};
                if (includeY && y.isNull()) y = Slice<float>{-10, 10};
            }
        } axes;
        struct GridAxisArgument {
            GridLiteralFrame& frame;
            bool second;
            operator Slice<float>() && {
                frame.prepare(second);
                return second ? frame.y : frame.x;
            }
        };
        require(std::allocate_shared<DefineGridCommand>(SourceAllocator<DefineGridCommand>{},
            GridAxisArgument{axes, false}, GridAxisArgument{axes, true})->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("mesh composite should define GridDeformer grid"));
        ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(path)});
        struct PathVerticesArgument {
            operator Slice<float>() && { return Slice<float>{-10, 0, 0, 10, 10, 0}; }
        };
        require(std::allocate_shared<DefineVerticesCommand>(SourceAllocator<DefineVerticesCommand>{},
            PathVerticesArgument{})->run(ctx)->succeeded,
            RetainedString::fromStaticLiteral("mesh composite should define PathDeformer vertices"));

        incActionUndo();
        require(path->getVertices().size() != 3,
            RetainedString::fromStaticLiteral("mesh composite undo should restore previous path topology only"));
        require(grid->getVertices().size() == 6 && part->getMesh().vertices.size() == 4,
            RetainedString::fromStaticLiteral("mesh composite undo path should preserve prior grid and part topology"));
        incActionRedo();
        require(path->getVertices().size() == 3,
            RetainedString::fromStaticLiteral("mesh composite redo should restore path topology"));
        require(ngActionStackScopeActive(ActionStackScopeUnit::VertexEdit),
            RetainedString::fromStaticLiteral("mesh composite undo/redo should keep edit scope active"));
    }, [&] {
        if (editScope->isActive()) editScope->close();
    });
}
} // namespace nijigenerate_tests::regression::detail
