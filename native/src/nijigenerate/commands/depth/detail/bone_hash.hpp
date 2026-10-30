#pragma once

#include "bone_dirty_providers.hpp"
#include <math/mat4.hpp>
#include <cstdint>

namespace nijigenerate::commands::depth::bone::detail {

std::uint64_t hashMix(std::uint64_t seed, std::uint64_t value);
std::uint64_t hashBool(std::uint64_t seed, bool value);
std::uint64_t hashInt(std::uint64_t seed, std::int64_t value);
std::uint64_t hashFloat(std::uint64_t seed, float value);
std::uint64_t hashVec2(std::uint64_t seed, nicxlive::core::math::Vec2 value);
std::uint64_t hashVec3(std::uint64_t seed, nicxlive::core::math::Vec3 value);
std::uint64_t hashMatrix(std::uint64_t seed, nicxlive::core::math::Mat4 matrix);
std::uint64_t hashStringValue(std::uint64_t seed, compat::RetainedStringSlice value);
std::uint64_t hashDepthBoneTargetStructure(std::uint64_t seed,
    std::shared_ptr<nicxlive::core::nodes::Node> target);
std::uint64_t depthBoneRigStructureHash(std::shared_ptr<ext::nodes::ExDepthRigRoot> root);
std::uint64_t depthBoneParameterStructureHash(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter);
std::uint64_t depthBonePoseKeyHash(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u keypoint);

// Use the actual base getIsSet array reference with checked direct indexing.
// Public isSet's invalid-index false and virtual getter overrides must not
// replace the original concrete storage read.
bool readDepthBonePoseIsSet(const std::shared_ptr<nicxlive::core::param::ValueParameterBinding>& binding,
    compat::Vec2u keypoint);

// The ordinary hash TU supplies AArch64 FCVTZS, x86 FISTP, and the standard
// fenv/llrint provider on other non-x64 targets. x64 uses its own CVTTSS2SI.
// The source cast truncates; out-of-range conversion is an invalid result,
// whose target hardware behavior must not be replaced by a C++ UB cast.
// https://dlang.org/spec/expression.html#cast_expressions
#if !defined(_M_X64) && !defined(__x86_64__)
std::int64_t convertDepthBoneQuantizedFloatToLong(float value);
#endif

} // namespace nijigenerate::commands::depth::bone::detail
