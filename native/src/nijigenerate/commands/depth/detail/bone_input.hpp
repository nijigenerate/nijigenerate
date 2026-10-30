#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <cstddef>
#include <memory>
#include <string_view>

namespace nijigenerate::commands::depth::bone::detail {

[[noreturn]] void failInput(std::string_view text, std::size_t sourceLine);

template<std::size_t N> [[noreturn]] void failInput(const char (&text)[N], std::size_t sourceLine) {
    failInput(std::string_view(text, N - 1), sourceLine);
}

nicxlive::core::math::Vec3 vec3From(compat::Slice<float> values, compat::RetainedStringSlice name);
std::shared_ptr<ext::nodes::ExDepthRigRoot> requireRoot(std::shared_ptr<nicxlive::core::nodes::Node> node);
std::shared_ptr<ext::nodes::ExDepthBone> requireBone(std::shared_ptr<nicxlive::core::nodes::Node> node);
std::shared_ptr<ext::nodes::ExDepthBone> requireSourceBone(std::shared_ptr<ext::nodes::ExDepthRigRoot> root,
    std::shared_ptr<nicxlive::core::nodes::Node> node);
ext::nodes::ExDepthTargetKind targetKindOf(std::shared_ptr<nicxlive::core::nodes::Node> node);

} // namespace nijigenerate::commands::depth::bone::detail
