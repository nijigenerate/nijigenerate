#pragma once
#include "standard_depth_fields.hpp"
#include "bone_parameter_axis.hpp"
#include "bone_action_scope.hpp"
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <array>
#include <cmath>

namespace nijigenerate::commands::depth::bone {
namespace {
using StandardParameterRef = std::shared_ptr<nicxlive::core::param::Parameter>;

compat::RetainedString standardDepthParameterTypeConflictMessage(
        StandardParameterRef parameter, StandardDepthParameterSpec& spec) {
    auto format = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "Existing parameter '%s' is %s but the standard depth template requires %s"));
    const auto name = spec.name;
    const auto actualKind = compat::ngRequireObject(parameter)->isVec2
        ? compat::RetainedString::fromStaticLiteral("2D") : compat::RetainedString::fromStaticLiteral("1D");
    const auto expectedKind = spec.isVec2
        ? compat::RetainedString::fromStaticLiteral("2D") : compat::RetainedString::fromStaticLiteral("1D");
    const std::array<core::translation::TypedFormatArgument, 3> arguments{
        core::translation::ngFormatArgument(name),
        core::translation::ngFormatArgument(actualKind),
        core::translation::ngFormatArgument(expectedKind)};
    return compat::RetainedString(core::translation::ngFormatTyped(format.view(), arguments));
}

bool parameterStructureMatchesSpec(StandardParameterRef parameter, StandardDepthParameterSpec& spec) {
    if (!parameter) return false;
    if (parameter->isVec2 != spec.isVec2) return false;
    if (std::abs(parameter->min.x - spec.minValue.x) > 0.000001f ||
        std::abs(parameter->max.x - spec.maxValue.x) > 0.000001f) return false;
    if (spec.isVec2 && (std::abs(parameter->min.y - spec.minValue.y) > 0.000001f ||
        std::abs(parameter->max.y - spec.maxValue.y) > 0.000001f)) return false;
    return true;
}

void ensureParameterAxisValue(StandardParameterRef parameter, std::uint32_t axis, float value) {
    constexpr float epsilon = 0.000001f;
    compat::ngRequireObject(parameter);
    if (!parameter->isVec2 && axis == 1) return;
    if (parameterHasAxisValue(parameter, axis, value)) return;
    const auto minValue = axis == 0 ? parameter->min.x : parameter->min.y;
    const auto maxValue = axis == 0 ? parameter->max.x : parameter->max.y;
    const auto span = maxValue - minValue;
    if (std::abs(span) <= epsilon) return;
    const auto offset = (value - minValue) / span;
    if (offset > epsilon && offset < 1.0f - epsilon) parameter->insertAxisPoint(axis, offset);
}

bool standardDepthParameterHasRequiredKeys(StandardParameterRef parameter,
        compat::Slice<StandardDepthBoneBindingSpec> bindingSpecs) {
    for (auto spec : bindingSpecs) {
        for (auto value : spec.values) {
            if (!parameterHasAxisValue(parameter, 0, value.paramValue.x)) return false;
            if (!parameterHasAxisValue(parameter, 1, value.paramValue.y)) return false;
        }
    }
    return true;
}

void ensureStandardDepthParameterKeys(StandardParameterRef parameter,
        compat::Slice<StandardDepthBoneBindingSpec> bindingSpecs) {
    for (auto spec : bindingSpecs) {
        for (auto value : spec.values) {
            ensureParameterAxisValue(parameter, 0, value.paramValue.x);
            ensureParameterAxisValue(parameter, 1, value.paramValue.y);
        }
    }
}

bool conformStandardDepthParameter(StandardParameterRef parameter, StandardDepthParameterSpec& spec,
        compat::Slice<StandardDepthBoneBindingSpec> bindingSpecs, std::shared_ptr<actions::GroupAction> group,
        bool& changed, compat::RetainedString& message) {
    changed = false;
    message.reset();
    compat::ngRequireObject(parameter);
    if (parameter->isVec2 != spec.isVec2) {
        message = standardDepthParameterTypeConflictMessage(parameter, spec);
        return false;
    }
    if (parameterStructureMatchesSpec(parameter, spec) &&
        standardDepthParameterHasRequiredKeys(parameter, bindingSpecs)) return true;
    auto action = detail::sourceDepthBoneObject<actions::parameter::ParameterShapeChangeAction>(
        compat::RetainedString::fromStaticLiteral("standard depth parameter axes"), parameter);
    parameter->min = spec.minValue;
    parameter->max = spec.maxValue;
    ensureStandardDepthParameterKeys(parameter, bindingSpecs);
    action->updateNewState();
    compat::ngRequireObject(group)->addAction(action);
    changed = true;
    return true;
}

} // namespace
} // namespace nijigenerate::commands::depth::bone
