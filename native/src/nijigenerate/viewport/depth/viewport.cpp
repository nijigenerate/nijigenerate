#include "viewport.hpp"
#include "nijigenerate/core/asyncderivedupdate.hpp"

namespace nijigenerate::viewport::depth::viewport {

std::uint32_t DepthEditViewport::asyncDerivedUpdateViewportChannel() {
    return static_cast<std::uint32_t>(core::asyncderivedupdate::AsyncDerivedUpdateViewportChannel::Depth);
}

DepthToolMode DepthEditViewport::activeToolMode() {
    return toolMode;
}

std::shared_ptr<mesheditor::editor::DepthMeshEditor> DepthEditViewport::getEditor() {
    return editor;
}

DepthBrushSettings& DepthEditViewport::brushSettings() {
    return brush;
}

DepthCamera3D& DepthEditViewport::depthCameraState() {
    return depthCamera;
}

} // namespace nijigenerate::viewport::depth::viewport
