#pragma once
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>

#include "bone_math.hpp"
#include <nijigenerate/compat/vec2u.hpp>
#include <param/parameter.hpp>
#include <common/nullable_string.hpp>

namespace nijigenerate::commands::depth::bone::detail {

// These are the original runtime class fields, not cloned domain nodes.
class RuntimeDepthBone {
    static nicxlive::core::math::Vec3 nanVector() {
        const auto n = std::numeric_limits<float>::quiet_NaN();
        return {n, n, n};
    }
    static nicxlive::core::math::Mat4 nanMatrix() {
        nicxlive::core::math::Mat4 value;
        for (int row = 0; row < 4; ++row)
            for (int column = 0; column < 4; ++column)
                value[row][column] = std::numeric_limits<float>::quiet_NaN();
        return value;
    }
public:
    RuntimeDepthBone() = default;
    RuntimeDepthBone(const RuntimeDepthBone&) = delete;
    RuntimeDepthBone& operator=(const RuntimeDepthBone&) = delete;
    RuntimeDepthBone(RuntimeDepthBone&&) = delete;
    RuntimeDepthBone& operator=(RuntimeDepthBone&&) = delete;
    std::shared_ptr<ext::nodes::ExDepthBone> source;
    std::shared_ptr<RuntimeDepthBone> parent;
    nicxlive::core::math::Vec3 restHead = nanVector();
    nicxlive::core::math::Vec3 restTail = nanVector();
    float restLength = std::numeric_limits<float>::quiet_NaN();
    BoneQuaternion restQuaternion;
    BoneQuaternion localRestQuaternion;
    nicxlive::core::math::Vec3 localRestOffset = nanVector();
    nicxlive::core::math::Vec3 poseTranslation = nanVector();
    BoneQuaternion poseQuaternion;
    BoneQuaternion poseWithoutYawQuaternion;
    float poseYaw = std::numeric_limits<float>::quiet_NaN();
    nicxlive::core::math::Vec3 worldHead = nanVector();
    nicxlive::core::math::Vec3 worldTail = nanVector();
    BoneQuaternion worldQuaternion;
    BoneQuaternion worldPosePrefix;
    nicxlive::core::math::Mat4 bindMatrix = nanMatrix();
    nicxlive::core::math::Mat4 inverseBindMatrix = nanMatrix();
    nicxlive::core::math::Mat4 skinMatrix = nanMatrix();
};

// Initialized map copies retain their shared entries; null copies initialize
// independently on their first write. Missing lookup never creates a record.
class RuntimeDepthBoneMap {
    using Storage = core::selector::SelectorMap<std::uint64_t, std::shared_ptr<RuntimeDepthBone>>;
    Storage storage_;
public:
    std::shared_ptr<RuntimeDepthBone>* find(std::uint64_t uuid) {
        if (storage_.isNull()) return nullptr;
        const auto found = storage_.find(uuid);
        return found == storage_.end() ? nullptr : &found->second;
    }
    std::shared_ptr<RuntimeDepthBone> at(std::uint64_t uuid,
            std::source_location source = std::source_location::current()) const {
        return at(uuid, compat::RetainedString::fromBorrowedView(source.file_name()), source.line());
    }
    std::shared_ptr<RuntimeDepthBone> at(std::uint64_t uuid,
            compat::RetainedString sourceFile, std::size_t sourceLine) const {
        if (storage_.isNull())
            throw compat::ngStaticError<compat::RangeError>(sourceFile, sourceLine);
        const auto found = storage_.find(uuid);
        if (found == storage_.end())
            throw compat::ngStaticError<compat::RangeError>(sourceFile, sourceLine);
        return found->second;
    }
    void set(std::uint64_t uuid, std::shared_ptr<RuntimeDepthBone> value) {
        storage_[uuid] = std::move(value);
    }
};

compat::Slice<float> snapshotTargetDepths(std::shared_ptr<nicxlive::core::nodes::Deformable> target);
float parameterValue(std::shared_ptr<nicxlive::core::nodes::Node> node,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter, compat::Vec2u cursor,
    nicxlive::core::common::NullableString key, float fallback);
RuntimeDepthBoneMap buildDepthRigRuntime(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::param::Parameter> parameter = nullptr, compat::Vec2u cursor = {});
nicxlive::core::math::Mat4 depthBoneNoYawSkinMatrix(std::shared_ptr<RuntimeDepthBone> bone);
void appendVec3(compat::Slice<float>& values, nicxlive::core::math::Vec3 value);
void appendMat4(compat::Slice<float>& values, nicxlive::core::math::Mat4 value);

} // namespace nijigenerate::commands::depth::bone::detail
