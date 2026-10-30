// Faithful body of regression.d12229-12270; include once outside namespaces.
#include "root_parameter_composite.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/parameter/group_commands.hpp>
#include <nijigenerate/commands/parameter/param_commands.hpp>
#include <nijigenerate/commands/parameter/paramedit_commands.hpp>
#include <nijigenerate/commands/parameter/prop_commands.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/node.hpp>
#include <param/binding_impl.hpp>
#include <array>
#include <cstdint>
#include <memory>

namespace nijigenerate_tests::regression::detail {
std::shared_ptr<nicxlive::core::param::ValueParameterBinding> newValueBinding(
    std::shared_ptr<nicxlive::core::param::Parameter> param,
    std::shared_ptr<nicxlive::core::nodes::Node> node,
    nijigenerate::compat::RetainedString name, bool setZero);

void testParameterCompositeLifecycleBindingWorkflow() {
    using namespace nijigenerate::commands::parameter::group;
    using namespace nijigenerate::commands::parameter::param;
    using namespace nijigenerate::commands::parameter::paramedit;
    using namespace nijigenerate::commands::parameter::prop;
    using nijigenerate::commands::Context;
    using nijigenerate::commands::detail::constructSourceCommand;
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::RetainedStringSlice;
    using nijigenerate::compat::Slice;
    using nicxlive::core::common::NullableString;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::nodes::Node;
    resetCase();

    auto ctx = std::allocate_shared<Context>(SourceAllocator<Context>{});
    ctx->puppet(nijigenerate::project::incActivePuppet());
    auto add = constructSourceCommand<Add2DParameterCommand>(std::int32_t{-1}, std::int32_t{1})->run(ctx);
    require(add->succeeded, RetainedString::fromStaticLiteral("parameter composite should create parameter"));
    auto param = add->created[0];
    auto node = Node::createSourceDeferred([] { return nijigenerate::project::incActivePuppet()->root; });
    node->setName(NullableString::fromStaticLiteral("parameter-composite-node"));
    ctx->parameters(Context::Parameters{param});
    require(constructSourceCommand<SetParameterNameCommand>(RetainedStringSlice(
        RetainedString::fromStaticLiteral("ParameterCompositeRenamed")))->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("parameter composite should rename parameter"));
    auto groupResult = constructSourceCommand<CreateParamGroupCommand>()->run(ctx);
    require(groupResult->succeeded, RetainedString::fromStaticLiteral("parameter composite should create group"));
    auto group = groupResult->created[0];
    require(constructSourceCommand<MoveParameterCommand>(group, std::int32_t{0})->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("parameter composite should move parameter into group"));

    auto binding = newValueBinding(param, node, RetainedString::fromStaticLiteral("transform.t.x"), true);
    binding->setValue({1, 1}, 12.0f);
    // These passive native descriptors cross allocation without evaluating
    // either Source array literal. Conversion prepares them once, in order.
    struct AxisLiteralFrame {
        Slice<float> x;
        Slice<float> y;
        void prepare(bool includeY) {
            if (x.isNull()) x = Slice<float>{0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
            if (includeY && y.isNull()) y = Slice<float>{0.0f, 0.5f, 1.0f};
        }
    } axes;
    struct AxisLiteralArgument {
        AxisLiteralFrame& frame;
        bool second;
        operator Slice<float>() && {
            frame.prepare(second);
            return second ? frame.y : frame.x;
        }
    };
    require(std::allocate_shared<ApplyParameterPropsAxesCommand>(SourceAllocator<ApplyParameterPropsAxesCommand>{},
        std::array<float, 2>{-2.0f, -2.0f}, std::array<float, 2>{2.0f, 2.0f},
        AxisLiteralArgument{axes, false}, AxisLiteralArgument{axes, true})->run(ctx)->succeeded,
        RetainedString::fromStaticLiteral("parameter composite should resize axes and remap binding"));
    require(binding->isSet({2, 1}) && near(binding->getValue({2, 1}), 12.0f),
        RetainedString::fromStaticLiteral("parameter composite should remap bound keypoint"));

    auto dup = constructSourceCommand<DuplicateParameterCommand>()->run(ctx);
    require(dup->succeeded,
        RetainedString::fromStaticLiteral("parameter composite should duplicate parameter after axes resize"));
    require(nijigenerate::project::incActivePuppet()->parameters.size() == 2,
        RetainedString::fromStaticLiteral("parameter composite duplicate should be inserted"));
    nijigenerate::core::actionstack::incActionUndo();
    require(nijigenerate::project::incActivePuppet()->parameters.size() == 1,
        RetainedString::fromStaticLiteral("parameter composite undo duplicate should restore parameter count"));
    nijigenerate::core::actionstack::incActionUndo();
    require(param->axisPoints[0].size() == 3 && binding->isSet({1, 1}), RetainedString::fromStaticLiteral(
        "parameter composite undo axes should restore original axis grid and binding key"));
    nijigenerate::core::actionstack::incActionRedo();
    require(param->axisPoints[0].size() == 5 && binding->isSet({2, 1}),
        RetainedString::fromStaticLiteral("parameter composite redo axes should restore remapped binding"));
}
} // namespace nijigenerate_tests::regression::detail
