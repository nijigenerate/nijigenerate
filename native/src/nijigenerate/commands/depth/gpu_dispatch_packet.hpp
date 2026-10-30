#pragma once
#include <nijigenerate/compat/slice.hpp>
#include <math/veca.hpp>
#include <math/mat4.hpp>
#include <cstdint>
#include <limits>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::depth::bone_gpu_async {
struct DepthBoneGpuDispatchPacket {
    nicxlive::core::math::Vec2Array vertices;
    compat::Slice<float> depths, bones, sources;
    nicxlive::core::math::Mat4 targetToRoot = initialMatrix();
    nicxlive::core::math::Mat4 rootToTarget = initialMatrix();
    float influenceRadiusFloor = std::numeric_limits<float>::quiet_NaN();
    float radiusScale = std::numeric_limits<float>::quiet_NaN();
    std::uint32_t boneCount = 0, sourceCount = 0, maxInfluences = 0;
    DepthBoneGpuDispatchPacket() = default;
    DepthBoneGpuDispatchPacket(const DepthBoneGpuDispatchPacket&) = default;
    DepthBoneGpuDispatchPacket& operator=(const DepthBoneGpuDispatchPacket&) = default;
    // Native move transport preserves the source value; field assignment uses
    // the actual SDK veca.opAssign semantics, while construction copies handles.
    DepthBoneGpuDispatchPacket(DepthBoneGpuDispatchPacket&& other)
        : DepthBoneGpuDispatchPacket(static_cast<const DepthBoneGpuDispatchPacket&>(other)) {}
    DepthBoneGpuDispatchPacket& operator=(DepthBoneGpuDispatchPacket&& other) {
        return operator=(static_cast<const DepthBoneGpuDispatchPacket&>(other));
    }
private:
    static nicxlive::core::math::Mat4 initialMatrix() {
        nicxlive::core::math::Mat4 result;
        for (int row = 0; row < 4; ++row) for (int column = 0; column < 4; ++column)
            result[row][column] = std::numeric_limits<float>::quiet_NaN();
        return result;
    }
};
} // namespace nijigenerate::commands::depth::bone_gpu_async

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::commands::depth::bone_gpu_async::DepthBoneGpuDispatchPacket> {
    inline static constexpr bool isNativeObject=false;
    inline static constexpr char qualifiedArgument[]=
        "(nijigenerate.commands.depth.bone_gpu_async.DepthBoneGpuDispatchPacket)";
    inline static constexpr char unqualifiedArgument[]="(DepthBoneGpuDispatchPacket)";
    inline static constexpr char qualifiedElement[]=
        "nijigenerate.commands.depth.bone_gpu_async.DepthBoneGpuDispatchPacket";
    inline static constexpr char unqualifiedElement[]="DepthBoneGpuDispatchPacket";
};
} // namespace nicxlive::core::d_runtime
