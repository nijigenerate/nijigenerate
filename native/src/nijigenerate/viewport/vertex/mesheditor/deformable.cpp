#include "deformable.hpp"
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::vertex::mesheditor::deformable {
template class IncMeshEditorOneFor<nicxlive::core::nodes::Deformable, project::EditMode::VertexEdit>;
std::shared_ptr<VertexDeformableEditor> ngCreateVertexDeformableEditor(
    std::shared_ptr<nicxlive::core::nodes::Node>) {
    return std::allocate_shared<VertexDeformableEditor>(
        nicxlive::core::d_runtime::SourceAllocator<VertexDeformableEditor>{});
}
}
