#pragma once

#include "nijigenerate/viewport/base.hpp"
#include "camera.hpp"
#include <compat/object_metadata.hpp>

namespace nicxlive::core::nodes { class GridDeformer; }
namespace nijigenerate::viewport::depth::mesheditor::editor { class DepthMeshEditor; }
namespace nijigenerate::viewport::depth::tools::base { class DepthEditTool; }

namespace nijigenerate::viewport::depth::viewport {

class DepthEditViewport : public ::nijigenerate::viewport::base::Viewport,
    public std::enable_shared_from_this<DepthEditViewport> {
private:
    compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> selection;
    DepthToolMode toolMode{DepthToolMode::DirectDepth};
    std::shared_ptr<mesheditor::editor::DepthMeshEditor> editor;
    DepthCamera3D depthCamera;
    DepthBrushSettings brush;

    void syncTargets();
    void leaveToModel(compat::Slice<std::shared_ptr<nicxlive::core::nodes::GridDeformer>> targets);
    bool updateDepthCamera(ImGuiIO* io);

public:
    DepthEditViewport() = default;
    DepthEditViewport(const DepthEditViewport&) = delete;
    DepthEditViewport& operator=(const DepthEditViewport&) = delete;

    std::uint32_t asyncDerivedUpdateViewportChannel() override;
    void present() override;
    void withdraw() override;
    void selectionChanged(compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> selection) override;
    virtual DepthToolMode activeToolMode();
    virtual std::shared_ptr<tools::base::DepthEditTool> activeTool();
    virtual void setToolMode(DepthToolMode mode);
    virtual std::shared_ptr<mesheditor::editor::DepthMeshEditor> getEditor();
    virtual DepthBrushSettings& brushSettings();
    virtual DepthCamera3D& depthCameraState();
    virtual void drawDepthOperationOptions();
    virtual void drawDepthOptions();
    void draw(std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void drawTools() override;
    void drawOptions() override;
    void drawConfirmBar() override;
    void update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void updatePresentation(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) override;
private:
    void prepareSourceClassInfo() override {
        ::nijigenerate::viewport::base::Viewport::prepareSourceClassInfo();
        ::nijigenerate::viewport::base::Viewport::registerSourceClassInfo<DepthEditViewport>(
            "nijigenerate.viewport.depth.viewport.DepthEditViewport");
    }
};

} // namespace nijigenerate::viewport::depth::viewport

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::viewport::DepthEditViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.viewport.DepthEditViewport)";
    inline static constexpr char unqualifiedArgument[] = "(DepthEditViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.viewport.DepthEditViewport";
    inline static constexpr char unqualifiedElement[] = "DepthEditViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::viewport::DepthEditViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.depth.viewport.DepthEditViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.depth.viewport.DepthEditViewport)";
};
}
