#pragma once

#include "base.hpp"
#include "operation.hpp"
#include <limits>

namespace nijigenerate::viewport::depth::tools::attachedpoint {

class DepthAttachedPointTool : public base::DepthEditTool,
    public std::enable_shared_from_this<DepthAttachedPointTool> {
private:
    std::shared_ptr<mesheditor::node::DepthMeshEditorOne> activeEditor;
    std::ptrdiff_t activeVertex{-1};
    float dragOriginY{std::numeric_limits<float>::quiet_NaN()};
    std::shared_ptr<operation::DepthAttachedPointOperation> operation;

    bool beginDrag(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport);
    void updateDrag(ImGuiIO* io, std::shared_ptr<viewport::DepthEditViewport> viewport);
    void endDrag(std::shared_ptr<viewport::DepthEditViewport> viewport);

public:
    DepthAttachedPointTool() = default;
    DepthAttachedPointTool(const DepthAttachedPointTool&) = delete;
    DepthAttachedPointTool& operator=(const DepthAttachedPointTool&) = delete;
    DepthToolMode mode() override;
    const char* icon() override;
    compat::RetainedString tooltip() override;
    bool update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport) override;
};

} // namespace nijigenerate::viewport::depth::tools::attachedpoint

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::depth::tools::attachedpoint::DepthAttachedPointTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.tools.attachedpoint.DepthAttachedPointTool)";
    inline static constexpr char unqualifiedArgument[] = "(DepthAttachedPointTool)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.tools.attachedpoint.DepthAttachedPointTool";
    inline static constexpr char unqualifiedElement[] = "DepthAttachedPointTool";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::depth::tools::attachedpoint::DepthAttachedPointTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.attachedpoint.DepthAttachedPointTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.attachedpoint.DepthAttachedPointTool)";
};
} // namespace nicxlive::core::d_runtime
