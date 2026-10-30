#pragma once
#include "catalog.hpp"
#include "enums.hpp"
#include "select.hpp"
#include "point.hpp"
#include "connect.hpp"
#include "pathdeform.hpp"
#include "bezierdeform.hpp"
#include "grid.hpp"
#include "brush.hpp"
#include "lasso.hpp"
#include "onetimedeform.hpp"
#include <nijigenerate/viewport/vertex/mesheditor/tools/edgecutter.hpp>
#include <concepts>
#include <type_traits>

namespace nijigenerate::viewport::common::mesheditor::tools {
using enums::VertexToolMode;
using base::Tool;
using base::Draggable;
using base::ToolInfo;
using base::ToolInfoBase;
using select::NodeSelect;
using point::PointTool;
using connect::ConnectTool;
using pathdeform::PathDeformTool;
using bezierdeform::BezierDeformTool;
using grid::GridTool;
using brush::BrushTool;
using brush::TileColumns;
using brush::TileCount;
using brush::OptimizationInitialStride;
using brush::OptimizationFinalStrideThreshold;
using brush::OptimizationParameterEpsilon;
using brush::OptimizationMoveEpsilon;
using lasso::LassoType;
using lasso::LassoIO;
using lasso::LassoTool;
using onetimedeform::OneTimeDeformer;
using onetimedeform::OneTimeDeformBase;
using onetimedeform::OneTimeDeform;
using onetimedeform::SubToolMode;
using onetimedeform::ngGetArmedParameterEditorFor;
using onetimedeform::ngCurrentNodeFilter;
using vertex::mesheditor::tools::edgecutter::EdgeCutterTool;

namespace detail {
// Resolve the imported Source template family to the same existing concrete type.
template<class T> struct ImportedToolInfoType;
template<class T> requires std::derived_from<T, PointTool>
struct ImportedToolInfoType<T> : std::type_identity<point::ToolInfoImpl<T>> {};
template<class T> requires std::derived_from<T, ConnectTool>
struct ImportedToolInfoType<T> : std::type_identity<connect::ToolInfoImpl<T>> {};
template<class T> requires std::derived_from<T, PathDeformTool>
struct ImportedToolInfoType<T> : std::type_identity<pathdeform::ToolInfoImpl<T>> {};
template<class T> requires std::derived_from<T, BezierDeformTool>
struct ImportedToolInfoType<T> : std::type_identity<bezierdeform::ToolInfoImpl<T>> {};
template<class T> requires std::derived_from<T, GridTool>
struct ImportedToolInfoType<T> : std::type_identity<grid::ToolInfoImpl<T>> {};
template<class T> requires std::derived_from<T, BrushTool>
struct ImportedToolInfoType<T> : std::type_identity<brush::ToolInfoImpl<T>> {};
template<class T> requires std::derived_from<T, LassoTool>
struct ImportedToolInfoType<T> : std::type_identity<lasso::ToolInfoImpl<T>> {};
template<class T> requires std::derived_from<T, EdgeCutterTool>
struct ImportedToolInfoType<T> : std::type_identity<vertex::mesheditor::tools::edgecutter::ToolInfoImpl<T>> {};
template<class T> requires (std::derived_from<T, OneTimeDeform<onetimedeform::GridDeformer>> ||
                           std::derived_from<T, OneTimeDeform<onetimedeform::PathDeformer>>)
struct ImportedToolInfoType<T> : std::type_identity<onetimedeform::ToolInfoImpl<T>> {};
}

template<class T> using ToolInfoImpl = typename detail::ImportedToolInfoType<T>::type;
}
