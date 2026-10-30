#pragma once

#include "depthbone_mutation_kind.hpp"
#include "../compat/retained_string_slice.hpp"
#include "../compat/vec2u.hpp"
#include <memory>
#include <compat/object_metadata.hpp>

namespace nicxlive::core::nodes { class Node; }
namespace nicxlive::core::param { class ParameterBinding; }

namespace nijigenerate::actions::depthboneinvalidation {

// Retain the real runtime objects. Copies preserve handle identity and the
// immutable reason's backing storage while keeping each descriptor independent.
struct DepthBoneMutation {
    DepthBoneMutationKind kind = DepthBoneMutationKind::BindingValue;
    std::shared_ptr<nicxlive::core::nodes::Node> target;
    std::shared_ptr<nicxlive::core::nodes::Node> affectedTarget;
    std::shared_ptr<nicxlive::core::param::ParameterBinding> binding;
    compat::Vec2u keypoint{};
    compat::RetainedStringSlice reason;
    bool settleBeforeDispatch = false;
};

} // namespace nijigenerate::actions::depthboneinvalidation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::actions::depthboneinvalidation::DepthBoneMutation> {
    inline static constexpr bool isNativeObject=false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral(
                    "(nijigenerate.actions.depthboneinvalidation.DepthBoneMutation)"),
            RetainedString::fromStaticLiteral("(DepthBoneMutation)"),
            RetainedString::fromStaticLiteral("nijigenerate.actions.depthboneinvalidation.DepthBoneMutation"),
            RetainedString::fromStaticLiteral("DepthBoneMutation")};
    }
};
} // namespace nicxlive::core::d_runtime
