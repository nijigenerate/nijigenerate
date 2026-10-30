#include "package.hpp"
#include <compat/source_allocator.hpp>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor::brushes {
namespace {
thread_local compat::Slice<std::shared_ptr<base::Brush>> brushes;
template<class T, class... Args> std::shared_ptr<T> allocateBrush(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
}
compat::Slice<std::shared_ptr<base::Brush>> ngBrushList() {
    if (brushes.size() == 0) {
        brushes.append(allocateBrush<circlebrush::CircleBrush>(
            compat::RetainedString::fromStaticLiteral("Circle Brush"), 300));
        brushes.append(allocateBrush<doublethreshbrush::DoubleThreshBrush>(
            compat::RetainedString::fromStaticLiteral("Double Threshold Circle"), 300, 100));
        brushes.append(allocateBrush<rectanglebrush::RectangleBrush>(
            compat::RetainedString::fromStaticLiteral("Rectangle Brush"), 300, 300, 0.0f));
    }
    return brushes;
}

} // namespace nijigenerate::viewport::common::mesheditor::brushes
