#include "package.hpp"
#include "point.hpp"
#include "connect.hpp"
#include "pathdeform.hpp"
#include "grid.hpp"
#include "brush.hpp"
#include "lasso.hpp"
#include "bezierdeform.hpp"
#include "onetimedeform.hpp"
#include <nijigenerate/viewport/vertex/mesheditor/tools/edgecutter.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::common::mesheditor::tools {
compat::Slice<std::shared_ptr<base::ToolInfo>> incGetToolInfo() {
    // Preserve per-thread initialization, source registration order and the
    // shared backing of the returned descriptor.
    if (detail::infoList.empty()) {
        using PointInfo = point::ToolInfoImpl<point::PointTool>;
        detail::infoList.append(std::allocate_shared<PointInfo>(
            nicxlive::core::d_runtime::SourceAllocator<PointInfo>{}));
        using ConnectInfo = connect::ToolInfoImpl<connect::ConnectTool>;
        detail::infoList.append(std::allocate_shared<ConnectInfo>(
            nicxlive::core::d_runtime::SourceAllocator<ConnectInfo>{}));
        using PathInfo = pathdeform::ToolInfoImpl<pathdeform::PathDeformTool>;
        detail::infoList.append(std::allocate_shared<PathInfo>(
            nicxlive::core::d_runtime::SourceAllocator<PathInfo>{}));
        using GridInfo = grid::ToolInfoImpl<grid::GridTool>;
        detail::infoList.append(std::allocate_shared<GridInfo>(
            nicxlive::core::d_runtime::SourceAllocator<GridInfo>{}));
        using BrushInfo = brush::ToolInfoImpl<brush::BrushTool>;
        detail::infoList.append(std::allocate_shared<BrushInfo>(
            nicxlive::core::d_runtime::SourceAllocator<BrushInfo>{}));
        using LassoInfo = lasso::ToolInfoImpl<lasso::LassoTool>;
        detail::infoList.append(std::allocate_shared<LassoInfo>(
            nicxlive::core::d_runtime::SourceAllocator<LassoInfo>{}));
        using EdgeInfo = vertex::mesheditor::tools::edgecutter::ToolInfoImpl<
            vertex::mesheditor::tools::edgecutter::EdgeCutterTool>;
        detail::infoList.append(std::allocate_shared<EdgeInfo>(
            nicxlive::core::d_runtime::SourceAllocator<EdgeInfo>{}));
        using BezierInfo = bezierdeform::ToolInfoImpl<bezierdeform::BezierDeformTool>;
        detail::infoList.append(std::allocate_shared<BezierInfo>(
            nicxlive::core::d_runtime::SourceAllocator<BezierInfo>{}));
        using OneTimeGridInfo = onetimedeform::OneTimeDeformToolInfo<nicxlive::core::nodes::GridDeformer>;
        detail::infoList.append(std::allocate_shared<OneTimeGridInfo>(
            nicxlive::core::d_runtime::SourceAllocator<OneTimeGridInfo>{}));
        using OneTimePathInfo = onetimedeform::OneTimeDeformToolInfo<nicxlive::core::nodes::PathDeformer>;
        detail::infoList.append(std::allocate_shared<OneTimePathInfo>(
            nicxlive::core::d_runtime::SourceAllocator<OneTimePathInfo>{}));
    }
    return detail::infoList;
}
}
