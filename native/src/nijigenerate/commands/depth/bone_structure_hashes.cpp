#include "detail/bone_hash.hpp"
#include "detail/bone_math.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <param/binding_impl.hpp>
#include <bit>
#include <cmath>

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthBone;
using ext::nodes::ExDepthRigRoot;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::nodes::GridDeformer;
using nicxlive::core::nodes::PathDeformer;
using nicxlive::core::param::Parameter;
using nicxlive::core::param::ValueParameterBinding;

namespace {
// Preserve the original ulong-to-long bit pattern without an implementation-
// defined unsigned-to-signed C++ conversion. hashInt converts it back modulo64.
std::int64_t signedHashInput(std::uint64_t value) { return std::bit_cast<std::int64_t>(value); }
}

bool readDepthBonePoseIsSet(const std::shared_ptr<ValueParameterBinding>& binding, compat::Vec2u keypoint) {
    if (!binding) compat::ngFailNullAccess(false);
    using BaseBinding = nicxlive::core::param::ParameterBindingImpl<float>;
    return binding->BaseBinding::getIsSet()[keypoint.x][keypoint.y];
}

std::uint64_t hashDepthBoneTargetStructure(std::uint64_t hash, std::shared_ptr<Node> targetNode) {
    if (!targetNode) return hashBool(hash, false);
    hash = hashBool(hash, true);
    hash = hashStringValue(hash, compat::RetainedString(targetNode->typeId()));
    if (auto grid = std::dynamic_pointer_cast<GridDeformer>(targetNode)) {
        hash = hashStringValue(hash, compat::RetainedString::fromStaticLiteral("GridDeformer"));
        compat::Slice<float> xs;
        compat::Slice<float> ys;
        const auto vertices = grid->getVertices();
        const auto vertexCount = vertices.size();
        for (std::size_t i = 0; i < vertexCount; ++i) {
            const auto vertex = vertices.at(i);
            bool hasX = false;
            for (auto x : xs) {
                if (std::abs(x - vertex.x) <= 0.0001f) { hasX = true; break; }
            }
            if (!hasX) xs.append(vertex.x);
            bool hasY = false;
            for (auto y : ys) {
                if (std::abs(y - vertex.y) <= 0.0001f) { hasY = true; break; }
            }
            if (!hasY) ys.append(vertex.y);
        }
        hash = hashInt(hash, signedHashInput(static_cast<std::uint64_t>(xs.size())));
        hash = hashInt(hash, signedHashInput(static_cast<std::uint64_t>(ys.size())));
        hash = hashInt(hash, static_cast<std::int64_t>(grid->gridFormation()));
        hash = hashBool(hash, grid->dynamic);
        hash = hashBool(hash, grid->translateChildren);
        return hash;
    }
    if (auto path = std::dynamic_pointer_cast<PathDeformer>(targetNode)) {
        hash = hashStringValue(hash, compat::RetainedString::fromStaticLiteral("PathDeformer"));
        hash = hashInt(hash, static_cast<std::int64_t>(path->curveType));
        hash = hashInt(hash, static_cast<std::int64_t>(path->physicsType));
        hash = hashBool(hash, path->dynamicDeformation);
        hash = hashBool(hash, path->physicsOnly);
        hash = hashBool(hash, path->physicsEnabled());
        return hash;
    }
    return hash;
}

