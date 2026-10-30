#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the production bone.cpp intake.
#include "bone_deform_enqueue.hpp"
#include "bone_flush.hpp"
#include "bone_input.hpp"
#include "bone_job_workers.hpp"
#include "../bone_gpu_queries.hpp"
#include "../bone_packet_build.hpp"
#include "../bone_status.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <array>

namespace nijigenerate::actions::node::detail {
// Existing real production object read fault; no foreign implementation change.
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::commands::depth::bone::detail {

std::shared_ptr<CommandResult> enqueueDepthBoneGpuDeformCommand(
        std::shared_ptr<ext::nodes::ExDepthRigRoot> rigRoot,
        std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint,
        compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> actualTargets,
        compat::RetainedStringSlice reason, bool writePreview, bool writeBinding) {
    enforceDepthBoneGpuAvailable(reason);
    compat::Slice<DepthBoneGpuOffsetPacket> packets;
    for (auto targets = actualTargets; auto targetNode : targets) {
        targetKindOf(targetNode);
        auto deformable = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(targetNode);
        if (!deformable) continue;
        const auto targetUuid = targetNode->getUUID();
        if (!rigRoot) actions::node::detail::failActionObjectReference();
        const auto bindingIndex = rigRoot->findBindingIndex(targetUuid);
        if (bindingIndex < 0) continue;
        DepthBoneGpuOffsetPacket packet;
        compat::RetainedStringSlice error;
        auto& bindings = rigRoot->bindings;
        const auto index = static_cast<std::size_t>(bindingIndex);
        const auto length = bindings.size();
        if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), 4713);
        auto selectedBinding = &bindings[index];
        if (!ngBuildDepthBoneGpuOffsetPacket(rigRoot,
                selectedBinding, deformable, parameter,
                keypoint, packet, error, writePreview, writeBinding)) {
            const auto targetName = targetNode ? ext::nodes::ngRetainNodeName(targetNode->getName()).retainedString()
                : compat::RetainedString::fromStaticLiteral("(null)");
            std::array<core::translation::TypedFormatArgument, 4> arguments{
                core::translation::ngFormatArgument(targetName),
                core::translation::ngFormatArgument(keypoint.x),
                core::translation::ngFormatArgument(keypoint.y),
                core::translation::ngFormatArgument(error)};
            auto message = compat::RetainedString(core::translation::ngFormatTyped(
                "Depth Bone GPU packet build failed: target=%s key=(%s,%s) reason=%s", arguments));
            bone_status::ngDepthBoneUpdateFailed(
                rigRoot, targetNode, parameter, keypoint, reason.retainedString(), message);
            writeDepthBoneGpuFatalLog(message);
            failInput(message.view(), 4730);
        }
        packets.append(packet);
    }
    if (packets.empty()) failInput("No depth bone GPU targets updated", 4734);
    enqueueDepthBoneGpuRefreshBatch(packets, reason, depthBoneRefreshActionSink);
    return CommandResult::opCall(true);
}

} // namespace nijigenerate::commands::depth::bone::detail
