#pragma once
#include "attachedpoint.hpp"
#include "base.hpp"
#include "../camera.hpp"
#include "directdepth.hpp"
#include "operation.hpp"
#include "plane.hpp"
#include "ring.hpp"

namespace nijigenerate::viewport::depth::tools {
// Reexport the actual declarations from the seven source public imports.
using attachedpoint::DepthAttachedPointTool;
using base::DepthEditTool;
using nijigenerate::viewport::depth::DepthToolMode;
using nijigenerate::viewport::depth::DepthBrushSettings;
using nijigenerate::viewport::depth::DepthCamera3D;
using nijigenerate::viewport::depth::updateDepthCamera3D;
using nijigenerate::viewport::depth::focusGridOrigin;
using nijigenerate::viewport::depth::projectDepthPoint;
using nijigenerate::viewport::depth::unprojectDepthPoint;
using directdepth::DepthDirectDepthTool;
using operation::DepthOperationColor;
using operation::DepthOperationSelectedColor;
using operation::DepthOperationPositiveColor;
using operation::DepthOperationPositiveSelectedColor;
using operation::DepthOperationNegativeColor;
using operation::DepthOperationNegativeSelectedColor;
using operation::DepthOperationHandleColor;
using operation::DepthOperationAmountColor;
using operation::depthToolRound;
using operation::depthOperationColor;
using operation::DepthOperation;
using operation::DepthOperationHandle;
using operation::distanceToSegment;
using operation::drawDepthPoint;
using operation::DepthAttachedPointOperation;
using operation::DepthRingOperation;
using operation::DepthPlaneOperation;
using operation::toExDepthOp;
using operation::depthOperationFromExDepthOp;
using operation::applyRingDepth;
using operation::RingOrientation;
using operation::RingSample;
using operation::ringOrientation;
using operation::ringCrossCoord;
using operation::ringSampleAt;
using operation::combineRingSamplesByCross;
using operation::hermiteDepth;
using operation::applyRingFamilySurface;
using operation::applyRingNormalSurfaces;
using operation::ngComputeDepthsFromOps;
using operation::ringBaseDepthAt;
using operation::ringEndpointPoint;
using operation::ringCurvePoint;
using operation::ringAmountHandlePoint;
using operation::ringAngleHandlePoint;
using operation::ringCurveDistance;
using operation::drawDepthPolyline;
using operation::drawDepthRingCurve;
using operation::ellipseNorm;
using operation::applyPlaneFlatten;
using operation::drawDepthLine;
using operation::drawDepthLinePoints;
using operation::drawDepthEllipse;
using plane::DepthPlaneTool;
using ring::DepthRingTool;

using DepthToolRef = std::shared_ptr<base::DepthEditTool>;
void ngRegisterDepthEditTool(DepthToolRef tool);
compat::Slice<DepthToolRef> ngDepthEditTools();
DepthToolRef ngDepthEditTool(DepthToolMode mode);
}
