#include "map_json.hpp"
#include <compat/source_allocator.hpp>
#include "map_composition.hpp"
#include "detail/bone_action_scope.hpp"
#include "detail/bone_dirty_providers.hpp"
#include <nijigenerate/actions/depthbone.hpp>
#include <nijigenerate/actions/asyncprogress.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/io/depthmap_psd_targets.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <tuple>
#include <utility>
namespace nijigenerate::commands::depth::map {
using namespace detail;
using namespace io::depthmap_psd;
namespace detail {
compat::Slice<ext::nodes::ExDepthRigBinding> copyDepthRigBindings(
    compat::Slice<ext::nodes::ExDepthRigBinding> bindings) {
    auto result = bindings.duplicate();
    for (auto& binding : result) {
        binding.sourceBoneUuids = binding.sourceBoneUuids.duplicate();
        binding.sourceSettings = binding.sourceSettings.duplicate();
        ext::nodes::DepthUuidFloatMap copy;
        for (const auto& [uuid, value] : binding.influenceRule.multipliersByBoneUuid.entries()) copy[uuid] = value;
        binding.influenceRule.multipliersByBoneUuid = copy;
    }
    return result;
}
}
bool ngCanApplyPsdDepthImportResult(PsdDepthComposedView composed, compat::RetainedString& error) {
    error = {};
    const auto imported = PsdDepthAccess::imported(composed);
    if (!imported) { error = "PSD depth import apply requires a composed view"; return false; }
    if (const auto progress = activePsdDepthImportProgress; progress && progress->isActive()) {
        error = "PSD depth map import is still finalizing"; return false;
    }
    const auto grids = imported->grids;
    for (auto grid : grids) {
        if (grid.skipped || !grid.grid) continue;
        if (grid.depths.size() != grid.grid->getVertices().size()) {
            error = "Imported depths length must match target vertices"; return false;
        }
        if (!std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(grid.grid)) {
            error = "Imported target must support depth maps"; return false;
        }
    }
    return true;
}
std::shared_ptr<ExCommandResult<core::settings::SettingsJsonValue>> ngApplyPsdDepthImportResult(
    PsdDepthComposedView composed) {
    std::size_t changedGrids = 0;
    compat::RetainedString error;
    if (!ngCanApplyPsdDepthImportResult(composed, error)) return ExCommandResult<Json>::opCall(false, Json(nullptr), error);
    const auto imported = PsdDepthAccess::imported(composed);
    auto normalizationGrids = imported->grids;
    for (auto& grid : normalizationGrids) {
        ext::nodes::ngNormalizeDepths(std::span<float>(grid.depths.data(), grid.depths.size()));
        ext::nodes::ngNormalizeDepths(std::span<float>(grid.baseDepths.data(), grid.baseDepths.size()));
        updatePsdDepthGridRange(grid);
    }
    if (!imported->grids.empty()) {
        core::actionstack::ngGuardActionStackScopes();
        auto group = std::allocate_shared<PsdDepthImportChangeAction>(
            nicxlive::core::d_runtime::SourceAllocator<PsdDepthImportChangeAction>{});
        bone::ngBeginDepthBoneRefreshActionSink(group);
        compat::ngRunWithScopeExit([&] {
            detail::DepthMapIntegerMap<std::uint64_t,
                std::shared_ptr<nicxlive::core::nodes::Node>> appliedTargets;
            const auto targetGrids = imported->grids;
            for (auto grid : targetGrids)
                if (!grid.skipped && grid.grid)
                    appliedTargets[compat::ngRequireObject(grid.grid)->getUUID()] = grid.grid;
            detail::DepthMapIntegerMap<std::uint64_t, bool> sourceTransformChangedTargets;
            const auto puppet = project::incActivePuppet();
            const auto root = compat::ngRequireObject(project::incActivePuppet())->root;
            const auto roots = compat::ngRequireObject(puppet)->findNodesType<ext::nodes::ExDepthRigRoot>(root);
            for (auto root : roots) {
                const auto oldBindings = copyDepthRigBindings(compat::ngRequireObject(root)->bindings);
                auto nextBindings = copyDepthRigBindings(compat::ngRequireObject(root)->bindings);
                bool changed = false;
                for (auto& binding : nextBindings) {
                    if (!appliedTargets.contains(binding.targetUuid)) continue;
                    for (auto& setting : binding.sourceSettings) {
                        if (setting.depthScale == 1.0f && setting.depthOffset == 0.0f) continue;
                        setting.depthScale = 1.0f; setting.depthOffset = 0.0f;
                        changed = true; sourceTransformChangedTargets[binding.targetUuid] = true;
                    }
                }
                if (!changed) continue;
                compat::ngRequireObject(root)->bindings = copyDepthRigBindings(nextBindings);
                group->addAction(std::allocate_shared<actions::depthbone::DepthRigBindingsChangeAction>(
                    nicxlive::core::d_runtime::SourceAllocator<actions::depthbone::DepthRigBindingsChangeAction>{},
                    compat::RetainedString("Import PSD Depth Map"), root, oldBindings, nextBindings));
            }
            const auto gridsToApply = imported->grids;
            for (auto grid : gridsToApply) {
                if (grid.skipped || !grid.grid) continue;
                const auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(grid.grid);
                const auto old = compat::ngRequireObject(mapped)->copyDepths();
                const bool depthChanged = old.size() != grid.depths.size() ||
                    !std::equal(old.begin(), old.end(), grid.depths.begin());
                const auto operated = std::dynamic_pointer_cast<ext::nodes::DepthOperationMappedNode>(grid.grid);
                const bool hasOps = operated && !operated->copyDepthOps().empty();
                const bool sourceChanged = sourceTransformChangedTargets.contains(
                    compat::ngRequireObject(grid.grid)->getUUID());
                if (!depthChanged && !hasOps && !sourceChanged) continue;
                const auto reason = compat::RetainedString("Import PSD Depth Map");
                const auto clear = ngClearDepthOpsChangeAction(grid.grid, reason);
                if (clear) group->addAction(clear);
                if (depthChanged) group->addAction(ngApplyDepthsChangeAction(grid.grid, grid.depths, reason));
                else if (!clear && sourceChanged) bone::ngMarkDepthBoneDirtyForTarget(grid.grid, reason);
                ++changedGrids;
            }
            if (bone::ngHasPendingDepthBoneRefreshForSink(group)) {
                if (!group->empty()) core::actionstack::incActionPush(group);
                using Progress = actions::asyncprogress::AsyncGroupActionProgress;
                activePsdDepthImportProgress = Progress::ngCreateDerivedSource<>([&group] {
                    std::shared_ptr<actions::AsyncGroupAction> actionArg = group;
                    auto titleArg = core::translation::ngTranslateRetained(
                        compat::RetainedString::fromStaticLiteral("Finalizing PSD depth import..."));
                    auto completedArg = core::translation::ngTranslateRetained(
                        compat::RetainedString::fromStaticLiteral("PSD depth map imported"));
                    compat::RetainedString canceledArg;
                    auto workCountArg = Progress::WorkCountHandler(
                        [group] { return bone::ngPendingDepthBoneRefreshWorkForSink(group); });
                    auto pumpArg = Progress::PumpHandler([] { bone::ngFlushDepthBoneDirty(); });
                    return std::tuple{std::move(actionArg), std::move(titleArg), std::move(completedArg),
                        std::move(canceledArg), std::move(workCountArg), std::move(pumpArg)};
                });
            } else if (!group->empty()) core::actionstack::incActionPush(group);
        }, [group] { bone::ngEndDepthBoneRefreshActionSink(group); });
    }
    return ExCommandResult<Json>::opCall(true, ngPsdDepthImportSummaryToJson(*imported, changedGrids),
        "PSD depth map imported");
}
#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
bool ngPsdDepthImportProgressVisibleForRegression() {
    return activePsdDepthImportProgress && activePsdDepthImportProgress->isVisible();
}
#endif
}
