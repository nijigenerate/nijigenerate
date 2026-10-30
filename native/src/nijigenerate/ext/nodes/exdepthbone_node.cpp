#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/ext/nodes/exdepthrig_create.hpp>
#include <compat/object_metadata.hpp>
#include <math/serialization.hpp>
#include <limits>

namespace nijigenerate::ext::nodes {
using namespace nicxlive::core::serde;
using nicxlive::core::nodes::SerializeNodeFlags;

ExDepthBone::ExDepthBone(FieldInitializationOnly) : Node() {}

std::shared_ptr<ExDepthBone> ExDepthBone::create(std::shared_ptr<Node> parent) {
    return createSourceDeferred([&] { return parent; });
}

std::shared_ptr<ExDepthBone> ngCreateDepthBone(std::shared_ptr<Node> parent,
        compat::RetainedString boneId, nicxlive::core::math::Vec3 restHead,
        nicxlive::core::math::Vec3 restTail, float restRoll) {
    auto bone = ExDepthBone::create(parent);
    bone->setName(boneId.toNullableString());
    bone->boneId = boneId;
    bone->restHead = restHead;
    bone->restTail = restTail;
    bone->restRoll = restRoll;
    if (auto parentBone = std::dynamic_pointer_cast<ExDepthBone>(parent)) {
        bone->localTransform.translation = {restHead.x - parentBone->restHead.x,
            restHead.y - parentBone->restHead.y, restHead.z - parentBone->restHead.z};
    } else {
        bone->localTransform.translation = restHead;
    }
    bone->localTransform.update();
    bone->transformChanged();
    return bone;
}

nicxlive::core::common::NullableString ExDepthBone::typeId() const {
    return nicxlive::core::common::NullableString::fromStaticLiteral("DepthBone");
}

void ExDepthBone::copyFrom(const Node& source, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto sourceLifetime = source.weak_from_this().lock();
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Node::copyFrom(source, clone, deepCopy);
    const auto* bone = dynamic_cast<const ExDepthBone*>(&source);
    if (!bone) return;
    boneId = bone->boneId;
    restHead = bone->restHead;
    restTail = bone->restTail;
    restRoll = bone->restRoll;
    constraintType = bone->constraintType;
    hingeAxis = bone->hingeAxis;
    lockRotation = bone->lockRotation;
    lockTranslation = bone->lockTranslation;
    allowParentToTargets = bone->allowParentToTargets;
    {
        const auto sourceLimits = bone->rotationLimits;
        rotationLimits = sourceLimits.duplicate();
    }
    maxStepRadians = bone->maxStepRadians;
}

void ExDepthBone::serializeSelfImpl(TypedJsonWriter& serializer, bool recursive,
        SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (!nicxlive::core::nodes::has_flag(flags, SerializeNodeFlags::State)) return;
    const auto vector = [&serializer](nicxlive::core::math::Vec3 value) {
        nicxlive::core::math::serialize(value, serializer);
    };
    serializer.putKey("boneId");
    {
        const auto currentBoneId = boneId;
        serializer.serializeValue(currentBoneId.view());
    }
    serializer.putKey("restHead"); vector(restHead);
    serializer.putKey("restTail"); vector(restTail);
    serializer.putKey("restRoll"); serializer.serializeValue(restRoll);
    if (!constraintType.empty()) {
        serializer.putKey("constraintType");
        const auto currentConstraintType = constraintType;
        serializer.serializeValue(currentConstraintType.view());
    }
    serializer.putKey("hingeAxis"); vector(hingeAxis);
    serializer.putKey("lockRotation"); serializer.serializeValue(lockRotation);
    serializer.putKey("lockTranslation"); serializer.serializeValue(lockTranslation);
    serializer.putKey("allowParentToTargets"); serializer.serializeValue(allowParentToTargets);
    if (!rotationLimits.empty()) {
        serializer.putKey("rotationLimits");
        const auto currentRotationLimits = rotationLimits;
        serializer.serializeValue(currentRotationLimits);
    }
    if (maxStepRadians != 0.0f) {
        serializer.putKey("maxStepRadians"); serializer.serializeValue(maxStepRadians);
    }
}

ReadError ExDepthBone::deserializeFromFghj(TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = Node::deserializeFromFghj(data)) return error;
    const auto read = [&data](const char* key, auto& value) -> ReadError {
        const auto entry = data.findFirst(key);
        if (entry.isMissing()) return {};
        return deserializeValue(entry, value);
    };
    const auto vector = [&data](const char* key, nicxlive::core::math::Vec3& value) {
        (void)nicxlive::core::math::deserialize(value, data.findFirst(key));
    };
    if (auto error = read("boneId", boneId)) return error;
    vector("restHead", restHead);
    vector("restTail", restTail);
    if (auto error = read("restRoll", restRoll)) return error;
    if (auto error = read("constraintType", constraintType)) return error;
    vector("hingeAxis", hingeAxis);
    if (auto error = read("lockRotation", lockRotation)) return error;
    if (auto error = read("lockTranslation", lockTranslation)) return error;
    if (auto error = read("allowParentToTargets", allowParentToTargets)) return error;
    const auto limits = data.findFirst("rotationLimits");
    if (!limits.isMissing()) {
        if (limits.kind() == JsonKind::Null) rotationLimits = {};
        else {
            if (limits.kind() != JsonKind::Array) return unexpectedJsonKind(limits);
            auto entries = limits.byElement();
            std::size_t count = 0;
            for (auto remaining = entries.save(); !remaining.empty(); remaining.popFront()) ++count;
            // fghj replaces with new float[0], which is null under the D runtime.
            rotationLimits = detail::newDepthRigArray<float>(
                count, std::numeric_limits<float>::quiet_NaN());
            auto currentRotationLimits = rotationLimits;
            for (auto& value : currentRotationLimits) {
                if (auto error = deserializeValue(entries.front(), value)) return error;
                entries.popFront();
            }
        }
    }
    if (auto error = read("maxStepRadians", maxStepRadians)) return error;
    return {};
}

} // namespace nijigenerate::ext::nodes
