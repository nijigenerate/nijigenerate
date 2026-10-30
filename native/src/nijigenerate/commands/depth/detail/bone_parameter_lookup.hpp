#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include <nijigenerate/compat/retained_string.hpp>
#include <param/parameter.hpp>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

std::shared_ptr<nicxlive::core::param::Parameter> findDepthParameterByName(compat::RetainedString name);

} // namespace nijigenerate::commands::depth::bone::detail
