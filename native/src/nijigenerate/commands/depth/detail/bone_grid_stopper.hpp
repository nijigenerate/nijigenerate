#pragma once

// Copyright 2024 nijigenerate contributors. BSD-2-Clause; see repository LICENSE.
#include <nodes/grid_deformer.hpp>
#include <nodes/node.hpp>
#include <memory>

namespace nijigenerate::commands::depth::bone::detail {

bool gridPropagationStopper(std::shared_ptr<nicxlive::core::nodes::GridDeformer> ancestor,
    std::shared_ptr<nicxlive::core::nodes::Node> target,
    std::shared_ptr<nicxlive::core::nodes::Node>& stopper);

} // namespace nijigenerate::commands::depth::bone::detail
