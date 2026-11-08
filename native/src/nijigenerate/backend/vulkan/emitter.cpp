#include "emitter.hpp"
#include <render/commands.hpp>
#include <render/shared_deform_buffer.hpp>
#include <runtime_state.hpp>
#include <math/camera.hpp>

namespace nijigenerate::backend::vulkan {
using namespace nicxlive::core;

bool RenderQueue::ready() const {
    return activeBackend_ != nullptr && frameState_ != nullptr;
}

void RenderQueue::uploadSharedBuffers() {
    if (!activeBackend_) return;
    if (render::sharedVertexBufferDirty()) {
        auto vertices = render::sharedVertexBufferData();
        if (!vertices.empty()) activeBackend_->uploadSharedVertexBuffer(vertices);
        render::sharedVertexMarkUploaded();
    }
    if (render::sharedUvBufferDirty()) {
        auto uvs = render::sharedUvBufferData();
        if (!uvs.empty()) activeBackend_->uploadSharedUvBuffer(uvs);
        render::sharedUvMarkUploaded();
    }
    if (render::sharedDeformBufferDirty()) {
        auto data = render::sharedDeformBufferData();
        if (!data.empty()) activeBackend_->uploadSharedDeformBuffer(data);
        render::sharedDeformMarkUploaded();
    }
}

void RenderQueue::beginFrame(RenderBackend* backend, RenderGpuState& state) {
    activeBackend_ = backend;
    frameState_ = &state;
    state = RenderGpuState::init();
    dynamicDepth_ = 0;
    compositeViewports_.clear();
    uploadSharedBuffers();
}

void RenderQueue::endFrame(RenderBackend*, RenderGpuState&) {
    activeBackend_ = nullptr;
    frameState_ = nullptr;
}

void RenderQueue::drawPart(const std::shared_ptr<nodes::Part>& part, bool isMask) {
    if (!ready() || !part) return;
    activeBackend_->drawPartPacket(render::makePartDrawPacket(part, isMask));
}

void RenderQueue::drawPartPacket(const nodes::PartDrawPacket& packet) {
    if (ready()) activeBackend_->drawPartPacket(packet);
}

void RenderQueue::beginDynamicComposite(const std::shared_ptr<nodes::Projectable>&,
                                       DynamicCompositePass& pass) {
    if (!ready()) return;
    ++dynamicDepth_;
    activeBackend_->beginDynamicComposite(pass);
    const bool hasViewport=pass.surface && pass.surface->textureCount && pass.surface->textures[0];
    compositeViewports_.push_back(hasViewport);
    if(hasViewport) {
        auto texture=pass.surface->textures[0];
        inPushViewport(texture->width(),texture->height());
        auto camera=inGetCameraHandle();
        camera->scale={1,-1};
        const auto scaling=math::Mat4::scale({pass.scale.x==0?0:1/pass.scale.x,
            pass.scale.y==0?0:1/pass.scale.y,1});
        const auto offset=scaling*math::Mat4::zRotation(-pass.rotationZ);
        const auto origin=offset.transformPoint({0,0,0});
        camera->position={-origin.x,-origin.y};
    }
}

void RenderQueue::endDynamicComposite(const std::shared_ptr<nodes::Projectable>&,
                                     DynamicCompositePass& pass) {
    if (!ready()) return;
    if (dynamicDepth_ > 0) --dynamicDepth_;
    activeBackend_->endDynamicComposite(pass);
    if(!compositeViewports_.empty()) {
        if(compositeViewports_.back()) inPopViewport();
        compositeViewports_.pop_back();
    }
}

void RenderQueue::beginMask(bool useStencil) {
    if (ready()) activeBackend_->beginMask(useStencil);
}

void RenderQueue::applyMask(const std::shared_ptr<nodes::Drawable>& drawable, bool isDodge) {
    if (!ready() || !drawable) return;
    RenderBackend::MaskApplyPacket packet{};
    if (render::tryMakeMaskApplyPacket(drawable, isDodge, packet)) activeBackend_->applyMask(packet);
}

void RenderQueue::beginMaskContent() {
    if (ready()) activeBackend_->beginMaskContent();
}

void RenderQueue::endMask() {
    if (ready()) activeBackend_->endMask();
}

} // namespace nijigenerate::backend::vulkan
