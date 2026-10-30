#pragma once

#include <cstddef>
#include <memory>

namespace nicxlive::core::nodes { class Node; }

namespace nijigenerate::commands::model::set_deform_binding::detail {

bool ngNodeAcceptsDeformOffsets(
    std::shared_ptr<nicxlive::core::nodes::Node> node, std::size_t offsetCount);

}
