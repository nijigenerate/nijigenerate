#pragma once

#include "bone_runtime.hpp"
#include "../bone_gpu_packet.hpp"
#include <nijigenerate/compat/retained_string_slice.hpp>

namespace nijigenerate::commands::depth::bone::detail {

class DepthBoneIndexMap {
    using Storage = core::selector::SelectorMap<std::uint64_t, std::uint32_t>;
    Storage storage_;
public:
    const std::uint32_t* find(std::uint64_t uuid) const {
        if (storage_.isNull()) return nullptr;
        const auto found = storage_.find(uuid);
        return found == storage_.end() ? nullptr : &found->second;
    }
    void set(std::uint64_t uuid, std::uint32_t value) {
        storage_[uuid] = value;
    }
};

// Actual separately acquired S419 body lives in bone_packet_source.cpp.
bool appendDepthBoneGpuSourceData(ext::nodes::ExDepthRigBinding* binding,
    compat::Slice<std::shared_ptr<ext::nodes::ExDepthBone>> sourceBones,
    DepthBoneIndexMap boneIndices, RuntimeDepthBoneMap runtime,
    std::shared_ptr<nicxlive::core::nodes::Deformable> target,
    compat::Slice<float> scaledDepths, nicxlive::core::math::Mat4 targetToRoot, float worldScale,
    compat::Slice<float>& sourceData, compat::Slice<float>& sourceInputs, compat::RetainedStringSlice& error);

} // namespace nijigenerate::commands::depth::bone::detail
