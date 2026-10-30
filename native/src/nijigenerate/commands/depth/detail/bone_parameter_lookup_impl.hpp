#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
// Included once by the production bone.cpp intake.
#include "bone_parameter_lookup.hpp"
#include <nijigenerate/project.hpp>

namespace nijigenerate::actions::node::detail {
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::commands::depth::bone::detail {

std::shared_ptr<nicxlive::core::param::Parameter> findDepthParameterByName(compat::RetainedString name) {
    auto puppet = project::incActivePuppet();
    if (!puppet) return nullptr;
    const auto parameters = puppet->parameters;
    for (auto parameter : parameters) {
        if (!parameter) actions::node::detail::failActionObjectReference();
        if (parameter->getName().view() == name.view()) return parameter;
    }
    return nullptr;
}

} // namespace nijigenerate::commands::depth::bone::detail
