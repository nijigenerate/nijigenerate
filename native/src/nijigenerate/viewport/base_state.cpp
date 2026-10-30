#include "base_internal.hpp"

namespace nijigenerate::viewport::base {

thread_local bool shouldPostProcess = false;
thread_local nicxlive::core::math::Vec2 viewportTargetPosition{0.0f, 0.0f};
thread_local float viewportTargetZoom = 1.0f;
thread_local float viewportZoom = 1.0f;
thread_local float viewportTransformSnap = 10.0f;
thread_local bool shouldMirrorViewport = false;

namespace {
thread_local std::shared_ptr<MainViewport> viewport;
}

namespace detail {
std::shared_ptr<MainViewport>& ngMainViewportStorage() { return viewport; }
}

} // namespace nijigenerate::viewport::base
