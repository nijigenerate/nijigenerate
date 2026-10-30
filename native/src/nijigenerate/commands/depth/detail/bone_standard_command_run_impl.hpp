#pragma once
#include "../bone_commands.hpp"
#include "bone_input.hpp"
#include "bone_strict_scope.hpp"
#include "bone_standard_tables.hpp"
#include "bone_standard_parameters.hpp"
#include "bone_parameter_lookup.hpp"
#include "bone_action_scope.hpp"
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <bit>
#include <cstddef>
#include <limits>
#include <tuple>

namespace nijigenerate::commands::depth::bone {
namespace {
template<class Array> decltype(auto) standardDepthBindingElement(Array& values, std::size_t index) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/depth/bone.d"), 4359);
    return values[index];
}

template<class T> using StandardDepthMap = core::selector::SelectorMap<compat::RetainedStringSlice, T,
    core::selector::SelectorStringHash, core::selector::SelectorStringEqual>;

template<std::size_t N>
compat::RetainedString standardDepthNamedError(const char (&key)[N], compat::RetainedString name) {
    const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(name)};
    return compat::RetainedString(core::translation::ngFormatTyped(pattern.view(), arguments));
}

nicxlive::core::common::SharedArray<float> copyStandardDepthAxis(compat::Slice<float> source) {
    const auto count = source.size();
    nicxlive::core::common::SharedArray<float> result(count, std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < count; ++i) result[i] = source[i];
    return result;
}
} // namespace

