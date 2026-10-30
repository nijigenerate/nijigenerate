#include "detail/bone_fit.hpp"
#include "detail/bone_debug_log.hpp"
#include "detail/bone_refresh_state.hpp"
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/actions/binding_value_action.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/project.hpp>
#include <common/nullable_string.hpp>
#include <bit>
#include <cmath>
#include <limits>
#include <tuple>

namespace nijigenerate::commands::depth::bone {
using ext::nodes::ExDepthRigRoot;
using ext::nodes::ExDepthBone;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::param::Parameter;
using nicxlive::core::param::ValueParameterBinding;
using nicxlive::core::math::Vec2;

namespace detail {

bool nearestScaledWorldDepthAtPoint(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<ExDepthBone> bone, Vec2 worldPoint, float& worldDepth) {
    worldDepth = std::numeric_limits<float>::quiet_NaN();
    auto puppet = project::incActivePuppet();
    if (!root || !puppet) {
        depthBoneDebugLog("[FitZ] no root or puppet root=%s puppet=%s", !root, !puppet);
        return false;
    }
    std::size_t sampleCount = 0;
    float totalDepth = 0.0f;
    const auto worldScale = depthWorldScale(root);
    {
        const auto rootName = root->getName();
        const auto boneName = bone ? bone->getName()
            : nicxlive::core::common::NullableString::fromStaticLiteral("(fallback)");
        depthBoneDebugLog("[FitZ] start root=%s bone=%s bindings=%s", rootName, boneName, root->bindings.size());
    }
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        ext::nodes::ExDepthBoneSourceSettings setting;
        if (bone) {
            const auto sourceUuids = binding.sourceBoneUuids;
            const auto sourceUuid = bone->getUUID();
            bool contains = false;
            for (auto uuid : sourceUuids) {
                if (uuid == sourceUuid) { contains = true; break; }
            }
            if (!contains) {
                const auto debugTargetUuid = binding.targetUuid;
                const auto debugBoneUuid = bone->getUUID();
                depthBoneDebugLog("[FitZ] skip binding target=%s bone uuid=%s not in sources",
                    debugTargetUuid, debugBoneUuid);
                continue;
            }
            setting = binding.sourceSetting(bone->getUUID());
        }
        auto targetNode = findNodeByUuid(puppet->root, binding.targetUuid);
        auto target = std::dynamic_pointer_cast<Deformable>(targetNode);
        auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(targetNode);
        if (!targetNode || !target || !mapped) {
            depthBoneDebugLog("[FitZ] skip target=%s nodeNull=%s deformableNull=%s mappedNull=%s",
                binding.targetUuid, !targetNode, !target, !mapped);
            continue;
        }
        auto depths = mapped->copyDepths();
        if (depths.isNull() || depths.empty()) {
            const auto targetName = targetNode->getName();
            depthBoneDebugLog("[FitZ] skip target=%s depths=%s", targetName,
                depths.isNull() ? -1 : std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(depths.size())));
            continue;
        }
        {
            const auto targetName = targetNode->getName();
            depthBoneDebugLog("[FitZ] target=%s vertices=%s depths=%s",
                targetName, target->getVertices().size(), depths.size());
        }
        const auto targetToWorld = targetNode->transform().matrix();
        bool foundTargetSample = false;
        float bestDistanceSq = std::numeric_limits<float>::max();
        float bestDepth = 0.0f;
        if (!target->getVertices().empty()) {
            const auto vertices = target->getVertices();
            const auto vertexCount = vertices.size();
            for (std::size_t i = 0; i < vertexCount; ++i) {
                if (i >= depths.size()) break;
                const auto vertex = vertices.at(i);
                const auto sourceLocal = depthBoneSourceRestLocal(vertex.x, vertex.y, depths[i], setting, worldScale);
                if (!std::isfinite(sourceLocal.x) || !std::isfinite(sourceLocal.y) || !std::isfinite(sourceLocal.z))
                    continue;
                auto sample = transformPoint(targetToWorld, sourceLocal);
                if (!std::isfinite(sample.x) || !std::isfinite(sample.y) || !std::isfinite(sample.z)) sample = sourceLocal;
                const auto dx = sample.x - worldPoint.x;
                const auto dy = sample.y - worldPoint.y;
                const auto distanceSq = dx * dx + dy * dy;
                if (!foundTargetSample || distanceSq < bestDistanceSq) {
                    foundTargetSample = true;
                    bestDistanceSq = distanceSq;
                    bestDepth = sample.z;
                }
            }
            depthBoneDebugLog("[FitZ] scanned vertices found=%s bestDepth=%s bestDistanceSq=%s",
                foundTargetSample, bestDepth, bestDistanceSq);
        } else {
            float totalTargetDepth = 0.0f;
            std::size_t targetDepthCount = 0;
            for (auto value : depths) {
                if (!std::isfinite(value)) continue;
                totalTargetDepth += value;
                ++targetDepthCount;
            }
            if (targetDepthCount > 0) {
                const auto sourceLocal = depthBoneSourceRestLocal(0.0f, 0.0f,
                    totalTargetDepth / static_cast<float>(targetDepthCount), setting, worldScale);
                auto sample = transformPoint(targetToWorld, sourceLocal);
                if (!std::isfinite(sample.z)) sample = {worldPoint.x, worldPoint.y, sourceLocal.z};
                foundTargetSample = true;
                bestDepth = sample.z;
            }
        }
        if (foundTargetSample) {
            totalDepth += bestDepth;
            ++sampleCount;
            const auto targetName = targetNode->getName();
            depthBoneDebugLog("[FitZ] accepted target=%s bestDepth=%s sampleCount=%s",
                targetName, bestDepth, sampleCount);
        }
    }
    if (sampleCount == 0) {
        depthBoneDebugLog("[FitZ] no accepted samples");
        return false;
    }
    worldDepth = totalDepth / static_cast<float>(sampleCount);
    return true;
}

