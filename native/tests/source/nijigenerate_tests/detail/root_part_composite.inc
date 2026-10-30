// Faithful bodies of regression.d12163-12227 and 12316-12350.
// Include once outside namespaces.
#include "root_part_composite.hpp"
#include "regression_internal.hpp"
#include "source_strings.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/inspector/part_commands.hpp>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <nijigenerate/commands/inspector/griddeform_commands.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <nijigenerate/commands/puppet/file_commands.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_rmdir.hpp>
#include <nijigenerate/compat/phobos_temp_directory.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace nijigenerate_tests::regression::detail {
std::shared_ptr<nijigenerate::ext::ExParameter> new2DParameter(nijigenerate::compat::RetainedString name);
std::shared_ptr<nicxlive::core::nodes::Node> findNodeRecursive(
    std::shared_ptr<nicxlive::core::nodes::Node> root, std::string_view name);
std::shared_ptr<nicxlive::core::param::Parameter> findParameter(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::string_view name);

void testPartCompositeInspectorMeshRoundTrip() {
    using namespace nijigenerate::compat::phobos;
    using namespace nijigenerate::commands::inspector::apply_node;
    using namespace nijigenerate::commands::vertex::define_mesh;
    using namespace nijigenerate::commands::model::set_deform_binding;
    using namespace nijigenerate::commands::puppet::file;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec3;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    using nicxlive::core::param::DeformationParameterBinding;
    resetCase();

    auto part = newMeshPart(RetainedString::fromStaticLiteral("part-composite-target"));
    auto inspector = nijigenerate::panels::inspector::ngCreateInspectorDeferred<PartInspector, Part>(
        [&] { return Selection<Part>{part}; }, [] { return ModelEditSubMode::Layout; });
    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(part)});
    ctx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(inspector)});
    require(constructSourceCommand<PartTintCommandBase>(Vec3{0.2f, 0.4f, 0.6f})->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("part composite should apply tint"));
    require(constructSourceCommand<PartOpacityCommandBase>(0.55f)->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("part composite should apply opacity"));

    struct MeshLiteralFrame {
        Slice<float> vertices;
        Slice<std::uint16_t> indices;
        void prepare(bool includeIndices) {
            if (vertices.isNull()) vertices = Slice<float>{-10.0f, -10.0f, 10.0f, -10.0f,
                10.0f, 10.0f, -10.0f, 10.0f};
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
        RetainedString::fromStaticLiteral("part composite should replace mesh topology"));

    auto param = new2DParameter(RetainedString::fromStaticLiteral("PartCompositeParam"));
    auto bindCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    bindCtx->puppet(nijigenerate::project::incActivePuppet());
    bindCtx->parameters(Context::Parameters{param});
    bindCtx->nodes(Context::Nodes{std::static_pointer_cast<Node>(part)});
    bindCtx->keyPoint({2, 2});
    bindCtx->hasExplicitKeyPoint(true);
    struct DeformOffsetsArgument {
        operator Slice<float>() && { return Slice<float>{1, 0, 2, 0, 3, 0, 4, 0}; }
    };
    require(std::allocate_shared<SetDeformBindingCommand>(SourceAllocator<SetDeformBindingCommand>{},
        RetainedStringSlice(RetainedString::fromStaticLiteral("deform")), DeformOffsetsArgument{})->run(
            bindCtx)->succeeded,
        RetainedString::fromStaticLiteral("part composite should bind deformation after mesh change"));

    nijigenerate::core::actionstack::incActionUndo();
    require(param->getBinding(part, NullableString::fromStaticLiteral("deform")) == nullptr,
        RetainedString::fromStaticLiteral("part composite undo binding should keep prior mesh/property edits"));
    require(part->getMesh().vertices.size() == 4 && near(part->opacity, 0.55f),
        RetainedString::fromStaticLiteral("part composite undo binding should preserve previous commands"));
    nijigenerate::core::actionstack::incActionRedo();

    const std::array segments{ngTempDirectory(),
        RetainedString::fromStaticLiteral("nijigenerate-regression-part-composite")};
    const auto fixtureDir = ngBuildPath(segments);
    if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    ngMkdirRecurse(fixtureDir.view());
    nijigenerate::compat::ngRunWithScopeExit([&] {
        const std::array saveSegments{fixtureDir, RetainedString::fromStaticLiteral("part-composite")};
        const auto saveBase = ngBuildPath(saveSegments);
        const auto savePath = concatenateSourceStrings({saveBase, RetainedString::fromStaticLiteral(".inx")});
        require(constructSourceCommand<SaveFileCommand>(saveBase)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("part composite should save"));
        nijigenerate::project::incNewProject();
        require(constructSourceCommand<OpenFileCommand>(savePath)->run(
            std::allocate_shared<Context>(SourceAllocator<Context>{}))->succeeded,
            RetainedString::fromStaticLiteral("part composite should reopen"));

        auto loaded = std::dynamic_pointer_cast<Part>(findNodeRecursive(
            nijigenerate::project::incActivePuppet()->root, "part-composite-target"));
        require(loaded != nullptr && loaded->getMesh().vertices.size() > 0,
            RetainedString::fromStaticLiteral("part composite should preserve mesh target"));
        require(nearVec3(loaded->tint, Vec3{0.2f, 0.4f, 0.6f}) && near(loaded->opacity, 0.55f),
            RetainedString::fromStaticLiteral("part composite should preserve inspector properties"));
        auto loadedParam = findParameter(nijigenerate::project::incActivePuppet(), "PartCompositeParam");
        auto loadedBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
            nijigenerate::compat::ngRequireObject(loadedParam)->getBinding(loaded,
                NullableString::fromStaticLiteral("deform")));
        require(loadedBinding != nullptr && loadedBinding->getValue({2, 2}).vertexOffsets.size() ==
            loaded->getMesh().vertices.size(),
            RetainedString::fromStaticLiteral("part composite should preserve deformation binding length"));
    }, [&] {
        if (ngFileExists(fixtureDir.view())) ngRmdirRecurse(RetainedStringSlice(fixtureDir));
    });
}
void testInspectorCompositeNodePartMeshWorkflow() {
    using namespace nijigenerate::commands::inspector::apply_node;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::panels::inspector::ModelEditSubMode;
    using nijigenerate::panels::inspector::Selection;
    using nijigenerate::panels::inspector::TypedInspector;
    using nijigenerate::panels::inspector::ngCreateInspectorDeferred;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::Part;
    resetCase();

    auto node = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    node->setName(NullableString::fromStaticLiteral("inspector-composite-node"));
    auto nodeInspector = ngCreateInspectorDeferred<NodeApplyInspector, Node>(
        [&] { return Selection<Node>{node}; }, [] { return ModelEditSubMode::Layout; });
    auto nodeCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    nodeCtx->nodes(Context::Nodes{node});
    nodeCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(nodeInspector)});
    require(constructSourceCommand<TranslationXCommandBase>(15.0f)->run(nodeCtx)->succeeded,
        RetainedString::fromStaticLiteral("inspector composite should apply node translation"));

    auto part = newMeshPart(RetainedString::fromStaticLiteral("inspector-composite-part"));
    auto partInspector = ngCreateInspectorDeferred<PartInspector, Part>(
        [&] { return Selection<Part>{part}; }, [] { return ModelEditSubMode::Layout; });
    auto partCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    partCtx->nodes(Context::Nodes{std::static_pointer_cast<Node>(part)});
    partCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(partInspector)});
    require(constructSourceCommand<PartOpacityCommandBase>(0.35f)->run(partCtx)->succeeded,
        RetainedString::fromStaticLiteral("inspector composite should apply part opacity"));

    auto grid = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(NullableString::fromStaticLiteral("inspector-composite-grid"));
    auto gridInspector = ngCreateInspectorDeferred<GridInspector, GridDeformer>(
        [&] { return Selection<GridDeformer>{grid}; }, [] { return ModelEditSubMode::Layout; });
    auto gridCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    gridCtx->nodes(Context::Nodes{std::static_pointer_cast<Node>(grid)});
    gridCtx->inspectors(Context::Inspectors{std::static_pointer_cast<TypedInspector<Node>>(gridInspector)});
    require(constructSourceCommand<GridDeformDynamicCommandBase>(true)->run(gridCtx)->succeeded,
        RetainedString::fromStaticLiteral("inspector composite should apply grid dynamic"));

    nijigenerate::core::actionstack::incActionUndo();
    require(!grid->dynamic && near(part->opacity, 0.35f) && near(node->localTransform.translation.x, 15.0f),
        RetainedString::fromStaticLiteral("inspector composite undo should affect only latest inspector command"));
    nijigenerate::core::actionstack::incActionRedo();
    require(grid->dynamic,
        RetainedString::fromStaticLiteral("inspector composite redo should restore latest inspector command"));
}
} // namespace nijigenerate_tests::regression::detail
