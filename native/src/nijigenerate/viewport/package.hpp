#pragma once
#include "base.hpp"
#include "main_viewport.hpp"

namespace nijigenerate::viewport {
// Source public import viewport.base: retain the actual canonical declarations.
using base::Viewport;
using base::DelegationViewport;
using base::MainViewport;
using base::DraggingOnHandle;
using base::ngViewport;
using base::ngPresentTemporaryViewport;
using base::ngSetViewportAlwaysUpdate;
using base::ngGetMirrorX;
using base::ngGetMirrorX2;
using base::ngMirrorIO;
using base::ngStartedDrag;
using base::ngBeginDrag;
using base::ngEndDrag;
using base::ngDragStartedInViewport;
using base::ngDragStartedOnHandle;
using base::ngBeginDragInViewport;
using base::ngBeginDragOnHandle;
using base::ngGetDragOriginOnHandle;
using base::ngGetDragOrigValueOnHandle;
using base::ngGetDragPrevValueOnHandle;
using base::ngGetDragPrevPosOnHandle;
using base::ngSetDragPrevPosOnHandle;
using base::ngSetDragPrevValueOnHandle;
using base::ngEndDragInViewport;
using base::ngEndDragOnHandle;
using base::ngGetDragOnHandleStatus;
using base::ngViewportTransformHandle;
using base::ngViewportReset;
using base::ngGetViewportZoomMode;
using base::ngSetViewportZoomMode;
using base::ngGetViewportZoomSpeed;
using base::ngSetViewportZoomSpeed;
using base::viewportZoomMin;
using base::viewportZoomMax;
using base::shouldPostProcess;
using base::viewportTargetPosition;
using base::viewportTargetZoom;
using base::viewportZoom;
using base::viewportTransformSnap;
using base::shouldMirrorViewport;

// Call once on each application thread, before creating its first project.
void ngInitViewportModule();
}
