// Faithful body of regression.d12389-12428; include once outside namespaces.
#include "root_deform_composite.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <param/binding_impl.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate_tests::regression::detail {
std::shared_ptr<nijigenerate::ext::ExParameter> new2DParameter(nijigenerate::compat::RetainedString name);

void testDeformCompositeGridPathBindingWorkflow() {
    using namespace nijigenerate::commands::vertex::define_mesh;
    using namespace nijigenerate::commands::model::set_deform_binding;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::GridDeformer;
    using nicxlive::core::nodes::Node;
    using nicxlive::core::nodes::PathDeformer;
    using nicxlive::core::param::DeformationParameterBinding;
    resetCase();

    auto grid = GridDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    grid->setName(NullableString::fromStaticLiteral("deform-composite-grid"));
    auto path = PathDeformer::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    path->setName(NullableString::fromStaticLiteral("deform-composite-path"));

    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(grid)});
    struct GridAxisFrame {
        Slice<float> x;
        Slice<float> y;
        void prepare(bool includeY) {
            if (x.isNull()) x = Slice<float>{-30.0f, 0.0f, 30.0f};
            if (includeY && y.isNull()) y = Slice<float>{-20.0f, 0.0f, 20.0f};
        }
    } axes;
    struct GridAxisArgument {
        GridAxisFrame& frame;
        bool second;
        operator Slice<float>() && {
            frame.prepare(second);
            return second ? frame.y : frame.x;
        }
    };
    require(std::allocate_shared<DefineGridCommand>(SourceAllocator<DefineGridCommand>{},
        GridAxisArgument{axes, false}, GridAxisArgument{axes, true})->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("deform composite should define grid topology"));
    ctx->nodes(Context::Nodes{std::static_pointer_cast<Node>(path)});
    struct PathVerticesArgument {
        operator Slice<float>() && {
            return Slice<float>{-15.0f, 0.0f, 0.0f, 15.0f, 15.0f, 0.0f};
        }
    };
    require(std::allocate_shared<DefineVerticesCommand>(SourceAllocator<DefineVerticesCommand>{},
        PathVerticesArgument{})->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("deform composite should define path topology"));

    auto param = new2DParameter(RetainedString::fromStaticLiteral("DeformCompositeParam"));
    auto gridCtx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    gridCtx->puppet(nijigenerate::project::incActivePuppet());
    gridCtx->parameters(Context::Parameters{param});
    gridCtx->nodes(Context::Nodes{std::static_pointer_cast<Node>(grid)});
    gridCtx->keyPoint({2, 2});
    gridCtx->hasExplicitKeyPoint(true);
    Slice<float> offsets;
    const auto vertexCount = grid->getVertices().size();
    for (std::size_t i = 0; i < vertexCount; ++i) {
        offsets.append(2.0f);
        offsets.append(-1.0f);
    }
    require(constructSourceCommand<SetDeformBindingCommand>(
        RetainedStringSlice(RetainedString::fromStaticLiteral("deform")), offsets)->run(gridCtx)->succeeded,
        RetainedString::fromStaticLiteral("deform composite should bind grid deformation after topology change"));
    auto binding = std::dynamic_pointer_cast<DeformationParameterBinding>(
        param->getBinding(grid, NullableString::fromStaticLiteral("deform")));
    require(binding != nullptr && binding->getValue({2, 2}).vertexOffsets.size() == grid->getVertices().size(),
        RetainedString::fromStaticLiteral("deform composite binding should match grid topology"));

    nijigenerate::core::actionstack::incActionUndo();
    require(param->getBinding(grid, NullableString::fromStaticLiteral("deform")) == nullptr,
        RetainedString::fromStaticLiteral("deform composite undo should remove binding only"));
    require(grid->getVertices().size() == 9 && path->getVertices().size() == 3,
        RetainedString::fromStaticLiteral(
            "deform composite undo binding should preserve both topology changes"));
    nijigenerate::core::actionstack::incActionRedo();
    require(param->getBinding(grid, NullableString::fromStaticLiteral("deform")) != nullptr,
        RetainedString::fromStaticLiteral("deform composite redo should restore grid deformation binding"));
}
} // namespace nijigenerate_tests::regression::detail
