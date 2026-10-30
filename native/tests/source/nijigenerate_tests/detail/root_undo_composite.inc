// Faithful body of existing regression.d testUndoCompositeCrossFamilyStackWorkflow.
// Include once outside namespaces, after the module's other helper definitions.
#include "root_undo_composite.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/inspector/part_commands.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/part.hpp>
#include <param/binding_impl.hpp>
#include <cstdint>
#include <memory>

namespace nijigenerate_tests::regression::detail {
std::shared_ptr<nijigenerate::ext::ExParameter> new2DParameter(nijigenerate::compat::RetainedString name);
std::shared_ptr<nicxlive::core::param::ValueParameterBinding> newValueBinding(
    std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::shared_ptr<nicxlive::core::nodes::Node> node,
    nijigenerate::compat::RetainedString name, bool setZero);

void testUndoCompositeCrossFamilyStackWorkflow() {
    using namespace nijigenerate::commands::inspector::apply_node;
    using namespace nijigenerate::commands::node::node;
    using namespace nijigenerate::commands::vertex::define_mesh;
    using namespace nijigenerate::core::actionstack;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    resetCase();

    auto nodeCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    nodeCtx->puppet(nijigenerate::project::incActivePuppet());
    auto node = constructSourceCommand<AddNodeCommand>(
        RetainedString::fromStaticLiteral("Node"))->run(nodeCtx)->created[0];
    nodeCtx->nodes(Context::Nodes{node});
    struct RenameArrayArgument {
        operator Slice<RetainedString>() && {
            return Slice<RetainedString>{RetainedString::fromStaticLiteral("UndoCompositeNode")};
        }
    };
    require(std::allocate_shared<SetNodeNameCommand>(SourceAllocator<SetNodeNameCommand>{},
        RenameArrayArgument{})->run(nodeCtx)->succeeded,
        RetainedString::fromStaticLiteral("undo composite should rename node"));

    auto param = new2DParameter(RetainedString::fromStaticLiteral("UndoCompositeParam"));
    auto binding = newValueBinding(param, node, RetainedString::fromStaticLiteral("transform.t.x"), true);
    binding->setValue({1, 1}, 4.0f);
    auto part = newMeshPart(RetainedString::fromStaticLiteral("undo-composite-part"));
    auto meshCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    meshCtx->nodes(Context::Nodes{std::static_pointer_cast<Node>(part)});
    struct MeshLiteralFrame {
        Slice<float> vertices;
        Slice<std::uint16_t> indices;
        void prepare(bool includeIndices) {
            if (vertices.isNull()) vertices = Slice<float>{0, 0, 10, 0, 10, 10, 0, 10};
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
        MeshVerticesArgument{mesh}, MeshIndicesArgument{mesh})->run(meshCtx)->succeeded,
        RetainedString::fromStaticLiteral("undo composite should define mesh"));

    auto inspector = nijigenerate::panels::inspector::ngCreateInspectorDeferred<PartInspector, Part>(
        [&] { return Selection<Part>{part}; }, [] { return ModelEditSubMode::Layout; });
    meshCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(inspector)});
    require(constructSourceCommand<PartOpacityCommandBase>(0.25f)->run(meshCtx)->succeeded,
        RetainedString::fromStaticLiteral("undo composite should apply inspector property"));

    incActionUndo();
    require(near(part->opacity, 1.0f) && part->getMesh().vertices.size() == 4,
        RetainedString::fromStaticLiteral("undo composite first undo should revert inspector only"));
    incActionUndo();
    require(part->getMesh().vertices.size() == 3 && binding->isSet({1, 1}),
        RetainedString::fromStaticLiteral("undo composite second undo should revert mesh only and keep parameter binding"));
    incActionRedo();
    require(part->getMesh().vertices.size() == 4,
        RetainedString::fromStaticLiteral("undo composite redo should restore mesh"));
    incActionRedo();
    require(near(part->opacity, 0.25f),
        RetainedString::fromStaticLiteral("undo composite redo should restore inspector property"));
}
} // namespace nijigenerate_tests::regression::detail
