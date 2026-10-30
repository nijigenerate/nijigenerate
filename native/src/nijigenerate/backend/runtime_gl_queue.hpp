#pragma once

#include <render/common.hpp>

namespace nijigenerate::backend::runtime_gl {

// Immediate OpenGL emitter; the active backend and frame state belong to the caller.
class RenderQueue final : public nicxlive::core::RenderCommandEmitter {
public:
    void beginFrame(nicxlive::core::RenderBackend*, nicxlive::core::RenderGpuState&) override;
    void endFrame(nicxlive::core::RenderBackend*, nicxlive::core::RenderGpuState&) override;
    void drawPart(const std::shared_ptr<nicxlive::core::nodes::Part>&, bool) override;
    void drawPartPacket(const nicxlive::core::nodes::PartDrawPacket&) override;
    void beginDynamicComposite(const std::shared_ptr<nicxlive::core::nodes::Projectable>&,
                               nicxlive::core::DynamicCompositePass&) override;
    void endDynamicComposite(const std::shared_ptr<nicxlive::core::nodes::Projectable>&,
                             nicxlive::core::DynamicCompositePass&) override;
    void beginMask(bool) override;
    void applyMask(const std::shared_ptr<nicxlive::core::nodes::Drawable>&, bool) override;
    void beginMaskContent() override;
    void endMask() override;

private:
    nicxlive::core::RenderBackend* activeBackend_ = nullptr;
    nicxlive::core::RenderGpuState* frameState_ = nullptr;
    int dynamicDepth_ = 0;
    bool ready() const;
    void uploadSharedBuffers();
};

} // namespace nijigenerate::backend::runtime_gl
