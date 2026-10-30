#pragma once

#include <cstdint>

namespace nijigenerate::commands::depth::bone {

inline constexpr std::uint32_t DepthBoneGpuBoneStride = 24u;
inline constexpr std::uint32_t DepthBoneGpuSourceStride = 28u;
inline constexpr std::uint32_t DepthBoneGpuSourceDepthOffsetIndex = 6u;
inline constexpr std::uint32_t DepthBoneGpuSourceSinRotationIndex = 8u;
inline constexpr std::uint32_t DepthBoneGpuSourceCosRotationIndex = 9u;
inline constexpr std::uint32_t DepthBoneGpuSourceRotationPivotXShiftIndex = 10u;
inline constexpr std::uint32_t DepthBoneGpuSourcePoseYawIndex = 11u;
inline constexpr std::uint32_t DepthBoneGpuSourceNoYawSkinMatrixIndex = 12u;
inline constexpr std::uint32_t DepthBoneGpuMaxBones = 64u;
inline constexpr std::uint32_t DepthBoneGpuMaxSources = 128u;
inline constexpr std::uint32_t DepthBoneGpuMaxInfluences = 16u;
inline constexpr std::uint32_t DepthBoneGpuMaxVertices = 1'000'000u;
inline constexpr std::uint32_t DepthBoneGpuMaxStaleRetries = 3u;

} // namespace nijigenerate::commands::depth::bone
