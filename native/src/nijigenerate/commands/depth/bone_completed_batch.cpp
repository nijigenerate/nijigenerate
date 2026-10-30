#include "detail/bone_job_workers.hpp"
#include "detail/bone_input.hpp"
#include "detail/bone_action_scope.hpp"
#include "detail/bone_grid_index.hpp"
#include "detail/bone_grid_influence.hpp"
#include "detail/bone_math.hpp"
#include "bone_status.hpp"
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/compat/phobos_index_sort.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <bit>
#include <new>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace nijigenerate::commands::depth::bone::detail {
using nicxlive::core::math::Vec2Array;
using nicxlive::core::param::Parameter;
using nicxlive::core::param::ParameterBinding;
using nicxlive::core::param::DeformationParameterBinding;

namespace {
template<class... Args> compat::RetainedString completedBatchMessage(std::string_view pattern, Args&&... values) {
    const std::array<core::translation::TypedFormatArgument, sizeof...(Args)> arguments{
        core::translation::ngFormatArgument(std::forward<Args>(values))...};
    return compat::RetainedString(core::translation::ngFormatTyped(pattern, arguments));
}

[[noreturn]] void failCompletedBatch(compat::RetainedString message, std::size_t sourceLine) {
    abortDepthBoneGpuRefresh(message);
    failInput(message.view(), sourceLine);
}

// Keep the source's prefix/suffix replacement, including removal at the same index.
void removeCompletedBatch(std::uint32_t batchId) {
    std::size_t i = 0;
    while (i < depthBoneGpuCompletedJobs.size()) {
        if (depthBoneGpuCompletedJobs[i].batchId == batchId) {
            auto before = compat::ngArraySlice(depthBoneGpuCompletedJobs, 0, i);
            auto after = compat::ngArraySlice(depthBoneGpuCompletedJobs, i + 1, depthBoneGpuCompletedJobs.size());
            if (before.empty() && after.empty()) {
                depthBoneGpuCompletedJobs = {};
                continue;
            }
            try {
                std::vector<DepthBoneGpuCompletedJob> values;
                values.reserve(before.size() + after.size());
                for (auto value : before) values.push_back(value);
                for (auto value : after) values.push_back(value);
                depthBoneGpuCompletedJobs = compat::Slice<DepthBoneGpuCompletedJob>(std::move(values));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(
                    compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
            }
            continue;
        }
        ++i;
    }
}
} // namespace

bool applyDepthBoneGpuCompletedBatch(std::uint32_t batchId) {
    compat::Slice<DepthBoneGpuCompletedJob> batch;
    const auto completed = depthBoneGpuCompletedJobs;
    for (auto job : completed) if (job.batchId == batchId) batch.append(job);
    if (batch.empty() || batch.size() < batch[0].batchExpected) return false;

    std::shared_ptr<Parameter> parameter;
    compat::Vec2u keypoint;
    compat::RetainedStringSlice reason;
    std::shared_ptr<actions::AsyncGroupAction> actionSink;
    actions::AsyncActionToken actionToken;
    compat::Slice<std::shared_ptr<DeformationParameterBinding>> deformBindings;
    compat::Slice<Vec2Array> offsetsList;
    compat::Slice<std::shared_ptr<ParameterBinding>> created;
    bool changed = false;
    std::size_t expectedBindingWrites = 0;

    // M070.S0420: lexical preflight; failures return from the enclosing function.
    for (auto job : batch) {
        const auto targetNode = std::static_pointer_cast<nicxlive::core::nodes::Node>(job.packet.target);
        if (!targetNode || findActiveNodeByUuid(targetNode->getUUID()) != targetNode) {
            cancelDepthBoneGpuBatch(batchId,
                compat::RetainedString::fromStaticLiteral("Target is no longer attached to the active puppet"));
            return false;
        }
        if (job.actionSink && !job.actionToken.acceptsCompletion()) {
            cancelDepthBoneGpuBatch(batchId,
                compat::RetainedString::fromStaticLiteral("Owner operation is no longer active"));
            return false;
        }
        if (!job.valid) {
            bone_status::ngDepthBoneUpdateBatchCanceled(batchId, compat::RetainedString::fromStaticLiteral(
                "Depth Bone update batch was discarded because one target became stale"));
            removeCompletedBatch(batchId);
            return false;
        }
    }

    const auto hierarchyBatch = batch.duplicate();
    std::vector<std::size_t> order;
    try {
        order.resize(hierarchyBatch.size());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("rt/lifetime.d"), 1142);
    }
    std::iota(order.begin(), order.end(), std::size_t{0});
    // Both actual completed-record types exceed 32 bytes: the Phobos cutoff is 32.
    // Indices transport the real records; no stable-sort tie ordering is introduced.
    compat::phobos_index_sort::sort<sizeof(DepthBoneGpuCompletedJob)>(order,
        [&](std::size_t a, std::size_t b) {
            return nodeHierarchyDepth(hierarchyBatch[a].packet.target) <
                nodeHierarchyDepth(hierarchyBatch[b].packet.target);
        }, "sort: invalid comparison function.");
    DepthBoneResolvedOffsets resolvedOffsets;

