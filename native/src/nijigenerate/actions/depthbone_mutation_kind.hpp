#pragma once

#include <cstdint>
#include <compat/object_metadata.hpp>

namespace nijigenerate::actions::depthboneinvalidation {
enum class DepthBoneMutationKind : std::int32_t {
    BindingValue = 0,
    BindingAllValues = 1,
    BindingStructure = 2,
    RigConfiguration = 3,
    TargetTransform = 4,
    TargetGeometry = 5,
    TargetDepth = 6,
};
} // namespace nijigenerate::actions::depthboneinvalidation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::actions::depthboneinvalidation::DepthBoneMutationKind> {
    inline static constexpr bool isNativeObject=false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral(
                    "(nijigenerate.actions.depthboneinvalidation.DepthBoneMutationKind)"),
            RetainedString::fromStaticLiteral("(DepthBoneMutationKind)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.depthboneinvalidation.DepthBoneMutationKind"),
            RetainedString::fromStaticLiteral("DepthBoneMutationKind")};
    }
};
} // namespace nicxlive::core::d_runtime