std::shared_ptr<CommandResult> AddStandardDepthParametersCommand::run(std::shared_ptr<Context>) {
    using core::settings::SettingsJsonValue;
    using nicxlive::core::param::Parameter;
    using nicxlive::core::param::ValueParameterBinding;
    using ValueBindingBase = nicxlive::core::param::ParameterBindingImpl<float>;
    using compat::RetainedString;
    using compat::RetainedStringSlice;

    auto rigRoot = detail::requireRoot(root);
    auto puppet = project::incActivePuppet();
    if (!puppet) detail::failInput("No active puppet", 4240);
    auto bindingSpecs = standardDepthBoneBindingSpecs();
    if (!bindingDefinitions.is_null()) {
        bindingSpecs = compat::Slice<StandardDepthBoneBindingSpec>{};
        if (!bindingDefinitions.is_array()) detail::failInput("Explicit depth drivers must be an array", 4245);
        auto definitions = bindingDefinitions.as_array();
        for (auto definition : definitions) {
            StandardDepthBoneBindingSpec spec;
            spec.parameterName = definition.sourceIndexValue("parameter").as_string().retainedString();
            spec.boneId = definition.sourceIndexValue("bone").as_string().retainedString();
            spec.bindingName = definition.sourceIndexValue("binding").as_string().retainedString();
            const auto bindingName = spec.bindingName.view();
            if (bindingName != "transform.r.x" && bindingName != "transform.r.y" &&
                bindingName != "transform.r.z" && bindingName != "transform.t.x" && bindingName != "transform.t.y")
                detail::failInput("Unsupported explicit depth driver", 4250);
            auto points = definition.sourceIndexValue("values").as_array();
            for (auto point : points) {
                auto coordinate = point.sourceIndexValue("key").as_array();
                if (coordinate.size() != 2)
                    detail::failInput("Depth driver requires a two-dimensional parameter key", 4255);
                const auto x = detail::strictJsonNumber(coordinate[0],
                    RetainedString::fromStaticLiteral("driver key X"));
                const auto y = detail::strictJsonNumber(coordinate[1],
                    RetainedString::fromStaticLiteral("driver key Y"));
                const auto value = detail::strictJsonNumber(point.sourceIndexValue("value"),
                    RetainedString::fromStaticLiteral("driver value"));
                if (!(x >= -1 && x <= 1 && y >= -1 && y <= 1 && std::isfinite(value)))
                    detail::failInput("Invalid explicit depth driver value", 4258);
                spec.values.append(StandardDepthBindingValue{{x, y}, value});
            }
            if (spec.values.empty()) detail::failInput("Explicit depth driver has no keys", 4261);
            bindingSpecs.append(spec);
        }
        auto originalSpecs = bindingSpecs.duplicate();
        for (auto spec : originalSpecs) {
            if (spec.bindingName.view() != "transform.t.x" && spec.bindingName.view() != "transform.t.y") continue;
            auto counterpart = spec.bindingName.view() == "transform.t.x"
                ? RetainedString::fromStaticLiteral("transform.t.y")
                : RetainedString::fromStaticLiteral("transform.t.x");
            bool present = false;
            auto currentSpecs = bindingSpecs;
            for (auto other : currentSpecs) {
                if (other.parameterName == spec.parameterName && other.boneId == spec.boneId &&
                    other.bindingName == counterpart) present = true;
            }
            if (present) continue;
            StandardDepthBoneBindingSpec zero;
            zero.parameterName = spec.parameterName;
            zero.boneId = spec.boneId;
            zero.bindingName = counterpart;
            for (auto value : spec.values) zero.values.append(StandardDepthBindingValue{value.paramValue, 0.0f});
            bindingSpecs.append(zero);
        }
    }
    StandardDepthMap<std::shared_ptr<ext::nodes::ExDepthBone>> bonesById;
    for (auto spec : bindingSpecs) {
        if (bonesById.contains(spec.boneId.view())) continue;
        auto actualBone = findStandardDepthBone(rigRoot, spec.boneId);
        if (!actualBone)
            detail::failInput(standardDepthNamedError("Missing depth bone '%s'", spec.boneId).view(), 4283);
        bonesById[RetainedStringSlice(spec.boneId)] = actualBone;
    }
    auto group = detail::sourceDepthBoneObject<actions::GroupAction>();
    StandardDepthMap<std::shared_ptr<Parameter>> parametersByName;
    StandardDepthMap<compat::Slice<StandardDepthBoneBindingSpec>> specsByParameter;
    for (auto spec : bindingSpecs) specsByParameter[RetainedStringSlice(spec.parameterName)].append(spec);
    bool changed = false;
    auto parameterSpecs = standardDepthParameterSpecs();
    for (auto& spec : parameterSpecs) {
        auto parameter = findDepthParameterByName(spec.name);
        if (parameter && parameter->isVec2 != spec.isVec2)
            return CommandResult::opCall(false, standardDepthParameterTypeConflictMessage(parameter, spec));
    }
    for (auto spec : parameterSpecs) {
        auto parameter = findDepthParameterByName(spec.name);
        if (!parameter) {
            auto created = detail::sourceDepthBoneObject<ext::ExParameter>(
                spec.name.toNullableString(), spec.isVec2);
            created->min = spec.minValue;
            created->max = spec.maxValue;
            created->defaults = {0.0f, 0.0f};
            created->value = created->defaults;
            created->axisPoints[0] = copyStandardDepthAxis(spec.axisX);
            created->axisPoints[1] = copyStandardDepthAxis(spec.axisY);
            puppet->parameters.push_back(created);
            group->addAction(detail::sourceDepthBoneObject<actions::parameter::ParameterAddAction>(
                created, &puppet->parameters));
            parameter = created;
            changed = true;
        } else {
            RetainedString message;
            bool parameterChanged;
            const auto found = specsByParameter.find(spec.name.view());
            auto subset = found == specsByParameter.end()
                ? compat::Slice<StandardDepthBoneBindingSpec>{} : found->second;
            if (!conformStandardDepthParameter(parameter, spec, subset, group, parameterChanged, message))
                return CommandResult::opCall(false, message);
            changed = changed || parameterChanged;
        }
        parametersByName[RetainedStringSlice(spec.name)] = parameter;
    }
    for (auto spec : bindingSpecs) {
        const auto found = parametersByName.find(spec.parameterName.view());
        auto parameter = found == parametersByName.end() ? std::shared_ptr<Parameter>{} : found->second;
        if (!parameter)
            detail::failInput(standardDepthNamedError("Missing parameter '%s'", spec.parameterName).view(), 4327);
        auto actualBone = bonesById.at(spec.boneId.view());
        auto binding = std::dynamic_pointer_cast<ValueParameterBinding>(
            parameter->getBinding(actualBone, spec.bindingName.toNullableString()));
        if (!binding) {
            binding = std::dynamic_pointer_cast<ValueParameterBinding>(
                parameter->createBinding(actualBone, spec.bindingName.toNullableString()));
            parameter->addBinding(binding);
            group->addAction(actions::binding::ngCreateBindingAddRemoveAction<
                actions::binding::ParameterBindingAddAction>(parameter, binding));
            changed = true;
        }
        if (!binding)
            detail::failInput(
                standardDepthNamedError("Cannot create value binding '%s'", spec.bindingName).view(), 4337);
        for (auto valueSpec : spec.values) {
            std::ptrdiff_t xIndex = -1, yIndex = -1;
            const auto xCount = parameter->axisPoints[0].size();
            for (std::size_t x = 0; x < xCount; ++x) {
                if (std::abs(parameterAxisValue(parameter, 0, x) - valueSpec.paramValue.x) <= 0.000001f) {
                    xIndex = static_cast<std::ptrdiff_t>(x);
                    break;
                }
            }
            const auto yCount = parameter->axisPoints[1].size();
            for (std::size_t y = 0; y < yCount; ++y) {
                const auto axisValue = parameter->isVec2 ? parameterAxisValue(parameter, 1, y) : 0.0f;
                if (std::abs(axisValue - valueSpec.paramValue.y) <= 0.000001f) {
                    yIndex = static_cast<std::ptrdiff_t>(y);
                    break;
                }
            }
            if (xIndex < 0 || yIndex < 0)
                detail::failInput(standardDepthNamedError(
                    "Standard depth binding keypoint is not present in parameter '%s'",
                    spec.parameterName).view(), 4355);
            const auto x = static_cast<std::size_t>(xIndex);
            const auto y = static_cast<std::size_t>(yIndex);
            const auto value = valueSpec.value;
            if (standardDepthBindingElement(standardDepthBindingElement(
                    binding->ValueBindingBase::getIsSet(), x), y) &&
                std::abs(standardDepthBindingElement(standardDepthBindingElement(
                    binding->values, x), y) - value) <= 0.000001f) continue;
            using ValueAction = actions::binding::ParameterBindingValueChangeAction<float, ValueParameterBinding>;
            auto action = actions::binding::ngCreateBindingValueActionDeferred<ValueAction>([&] {
                const auto label = ext::nodes::ngRetainNodeName(
                    compat::ngRequireObject(binding)->getNameDescriptor());
                return std::tuple{label, binding,
                    std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(x)),
                    std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(y))};
            });
            binding->setValue(nicxlive::core::param::Vec2u{
                static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y)}, value);
            action->updateNewState();
            group->addAction(action);
            changed = true;
        }
        binding->reInterpolate();
    }
    if (!changed) return CommandResult::opCall(false,
        RetainedString::fromStaticLiteral("Standard depth parameters already exist"));
    core::actionstack::incActionPush(group);
    if (rigRoot->puppetRef()) {
        auto currentPuppet = rigRoot->puppetRef();
        if (!currentPuppet) actions::node::detail::failActionObjectReference();
        currentPuppet->rescanNodes();
    }
    return CommandResult::opCall(true);
}

} // namespace nijigenerate::commands::depth::bone
