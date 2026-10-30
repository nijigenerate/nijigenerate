#include "set_deform_binding.hpp"
#include "detail/node_deform_offsets.hpp"
#include <nodes/drawable.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate::commands::model::set_deform_binding {

thread_local CommandTable commands;

namespace detail {

bool ngNodeAcceptsDeformOffsets(
    std::shared_ptr<nicxlive::core::nodes::Node> node, std::size_t offsetCount) {
    using Drawable = nicxlive::core::nodes::Drawable;
    if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node)) {
        return drawable->Drawable::getMesh().vertices.size() == offsetCount;
    }
    if (auto deformable = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node)) {
        return deformable->getVertices().size() == offsetCount;
    }
    return false;
}

}
}
