#pragma once

#include "depthbone_publishers.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <param/binding.hpp>
#include <cstdint>
#include <memory>

namespace nijigenerate::actions::binding::detail {

inline void notifyBindingValueChanged(std::shared_ptr<nicxlive::core::param::ParameterBinding> binding,
    int pointx, int pointy, bool notifyDepthBone) {
    if (auto target = compat::ngRequireObject(binding)->getTarget().target.lock())
        target->notifyChange(target, nicxlive::core::nodes::NotifyReason::AttributeChanged);
    if (notifyDepthBone) {
        depthboneinvalidation::ngNotifyDepthBoneBindingValueChanged(binding,
            compat::Vec2u{static_cast<std::uint32_t>(pointx), static_cast<std::uint32_t>(pointy)});
    }
}

} // namespace nijigenerate::actions::binding::detail
