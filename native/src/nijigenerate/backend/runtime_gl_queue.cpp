#include "runtime_gl_queue.hpp"
#include <render/commands.hpp>
#include <render/shared_deform_buffer.hpp>

namespace nijigenerate::backend::runtime_gl {
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
}

void RenderQueue::endDynamicComposite(const std::shared_ptr<nodes::Projectable>&,
                                     DynamicCompositePass& pass) {
    if (!ready()) return;
    if (dynamicDepth_ > 0) --dynamicDepth_;
    activeBackend_->endDynamicComposite(pass);
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

} // namespace nijigenerate::backend::runtime_gl