    // M070.S0421: correct rootward-first input, preserving original offsets each pass.
    for (auto index : order) {
        auto job = hierarchyBatch[index];
        const auto target = job.packet.target;
        const auto targetNode = std::static_pointer_cast<nicxlive::core::nodes::Node>(target);
        if (!target || !targetNode) continue;
        auto adjusted = job.offsets.dup();
        for (int iteration = 0; iteration < 6; ++iteration) {
            auto inherited = depthBoneAncestorGridInfluence(job.packet.root, target, job.packet.parameter,
                job.packet.keypoint, adjusted, resolvedOffsets);
            if (inherited.size() != adjusted.size()) break;
            auto next = job.offsets.dup();
            next -= inherited;
            const auto converged = sameVec2Array(next, adjusted);
            adjusted = next;
            if (converged) break;
        }
        resolvedOffsets[static_cast<std::uint32_t>(targetNode->getUUID())] = adjusted;
    }

    // M070.S0422: write in original batch order; preview effects precede binding work.
    for (auto job : batch) {
        const auto target = job.packet.target;
        const auto targetNode = std::static_pointer_cast<nicxlive::core::nodes::Node>(target);
        if (!target || !targetNode) failCompletedBatch(compat::RetainedString::fromStaticLiteral(
            "Depth Bone GPU writeback failed: target was deleted before batch writeback"), 2466);
        if (job.offsets.size() != target->getVertices().size()) {
            const auto targetName = targetNode->getName();
            failCompletedBatch(completedBatchMessage(
                "Depth Bone GPU writeback failed: target=%s key=(%s,%s) readback offsets=%s vertices=%s",
                targetName, job.packet.keypoint.x, job.packet.keypoint.y,
                job.offsets.size(), target->getVertices().size()), 2476);
        }
        const auto adjusted = resolvedOffsets.find(static_cast<std::uint32_t>(targetNode->getUUID()));
        auto offsets = adjusted == resolvedOffsets.end() ? job.offsets : adjusted->second;
        if (job.packet.writePreview) {
            target->deformation = offsets;
            target->notifyChange(target, nicxlive::core::nodes::NotifyReason::AttributeChanged);
            changed = true;
        }
        if (!job.packet.writeBinding || !job.packet.parameter) continue;
        ++expectedBindingWrites;
        parameter = job.packet.parameter;
        keypoint = job.packet.keypoint;
        reason = job.reason;
        actionSink = job.actionSink;
        actionToken = job.actionToken;
        const auto bindingName = nicxlive::core::common::NullableString::fromStaticLiteral("deform");
        const auto existing = parameter->getBinding(targetNode, bindingName);
        auto deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(existing);
        if (!deformBinding) {
            deformBinding = std::dynamic_pointer_cast<DeformationParameterBinding>(
                parameter->getOrAddBinding(targetNode, bindingName));
            if (deformBinding) created.append(deformBinding);
        }
        if (!deformBinding) {
            const auto targetName = targetNode->getName();
            failCompletedBatch(completedBatchMessage(
                "Depth Bone GPU writeback failed: target=%s key=(%s,%s) deform binding could not be created",
                targetName, job.packet.keypoint.x, job.packet.keypoint.y), 2508);
        }
        deformBindings.append(deformBinding);
        offsetsList.append(offsets);
    }

    if (expectedBindingWrites != deformBindings.size()) failCompletedBatch(completedBatchMessage(
        "Depth Bone GPU writeback failed: batch=%s expected binding writes=%s actual=%s",
        batchId, expectedBindingWrites, deformBindings.size()), 2518);

    if (parameter && !deformBindings.empty()) {
        auto group = sourceDepthBoneObject<actions::GroupAction>();
        for (auto binding : created) group->addAction(actions::binding::ngCreateBindingAddRemoveAction<
            actions::binding::ParameterBindingAddAction>(parameter, binding, false));
        auto label = !reason.empty()
            ? completedBatchMessage(core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Auto Refresh Depth Bone Deform: %s")).view(), reason)
            : core::translation::ngTranslateRetained(
                compat::RetainedString::fromStaticLiteral("Auto Refresh Depth Bone Deform"));
        auto action = actions::parameter::ngCreateParameterBindingsAction<
            actions::parameter::ParameterChangeBindingsValueAction>(label,
            parameter, deformBindings, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(keypoint.x)),
            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(keypoint.y)), false);
        for (std::size_t i = 0; i < deformBindings.size(); ++i)
            deformBindings[i]->update({keypoint.x, keypoint.y}, offsetsList[i]);
        action->updateNewState();
        group->addAction(action);
        // M070.S0273: the synchronous bound push callback still returns true after a normal rollback.
        runWithDepthBoneRefreshActionSink(actionSink, [group, actionToken] {
            pushDepthBoneRefreshAction(group, actionToken);
            return true;
        });
        changed = true;
    }

    for (auto job : batch) bone_status::ngDepthBoneUpdateApplied(batchId, job.packet.root, job.packet.target);
    removeCompletedBatch(batchId);
    return changed;
}
} // namespace nijigenerate::commands::depth::bone::detail