bool fitDepthBoneTranslationZToCurrentDepth(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<ExDepthBone> bone, compat::Slice<std::shared_ptr<ExDepthBone>>& changedBones,
        compat::Slice<float>& oldValues, compat::Slice<float>& newValues) {
    if (!root || !bone) return false;
    float localTranslationZ;
    float rootDepth;
    if (!ngDepthRigNodeCurrentScaledDepth(bone, localTranslationZ, rootDepth)) {
        depthBoneDebugLog("[FitZ] current scaled depth failed bone=%s", bone->getName());
        return false;
    }
    {
        const auto boneName = bone->getName();
        depthBoneDebugLog("[FitZ] fit translation bone=%s localZ=%s rootDepth=%s",
            boneName, localTranslationZ, rootDepth);
    }
    return fitDepthBoneTranslationZ(root, bone, localTranslationZ, changedBones, oldValues, newValues);
}

bool fitDepthBoneTranslationZ(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<ExDepthBone> bone,
        float localTranslationZ, compat::Slice<std::shared_ptr<ExDepthBone>>& changedBones,
        compat::Slice<float>& oldValues, compat::Slice<float>& newValues) {
    if (!root || !bone || !std::isfinite(localTranslationZ)) return false;
    const auto oldValue = bone->localTransform.translation.z;
    {
        const auto boneName = bone->getName();
        depthBoneDebugLog("[FitZ] apply translation bone=%s old=%s new=%s",
            boneName, oldValue, localTranslationZ);
    }
    if (std::abs(oldValue - localTranslationZ) <= 0.00001f) return true;
    bone->localTransform.translation.z = localTranslationZ;
    bone->localTransform.update();
    bone->transformChanged();
    changedBones.append(bone);
    oldValues.append(oldValue);
    newValues.append(localTranslationZ);
    return true;
}

std::shared_ptr<Parameter> depthFitParameter(std::shared_ptr<ExDepthRigRoot> root) {
    auto parameter = project::incArmedParameter();
    if (parameter) return parameter;
    if (lastDepthBoneDirtyRoot == root && lastDepthBoneDirtyParameter) return lastDepthBoneDirtyParameter;
    return nullptr;
}

