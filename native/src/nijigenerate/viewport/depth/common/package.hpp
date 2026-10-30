#pragma once

#include "session.hpp"
#include "targetview.hpp"

namespace nijigenerate::viewport::depth::common {

using session::ngDepthViewWorkingDepthsChangeAction;
using session::DepthViewSession;

using targetview::DepthTargetDisplayPlaneSize;
using targetview::DepthTargetDisplayZScale;
using targetview::ngDepthDisplayScaleForBounds;
using targetview::ngDepthTargetRoundDepth;
using targetview::ngDepthDisplayScaleForDocument;
using targetview::ngDepthTargetClampDepth;
using targetview::ngDepthDisplayScaleForTargets;
using targetview::ngDepthDisplayScaleForTargetsInNodeSpace;
using targetview::ngDepthDisplayScaleForTarget;
using targetview::DepthTargetView;

} // namespace nijigenerate::viewport::depth::common
