#include "package.hpp"
#include "nijigenerate/core/asyncderivedupdate.hpp"
#include "nijigenerate/project.hpp"
#include "nijigenerate/compat/native_access_fault.hpp"

namespace nijigenerate::viewport::anim {

void ngViewportAnimOverlay() {}

std::uint32_t AnimationViewport::asyncDerivedUpdateViewportChannel() {
    return static_cast<std::uint32_t>(core::asyncderivedupdate::AsyncDerivedUpdateViewportChannel::Model);
}

void AnimationViewport::draw(std::shared_ptr<nicxlive::core::math::Camera>) {
    const auto keepAlive = weak_from_this().lock();
    compat::ngRequireObject(project::incActivePuppet())->update();
    compat::ngRequireObject(project::incActivePuppet())->draw();
}

} // namespace nijigenerate::viewport::anim
