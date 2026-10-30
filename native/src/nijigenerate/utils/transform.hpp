#pragma once

#include <param/binding.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <cstdint>
#include <memory>

namespace nijigenerate::windows::flipconfig {
// Canonical M325 declaration identity only; its fields and bodies have separate gates.
class FlipPair;
}

namespace nijigenerate::utils::transform {

std::shared_ptr<nicxlive::core::param::ParameterBinding> ngBindingGetPairFor(
    std::shared_ptr<nicxlive::core::param::Parameter> parameter,
    std::shared_ptr<nicxlive::core::nodes::Node> target,
    std::shared_ptr<windows::flipconfig::FlipPair> pair,
    compat::RetainedStringSlice name, bool forceCreate = false);

void ngBindingAutoFlip(
    std::shared_ptr<nicxlive::core::param::ParameterBinding> binding,
    std::shared_ptr<nicxlive::core::param::ParameterBinding> sourceBinding,
    compat::Vec2u index, std::int32_t axis, bool extrapolation = true,
    compat::Slice<std::uint64_t>* selected = nullptr);

} // namespace nijigenerate::utils::transform
