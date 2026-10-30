#include "nijigenerate/viewport/depth/camera.hpp"
#include <imgui.h>

namespace nijigenerate::viewport::depth {

namespace {
float clampCameraValue(float value, float lower, float upper) {
    if (value < lower) return lower;
    if (value > upper) return upper;
    return value;
}
} // namespace

bool ngUpdateDepthCamera3D(DepthCamera3D& camera, ImGuiIO* io,
    bool rotateRequested, bool panRequested, bool zoomRequested) {
    if (io == nullptr) return false;
    bool changed = false;
    if (rotateRequested) {
        camera.yaw -= io->MouseDelta.x * 0.01f;
        camera.pitch = clampCameraValue(camera.pitch + io->MouseDelta.y * 0.01f, -1.35f, 1.35f);
        changed = true;
    }
    if (panRequested) {
        camera.pan.x += io->MouseDelta.x;
        camera.pan.y += io->MouseDelta.y;
        changed = true;
    }
    if (zoomRequested && io->MouseWheel != 0) {
        camera.zoom = clampCameraValue(camera.zoom * (1 + io->MouseWheel * 0.08f), 0.1f, 8.0f);
        changed = true;
    }
    return changed;
}

} // namespace nijigenerate::viewport::depth
