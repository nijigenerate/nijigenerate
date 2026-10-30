#include "map_json.hpp"
#include "detail/bone_dirty_providers.hpp"
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nodes/grid_deformer.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
namespace nijigenerate::commands::depth::map {
namespace detail {
namespace {
[[maybe_unused]] const bool psdDepthImportSourceClassInfo = [] {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(PsdDepthImportChangeAction),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.PsdDepthImportChangeAction"));
    return true;
}();
}
std::shared_ptr<ext::nodes::DepthMappedNode> requireDepthMapped(std::shared_ptr<nicxlive::core::nodes::Node> node) {
    auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(node);
    if (!mapped) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("target must support depth maps"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 62);
    return mapped;
}
std::shared_ptr<ext::nodes::DepthOperationMappedNode> requireDepthOperated(
    std::shared_ptr<nicxlive::core::nodes::Node> node) {
    auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(node);
    if (!operated) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("target must support depth operations"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 123);
    return operated;
}
std::shared_ptr<nicxlive::core::nodes::GridDeformer> requireDepthGrid(
    std::shared_ptr<nicxlive::core::nodes::Node> node) {
    auto grid = std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(node);
    if (!grid) throw depthMapEnforcementFailure(
        compat::RetainedString::fromStaticLiteral("target must be a GridDeformer"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/map.d"), 129);
    return grid;
}
actions::ActionText PsdDepthImportChangeAction::describe() {
    const auto receiver = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Imported PSD depth map"));
}
actions::ActionText PsdDepthImportChangeAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    return core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("PSD depth map import was reverted"));
}
actions::ActionText PsdDepthImportChangeAction::getName() {
    const auto receiver = weak_from_this().lock();
    return compat::RetainedString::fromStaticLiteral("this");
}
void replaceDepthsWithUndo(std::shared_ptr<nicxlive::core::nodes::Node> target,
    compat::Slice<float> values, compat::RetainedStringSlice reason) {
    core::actionstack::incActionPush(ngApplyDepthsChangeAction(target, values, reason));
}
std::shared_ptr<actions::depth::DepthOperationMappedChangeAction> ngClearDepthOpsChangeAction(
    std::shared_ptr<nicxlive::core::nodes::Node> target, compat::RetainedStringSlice reason) {
    auto action = actions::depth::ngClearDepthOperationsChangeAction(target);
    if (!action) return {};
    bone::ngMarkDepthBoneDirtyForTarget(target, reason);
    return action;
}
void replaceDepthOpsWithUndo(std::shared_ptr<nicxlive::core::nodes::Node> target,
    compat::Slice<ext::nodes::ExDepthOp> nextOps, compat::RetainedStringSlice reason) {
    auto operated = requireDepthOperated(target);
    auto action = std::allocate_shared<actions::depth::DepthOperationMappedChangeAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::depth::DepthOperationMappedChangeAction>{}, target);
    auto previousOps = operated->copyDepthOps();
    if (nextOps.empty()) operated->replaceDepthOpBaseDepths({});
    else if (previousOps.empty()) {
        auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(target);
        operated->replaceDepthOpBaseDepths(mapped ? mapped->copyDepths() : compat::Slice<float>{});
    }
    operated->replaceDepthOps(nextOps);
    action->updateNewState();
    core::actionstack::incActionPush(action);
    bone::ngMarkDepthBoneDirtyForTarget(target, reason);
}
}
std::shared_ptr<actions::depth::DepthMappedChangeAction> ngApplyDepthsChangeAction(
    std::shared_ptr<nicxlive::core::nodes::Node> target, compat::Slice<float> values,
    compat::RetainedStringSlice reason) {
    auto mapped = detail::requireDepthMapped(target);
    auto action = std::allocate_shared<actions::depth::DepthMappedChangeAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::depth::DepthMappedChangeAction>{}, target, reason);
    mapped->replaceDepths(values);
    target->notifyChange(target, nicxlive::core::nodes::NotifyReason::AttributeChanged);
    action->updateNewState();
    bone::ngMarkDepthBoneDirtyForTarget(target, reason);
    return action;
}
}
