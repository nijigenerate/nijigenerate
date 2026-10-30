#pragma once

#include "gpu_dispatch_packet.hpp"
#include <nijigenerate/compat/vec2u.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nodes/deformable.hpp>
#include <param/parameter.hpp>
#include <limits>

namespace nijigenerate::commands::depth::bone {
namespace detail {
inline nicxlive::core::math::Mat4 initialDepthBonePacketMatrix() {
    nicxlive::core::math::Mat4 result;
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            result[row][column] = std::numeric_limits<float>::quiet_NaN();
    return result;
}
}

struct DepthBoneGpuOffsetPacket {
    std::shared_ptr<ext::nodes::ExDepthRigRoot> root;
    std::shared_ptr<nicxlive::core::nodes::Deformable> target;
    std::shared_ptr<nicxlive::core::param::Parameter> parameter;
    compat::Vec2u keypoint{};
    bool writePreview = false;
    bool writeBinding = false;
    nicxlive::core::math::Vec2Array vertices;
    compat::Slice<float> rawDepths;
    compat::Slice<float> depths;
    compat::Slice<float> bones;
    compat::Slice<float> sourceInputs;
    compat::Slice<float> sources;
    nicxlive::core::math::Mat4 targetToRoot = detail::initialDepthBonePacketMatrix();
    nicxlive::core::math::Mat4 rootToTarget = detail::initialDepthBonePacketMatrix();
    float worldScale = std::numeric_limits<float>::quiet_NaN();
    float influenceRadiusFloor = std::numeric_limits<float>::quiet_NaN();
    float radiusScale = std::numeric_limits<float>::quiet_NaN();
    std::uint32_t boneCount = 0;
    std::uint32_t sourceCount = 0;
    std::uint32_t maxInfluences = 0;
    std::uint64_t rigHash = 0;
    std::uint64_t parameterStructureHash = 0;
    std::uint64_t poseHash = 0;
    std::uint32_t staleRetryCount = 0;

    DepthBoneGpuOffsetPacket() = default;
    DepthBoneGpuOffsetPacket(const DepthBoneGpuOffsetPacket&) = default;
    DepthBoneGpuOffsetPacket& operator=(const DepthBoneGpuOffsetPacket&) = default;
    // D value transport retains source descriptors; assignment still invokes veca.opAssign.
    DepthBoneGpuOffsetPacket(DepthBoneGpuOffsetPacket&& other)
        : DepthBoneGpuOffsetPacket(static_cast<const DepthBoneGpuOffsetPacket&>(other)) {}
    DepthBoneGpuOffsetPacket& operator=(DepthBoneGpuOffsetPacket&& other) {
        return operator=(static_cast<const DepthBoneGpuOffsetPacket&>(other));
    }

    bone_gpu_async::DepthBoneGpuDispatchPacket dispatchPacket() {
        bone_gpu_async::DepthBoneGpuDispatchPacket packet;
        packet.vertices = vertices;
        packet.depths = depths;
        packet.bones = bones;
        packet.sources = sources;
        packet.targetToRoot = targetToRoot;
        packet.rootToTarget = rootToTarget;
        packet.influenceRadiusFloor = influenceRadiusFloor;
        packet.radiusScale = radiusScale;
        packet.boneCount = boneCount;
        packet.sourceCount = sourceCount;
        packet.maxInfluences = maxInfluences;
        return packet;
    }
};

} // namespace nijigenerate::commands::depth::bone
