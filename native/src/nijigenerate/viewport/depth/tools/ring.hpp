#pragma once
#include "base.hpp"
#include "operation.hpp"
#include <cmath>

namespace nijigenerate::viewport::depth::tools::ring {
class DepthRingTool : public base::DepthEditTool, public std::enable_shared_from_this<DepthRingTool> {
    using Camera = std::shared_ptr<nicxlive::core::math::Camera>;
    using Viewport = std::shared_ptr<viewport::DepthEditViewport>;
    using Editor = std::shared_ptr<mesheditor::node::DepthMeshEditorOne>;
    using Vec2 = nicxlive::core::math::Vec2;
    enum class RingToolMode { Create, Edit };
    Editor activeEditor;
    std::ptrdiff_t startVertex = -1;
    std::ptrdiff_t currentVertex = -1;
    Vec2 startPoint{NAN, NAN};
    Vec2 currentPoint{NAN, NAN};
    RingToolMode toolMode = RingToolMode::Create;
    bool isRingOperation(std::shared_ptr<operation::DepthOperation> operation);
    void resetCreateDrag();
    void setMode(RingToolMode mode);
    void toggleMode();
    bool nearest(ImGuiIO* io, Camera camera, Viewport viewport, Editor& editor, std::ptrdiff_t& index);
    Vec2 localMouse(ImGuiIO* io, Camera camera, Viewport viewport);
    std::shared_ptr<operation::DepthRingOperation> buildOperation(Viewport viewport);
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
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::ring::DepthRingTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.tools.ring.DepthRingTool)";
    inline static constexpr char unqualifiedArgument[] = "(DepthRingTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.tools.ring.DepthRingTool";
    inline static constexpr char unqualifiedElement[] = "DepthRingTool";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::tools::ring::DepthRingTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.ring.DepthRingTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.ring.DepthRingTool)";
};
} // namespace nicxlive::core::d_runtime
