#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <cstdint>
#include <memory>

namespace nijigenerate::ext::nodes { class ExDepthRigRoot; }
namespace nicxlive::core::param { class Parameter; }

namespace nijigenerate::commands::depth::bone {

compat::Slice<double> ngDepthBoneKeyProjection(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::uint64_t boneUuid, std::shared_ptr<nicxlive::core::param::Parameter> parameter,
    compat::Vec2u key);

} // namespace nijigenerate::commands::depth::bone
