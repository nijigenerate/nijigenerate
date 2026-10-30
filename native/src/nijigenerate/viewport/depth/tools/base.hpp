#pragma once

#include "nijigenerate/compat/retained_string_slice.hpp"
#include "nijigenerate/viewport/depth/camera.hpp"
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <memory>
#include <string>

struct ImGuiIO;
namespace nicxlive::core::math { class Camera; }
namespace nijigenerate::viewport::depth::mesheditor::editor { class DepthMeshEditor; }
namespace nijigenerate::viewport::depth::mesheditor::node { class DepthMeshEditorOne; }
namespace nijigenerate::viewport::depth::viewport { class DepthEditViewport; }
namespace nijigenerate::viewport::depth::tools::base { class DepthEditTool; }
namespace nijigenerate::viewport::depth::tools {
void ngRegisterDepthEditTool(std::shared_ptr<base::DepthEditTool> tool);
}

namespace nijigenerate::viewport::depth::tools::base {

class DepthEditTool {
    friend void ::nijigenerate::viewport::depth::tools::ngRegisterDepthEditTool(
        std::shared_ptr<DepthEditTool> tool);
    std::weak_ptr<DepthEditTool> sourceOwner_;
public:
    DepthEditTool() = default;
    virtual ~DepthEditTool() = default;
    DepthEditTool(const DepthEditTool&) = delete;
    DepthEditTool& operator=(const DepthEditTool&) = delete;

    virtual DepthToolMode mode() = 0;
    virtual const char* icon() = 0;
    virtual compat::RetainedString tooltip() = 0;
    virtual void selected(std::shared_ptr<viewport::DepthEditViewport> viewport);
    virtual void draw(std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport);
    virtual void drawOptions(std::shared_ptr<viewport::DepthEditViewport> viewport);
    virtual bool update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport);
    virtual nicxlive::core::math::Vec2 screenMouseToView(
        ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera);
    virtual nicxlive::core::math::Vec2 modelToView(
        std::shared_ptr<mesheditor::node::DepthMeshEditorOne> editor,
        nicxlive::core::math::Vec2 point, float depth, std::shared_ptr<viewport::DepthEditViewport> viewport);
    virtual nicxlive::core::math::Vec2 viewToModel(
        std::shared_ptr<mesheditor::node::DepthMeshEditorOne> editor,
        nicxlive::core::math::Vec2 point, std::shared_ptr<viewport::DepthEditViewport> viewport, float depth = 0.0f);
    virtual nicxlive::core::math::Vec2 screenMouseToModel(ImGuiIO* io,
        std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<mesheditor::node::DepthMeshEditorOne> editor,
        std::shared_ptr<viewport::DepthEditViewport> viewport, float depth = 0.0f);
    virtual std::shared_ptr<mesheditor::node::DepthMeshEditorOne> nearestVertexFromScreenMouse(
        ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport, float radius, std::ptrdiff_t& index);
    virtual std::shared_ptr<mesheditor::node::DepthMeshEditorOne> nearestVertexFromScreenMouse(
        ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera,
        std::shared_ptr<viewport::DepthEditViewport> viewport, std::ptrdiff_t& index);

protected:
    virtual bool drawOptionDrag(compat::RetainedStringSlice id, float* value, float adjustSpeed,
        float minValue, float maxValue, compat::RetainedStringSlice fmt, bool sameLineAfter = true);

public:
    virtual bool drawToolButton(DepthToolMode activeMode);

    // Inherited Source Object protocol, after all existing virtual slots.
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthEditTool> other);
    virtual int opCmp(std::shared_ptr<DepthEditTool> other);
};

bool ngDepthToolObjectEquals(std::shared_ptr<DepthEditTool> left, std::shared_ptr<DepthEditTool> right);

} // namespace nijigenerate::viewport::depth::tools::base

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::tools::base::DepthEditTool> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.tools.base.DepthEditTool)";
    inline static constexpr char unqualifiedArgument[] = "(DepthEditTool)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.tools.base.DepthEditTool";
    inline static constexpr char unqualifiedElement[] = "DepthEditTool";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::tools::base::DepthEditTool> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.depth.tools.base.DepthEditTool";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.tools.base.DepthEditTool)";
};
} // namespace nicxlive::core::d_runtime
