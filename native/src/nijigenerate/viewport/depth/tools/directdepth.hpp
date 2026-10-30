#pragma once
#include "base.hpp"
#include "../mesheditor/action.hpp"
#include <limits>

namespace nijigenerate::viewport::depth::tools::directdepth {
class DepthDirectDepthTool : public base::DepthEditTool,
    public std::enable_shared_from_this<DepthDirectDepthTool> {
    using Editor = std::shared_ptr<mesheditor::node::DepthMeshEditorOne>;
    using Viewport = std::shared_ptr<viewport::DepthEditViewport>;
    using Camera = std::shared_ptr<nicxlive::core::math::Camera>;
    using Vec2 = nicxlive::core::math::Vec2;
    using Indices = compat::Slice<std::ptrdiff_t>;
    Editor activeEditor, pendingEditor, maybeSelectOneEditor;
    std::ptrdiff_t activeVertex = -1, pendingVertex = -1, maybeSelectOneVertex = -1;
    bool selecting = false, selectAdd = false, selectRemove = false;
    Vec2 selectOriginView{NAN, NAN}, selectCurrentView{NAN, NAN};
    Vec2 dragOriginView{NAN, NAN}, depthAxisView{NAN, NAN};
    float startDepth = NAN;
    compat::Slice<float> startDepths;
    Indices draggedVertices;
    std::shared_ptr<mesheditor::action::DepthEditorDepthChangeAction> action;
    float visibleDepthDelta(Vec2 currentView);
    void applyClickSelection(ImGuiIO* io, Editor editor, std::ptrdiff_t index);
    bool hitVertex(ImGuiIO* io, Camera camera, Viewport viewport, Editor& editor, std::ptrdiff_t& index);
    bool beginPending(ImGuiIO* io, Camera camera, Viewport viewport);
    bool beginDepthDrag(Viewport viewport);
    void updateDrag(ImGuiIO* io, Camera camera, Viewport viewport);
    void endDrag();
    void updateSelection(ImGuiIO* io, Camera camera, Viewport viewport);
    void endSelection(Viewport viewport);
    void commitMaybeSelectOne();
    void clearPending();
    void drawRect(Vec2 p0, Vec2 p1);
    void drawEditorSelection(Editor editor, std::ptrdiff_t hotIndex, Indices candidates = {});
public:
    DepthDirectDepthTool() = default;
    DepthDirectDepthTool(const DepthDirectDepthTool&) = delete;
    DepthDirectDepthTool& operator=(const DepthDirectDepthTool&) = delete;
    DepthToolMode mode() override;
    const char* icon() override;
    compat::RetainedString tooltip() override;
    void draw(Camera camera, Viewport viewport) override;
    bool update(ImGuiIO* io, Camera camera, Viewport viewport) override;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::depth::tools::directdepth::DepthDirectDepthTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.tools.directdepth.DepthDirectDepthTool)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDirectDepthTool)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.tools.directdepth.DepthDirectDepthTool";
    inline static constexpr char unqualifiedElement[] = "DepthDirectDepthTool";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::depth::tools::directdepth::DepthDirectDepthTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.directdepth.DepthDirectDepthTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.directdepth.DepthDirectDepthTool)";
};
} // namespace nicxlive::core::d_runtime