std::uint64_t depthBoneRigStructureHash(std::shared_ptr<ExDepthRigRoot> root) {
    auto puppet = project::incActivePuppet();
    std::uint64_t hash = UINT64_C(1469598103934665603);
    if (!root || !puppet) return hash;
    hash = hashInt(hash, signedHashInput(root->getUUID()));
    hash = hashMatrix(hash, root->transform().matrix());
    for (auto bone : root->depthBones()) {
        hash = hashInt(hash, signedHashInput(compat::ngRequireObject(bone)->getUUID()));
        hash = hashStringValue(hash, bone->boneId);
        hash = hashVec3(hash, bone->restHead);
        hash = hashVec3(hash, bone->restTail);
        hash = hashFloat(hash, bone->restRoll);
        hash = hashBool(hash, bone->lockRotation);
        hash = hashBool(hash, bone->lockTranslation);
        hash = hashBool(hash, bone->lockToRootValue());
        hash = hashBool(hash, bone->allowParentToTargets);
        hash = hashVec3(hash, bone->localTransform.translation);
        hash = hashVec3(hash, bone->localTransform.rotation);
        hash = hashVec2(hash, bone->localTransform.scale);
        auto parent = bone->parentRef();
        hash = hashInt(hash, parent ? signedHashInput(parent->getUUID()) : 0);
    }
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        hash = hashInt(hash, signedHashInput(binding.targetUuid));
        hash = hashInt(hash, static_cast<std::int64_t>(binding.targetKind));
        hash = hashInt(hash, static_cast<std::int64_t>(binding.influenceRule.maxInfluences));
        hash = hashFloat(hash, binding.influenceRule.radiusScale);
        hash = hashFloat(hash, binding.influenceRule.minimumRadius);
        hash = hashStringValue(hash, binding.influenceRule.falloff);
        auto sources = binding.sourceBoneUuids;
        for (auto uuid : sources) {
            hash = hashInt(hash, signedHashInput(uuid));
            auto setting = binding.sourceSetting(uuid);
            hash = hashFloat(hash, setting.weight);
            hash = hashFloat(hash, setting.depthOffset);
            hash = hashFloat(hash, setting.depthScale);
            hash = hashFloat(hash, ext::nodes::ngNormalizeDepthBoneSourceRotation(setting.rotation));
            if (const auto* multiplier = binding.influenceRule.multipliersByBoneUuid.find(uuid))
                hash = hashFloat(hash, *multiplier);
        }
        auto target = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
        hash = hashBool(hash, static_cast<bool>(target));
        if (target) {
            hash = hashBool(hash, target->lockToRootValue());
            hash = hashMatrix(hash, targetToRootMatrix(root, target));
            hash = hashDepthBoneTargetStructure(hash, target);
            if (auto deformable = std::dynamic_pointer_cast<Deformable>(target)) {
                hash = hashInt(hash, signedHashInput(static_cast<std::uint64_t>(deformable->getVertices().size())));
                const auto vertices = deformable->getVertices();
                const auto vertexCount = vertices.size();
                for (std::size_t i = 0; i < vertexCount; ++i) {
                    const auto vertex = vertices.at(i);
                    hash = hashVec2(hash, {vertex.x, vertex.y});
                }
            }
        }
    }
    return hash;
}

std::uint64_t depthBoneParameterStructureHash(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter) {
    std::uint64_t hash = UINT64_C(1099511628211);
    if (!root || !parameter) return hash;
    hash = hashInt(hash, signedHashInput(parameter->getUUID()));
    hash = hashBool(hash, parameter->isVec2);
    hash = hashVec2(hash, parameter->min);
    hash = hashVec2(hash, parameter->max);
    for (std::size_t axis = 0; axis < 2; ++axis) {
        hash = hashInt(hash, signedHashInput(static_cast<std::uint64_t>(parameter->axisPoints[axis].size())));
        auto points = parameter->axisPoints[axis];
        for (auto point : points) hash = hashFloat(hash, point);
    }
    auto bindings = parameter->bindings;
    for (auto binding : bindings) {
        auto bone = std::dynamic_pointer_cast<ExDepthBone>(
            compat::ngRequireObject(binding)->getTarget().getResource());
        if (!bone || !rootContainsBone(root, bone)) continue;
        if (!isDepthBoneTransformBindingName(compat::RetainedString(binding->getNameDescriptor()))) continue;
        hash = hashInt(hash, signedHashInput(bone->getUUID()));
        hash = hashStringValue(hash, compat::RetainedString(binding->getNameDescriptor()));
    }
    return hash;
}

std::uint64_t depthBonePoseKeyHash(std::shared_ptr<ExDepthRigRoot> root,
        std::shared_ptr<Parameter> parameter, compat::Vec2u keypoint) {
    std::uint64_t hash = UINT64_C(7809847782465536322);
    if (!root || !parameter) return hash;
    auto bindings = parameter->bindings;
    for (auto binding : bindings) {
        auto bone = std::dynamic_pointer_cast<ExDepthBone>(
            compat::ngRequireObject(binding)->getTarget().getResource());
        if (!bone || !rootContainsBone(root, bone)) continue;
        if (!isDepthBoneTransformBindingName(compat::RetainedString(binding->getNameDescriptor()))) continue;
        auto valueBinding = std::dynamic_pointer_cast<ValueParameterBinding>(binding);
        if (!valueBinding) continue;
        hash = hashInt(hash, signedHashInput(bone->getUUID()));
        hash = hashStringValue(hash, compat::RetainedString(valueBinding->getNameDescriptor()));
        const bool isSet = readDepthBonePoseIsSet(valueBinding, keypoint);
        hash = hashBool(hash, isSet);
        if (isSet) hash = hashFloat(hash, valueBinding->values[keypoint.x][keypoint.y]);
    }
    return hash;
}

} // namespace nijigenerate::commands::depth::bone::detail
