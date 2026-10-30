#include "apply.hpp"
#include "../common/session.hpp"
#include "../tools/operation.hpp"
#include <nijigenerate/actions/depth.hpp>
#include <nijigenerate/commands/depth/detail/bone_action_scope.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nodes/grid_deformer.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <tuple>

namespace nijigenerate::viewport::depth::draw {
namespace {
class DepthDrawApplyAction : public actions::AsyncGroupAction {
public:
    actions::ActionText describe() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Applied DepthDraw depth map"));
    }
    actions::ActionText describeUndo() override {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("DepthDraw depth map apply was reverted"));
    }
    actions::ActionText getName() override { return compat::RetainedString::fromStaticLiteral("this"); }
};
[[maybe_unused]] const bool sourceApplyActionClassNameRegistered = [] {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthDrawApplyAction),
        compat::RetainedString::fromStaticLiteral(
            "nijigenerate.viewport.depth.draw.apply.DepthDrawApplyAction"));
    return true;
}();
std::size_t countChangedVertices(compat::Slice<float> before, compat::Slice<float> after) {
    if (before.size() != after.size()) return after.size();
    std::size_t changed = 0;
    for (std::size_t i = 0; i < after.size(); ++i) if (before[i] != after[i]) ++changed;
    return changed;
}
DepthDrawApplySummary summarizeDepthDrawApply(
    std::shared_ptr<common::targetview::DepthTargetView> target, DepthDrawComposeResult& result) {
    DepthDrawApplySummary summary;
    if (!target) return summary;
    const auto grid = target->getTarget();
    if (!grid) return summary;
    const auto expectedUuid = result.targetGridUuid;
    const auto actualUuid = grid->getUUID();
    if (expectedUuid != actualUuid) return summary;
    const auto expectedVertices = result.depths.size();
    const auto actualVertices = grid->getVertices().size();
    if (expectedVertices != actualVertices) return summary;
    summary.missingSamples = result.missingVertices;
    for (auto stats : result.layerStats) {
        if (stats.contributedVertices > 0) ++summary.layersApplied;
        else ++summary.layersSkipped;
    }
    const auto depthMapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(grid);
    if (!depthMapped) return summary;
    const auto before = depthMapped->copyDepths();
    summary.changedVertices = countChangedVertices(before, result.depths);
    summary.changedTargets = summary.changedVertices > 0 ? 1 : 0;
    summary.succeeded = true;
    return summary;
}
}
DepthDrawApplySummary ngApplyDepthDrawTargetResultWithSummary(
    std::shared_ptr<commands::Context> context,
    std::shared_ptr<common::targetview::DepthTargetView> target,
    DepthDrawComposeResult& result) {
    auto summary = summarizeDepthDrawApply(target, result);
    if (!summary.succeeded || !context) {
        summary.succeeded = false;
        return summary;
    }
    core::actionstack::ngGuardActionStackScopes();
    const auto group = std::allocate_shared<DepthDrawApplyAction>(
        nicxlive::core::d_runtime::SourceAllocator<DepthDrawApplyAction>{});
    commands::depth::bone::ngBeginDepthBoneRefreshActionSink(group);
    DepthDrawApplySummary returnedSummary;
    compat::ngRunWithScopeExit([&] {
        const auto targetNode = target->getTarget();
        const auto depthMapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(targetNode);
        const auto depthReceiver = compat::ngRequireObject(depthMapped);
        const auto beforeDepths = depthReceiver->copyDepths();
        auto appliedDepths = result.depths.duplicate();
        const auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(targetNode);
        const auto grid = std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(targetNode);
        if (operated && grid) {
            const auto operations = operated->copyDepthOps();
            if (!operations.empty()) {
                const auto action = actions::depth::DepthOperationMappedChangeAction::createSource([&] {
                    return std::tuple{targetNode};
                });
                operated->replaceDepthOpBaseDepths(result.depths);
                action->updateNewState();
                group->addAction(action);
                appliedDepths = tools::operation::ngComputeDepthsFromOps(grid, operations, result.depths);
            }
        }
        target->replaceWorkingDepths(appliedDepths);
        group->addAction(common::session::ngDepthViewWorkingDepthsChangeAction(
            target, compat::RetainedString::fromStaticLiteral("Apply DepthDraw Depth Map")));
        if (!group->empty()) core::actionstack::incActionPush(group);
        summary.changedVertices = countChangedVertices(beforeDepths, appliedDepths);
        summary.changedTargets = summary.changedVertices > 0 ? 1 : 0;
        returnedSummary = summary;
    }, [&] { commands::depth::bone::ngEndDepthBoneRefreshActionSink(group); });
    return returnedSummary;
}
bool ngApplyDepthDrawTargetResult(std::shared_ptr<commands::Context> context,
    std::shared_ptr<common::targetview::DepthTargetView> target, DepthDrawComposeResult& result) {
    return ngApplyDepthDrawTargetResultWithSummary(context, target, result).succeeded;
}
} // namespace nijigenerate::viewport::depth::draw

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::draw::DepthDrawApplyAction> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.draw.apply.DepthDrawApplyAction";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.draw.apply.DepthDrawApplyAction)";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawApplyAction> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.apply.DepthDrawApplyAction)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawApplyAction)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.apply.DepthDrawApplyAction";
    inline static constexpr char unqualifiedElement[] = "DepthDrawApplyAction";
};
} // namespace nicxlive::core::d_runtime