bool fitDepthBoneBindingZToCurrentDepth(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<ExDepthBone> bone, std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint,
        std::shared_ptr<actions::GroupAction> group, bool& changed) {
    if (!root || !bone || !parameter) return false;
    float worldDepth;
    const auto translation = bone->transform().translation;
    const Vec2 worldPoint{translation.x, translation.y};
    if (!nearestScaledWorldDepthAtPoint(root, bone, worldPoint, worldDepth) &&
        !nearestScaledWorldDepthAtPoint(root, nullptr, worldPoint, worldDepth)) return false;
    if (!std::isfinite(worldDepth) || !std::isfinite(bone->transform().translation.z)) return false;
    const auto bindingZ = worldDepth - bone->transform().translation.z;
    if (!std::isfinite(bindingZ)) return false;
    const auto bindingName = nicxlive::core::common::NullableString::fromStaticLiteral("transform.t.z");
    auto binding = std::dynamic_pointer_cast<ValueParameterBinding>(parameter->getBinding(bone, bindingName));
    if (!binding) {
        binding = std::dynamic_pointer_cast<ValueParameterBinding>(parameter->createBinding(bone, bindingName));
        if (!binding) return false;
        if (group) group->addAction(actions::binding::ngCreateBindingAddRemoveAction<
            actions::binding::ParameterBindingAddAction>(parameter, binding));
    }
    const auto oldValue = binding->getValue({keypoint.x, keypoint.y});
    if (std::abs(oldValue - bindingZ) <= 0.00001f) return true;
    using ValueAction = actions::binding::ParameterBindingValueChangeAction<float, ValueParameterBinding>;
    auto action = actions::binding::ngCreateBindingValueActionDeferred<ValueAction>([&] {
        const auto label = compat::RetainedString(compat::ngRequireObject(binding)->getNameDescriptor());
        return std::tuple{label, binding,
            static_cast<int>(std::bit_cast<std::int32_t>(keypoint.x)),
            static_cast<int>(std::bit_cast<std::int32_t>(keypoint.y))};
    });
    binding->setValue({keypoint.x, keypoint.y}, bindingZ);
    action->updateNewState();
    if (group) group->addAction(action);
    else core::actionstack::incActionPush(action);
    changed = true;
    return true;
}

bool fitOneDepthBoneZToDepth(std::shared_ptr<ExDepthRigRoot> root, std::shared_ptr<ExDepthBone> bone,
        compat::Slice<std::shared_ptr<ExDepthBone>>& changedBones,
        compat::Slice<float>& oldValues, compat::Slice<float>& newValues) {
    return fitDepthBoneTranslationZToCurrentDepth(root, bone, changedBones, oldValues, newValues);
}

} // namespace detail

bool ngDepthRigNodeCurrentScaledDepth(std::shared_ptr<Node> node, float& localTranslationZ, float& rootDepth) {
    using namespace detail;
    localTranslationZ = std::numeric_limits<float>::quiet_NaN();
    rootDepth = std::numeric_limits<float>::quiet_NaN();
    if (auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(node)) {
        for (auto bone : root->depthBones()) {
            if (ngDepthRigNodeCurrentScaledDepth(bone, localTranslationZ, rootDepth)) return true;
        }
        return false;
    }
    auto bone = std::dynamic_pointer_cast<ExDepthBone>(node);
    if (!bone) return false;
    auto root = findDepthRigRoot(bone);
    if (!root) return false;
    const auto translation = bone->transform().translation;
    const Vec2 worldPoint{translation.x, translation.y};
    if (!nearestScaledWorldDepthAtPoint(root, bone, worldPoint, rootDepth) &&
        !nearestScaledWorldDepthAtPoint(root, nullptr, worldPoint, rootDepth)) return false;
    if (!std::isfinite(rootDepth)) {
        const auto boneName = bone->getName();
        depthBoneDebugLog("[FitZ] rootDepth not finite bone=%s rootDepth=%s", boneName, rootDepth);
        return false;
    }
    const auto currentWorldZ = bone->transform().translation.z;
    if (!std::isfinite(currentWorldZ)) {
        const auto boneName = bone->getName();
        depthBoneDebugLog("[FitZ] currentWorldZ not finite bone=%s currentWorldZ=%s",
            boneName, currentWorldZ);
        return false;
    }
    const auto currentLocalZ = bone->localTransform.translation.z;
    localTranslationZ = currentLocalZ + (rootDepth - currentWorldZ);
    if (!std::isfinite(localTranslationZ)) {
        const auto boneName = bone->getName();
        depthBoneDebugLog("[FitZ] localTranslationZ not finite bone=%s local=%s localZRead=%s rootDepth=%s currentWorldZ=%s",
            boneName, localTranslationZ, currentLocalZ, rootDepth, currentWorldZ);
        return false;
    }
    return true;
}

std::size_t ngDepthRigRootFittableDepthBoneCount(std::shared_ptr<ExDepthRigRoot> root) {
    if (!root) return 0;
    std::size_t result = 0;
    for (auto bone : root->depthBones()) {
        float localTranslationZ;
        float rootDepth;
        if (ngDepthRigNodeCurrentScaledDepth(bone, localTranslationZ, rootDepth)) ++result;
    }
    return result;
}

} // namespace nijigenerate::commands::depth::bone
