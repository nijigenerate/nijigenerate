#pragma once

#include <param/parameter.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/vec2u.hpp>
#include <memory>

namespace nijigenerate::commands::model::set_deform_binding::detail {

// Original module-private helper. Outputs reset before any provider operation.
bool ngResolveParameterValueToKeyPoint(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
    nicxlive::core::math::Vec2 value, compat::Vec2u& keyPoint, compat::RetainedStringSlice& message);

} // namespace nijigenerate::commands::model::set_deform_binding::detail
