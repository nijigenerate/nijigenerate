#include "deformable.hpp"
#include <compat/source_allocator.hpp>
namespace nijigenerate::viewport::model::mesheditor::deformable {
std::shared_ptr<ModelDeformableEditor> ngCreateModelDeformableEditor(
    std::shared_ptr<nicxlive::core::nodes::Node>) {
    return std::allocate_shared<ModelDeformableEditor>(
        nicxlive::core::d_runtime::SourceAllocator<ModelDeformableEditor>{});
}
} // namespace nijigenerate::viewport::model::mesheditor::deformable
