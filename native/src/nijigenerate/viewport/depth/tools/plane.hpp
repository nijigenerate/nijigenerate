#pragma once
#include "base.hpp"
#include "operation.hpp"
#include <cmath>

namespace nijigenerate::viewport::depth::tools::plane {
class DepthPlaneTool : public base::DepthEditTool,
    public std::enable_shared_from_this<DepthPlaneTool> {
    using Camera = std::shared_ptr<nicxlive::core::math::Camera>;
    using Viewport = std::shared_ptr<viewport::DepthEditViewport>;
    using Editor = std::shared_ptr<mesheditor::node::DepthMeshEditorOne>;
    using Vec2 = nicxlive::core::math::Vec2;
    Editor activeEditor;
    std::ptrdiff_t centerVertex = -1;
    Vec2 centerPoint{NAN, NAN};
    float radiusX = NAN;
    float radiusY = NAN;
    bool nearest(ImGuiIO* io, Camera camera, Viewport viewport, Editor& editor, std::ptrdiff_t& index);
    Vec2 center();
    Vec2 localMouse(ImGuiIO* io, Camera camera, Viewport viewport);
    void updateRadius(ImGuiIO* io, Camera camera, Viewport viewport);
    void apply(Viewport viewport);
public:
    DepthToolMode mode() override;
    const char* icon() override;
    compat::RetainedString tooltip() override;
    void draw(Camera camera, Viewport viewport) override;
    void drawOptions(Viewport viewport) override;
    bool update(ImGuiIO* io, Camera camera, Viewport viewport) override;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::plane::DepthPlaneTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.tools.plane.DepthPlaneTool)";
    inline static constexpr char unqualifiedArgument[] = "(DepthPlaneTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.tools.plane.DepthPlaneTool";
    inline static constexpr char unqualifiedElement[] = "DepthPlaneTool";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::tools::plane::DepthPlaneTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.plane.DepthPlaneTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.plane.DepthPlaneTool)";
};
} // namespace nicxlive::core::d_runtime
