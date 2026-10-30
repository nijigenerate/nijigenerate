#pragma once

#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/viewport/depth/camera.hpp"
#include "nijigenerate/viewport/depth/common/targetview.hpp"
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

namespace nicxlive::core { class Texture; }
namespace nicxlive::core::math { class Camera; }
namespace nicxlive::core::nodes { class Node; class GridDeformer; }
namespace nijigenerate::ext::nodes { class DepthMappedNode; }
namespace nijigenerate::viewport::depth {
class DepthTargetRenderer;
class DepthTargetOffscreenTextureRenderer;
class DepthTextureMeshRenderer;
namespace mesheditor::editor { class DepthMeshEditor; }
namespace tools::directdepth { class DepthDirectDepthTool; }
}

namespace nijigenerate::viewport::depth::mesheditor::node {

inline constexpr float DepthDisplayPlaneSize=common::targetview::DepthTargetDisplayPlaneSize;
inline constexpr float DepthDisplayZScale=common::targetview::DepthTargetDisplayZScale;

class DepthMeshEditorOne : public std::enable_shared_from_this<DepthMeshEditorOne> {
    using Vec2=nicxlive::core::math::Vec2;
    using Grid=std::shared_ptr<nicxlive::core::nodes::GridDeformer>;
    using Node=std::shared_ptr<nicxlive::core::nodes::Node>;
    using View=std::shared_ptr<common::targetview::DepthTargetView>;
    Grid target;
    std::shared_ptr<ext::nodes::DepthMappedNode> depthMapped;
    std::shared_ptr<nicxlive::core::Texture> texture;
    std::uint32_t textureFbo=0;
    std::int32_t textureWidth=0;
    std::int32_t textureHeight=0;
    std::shared_ptr<DepthTargetOffscreenTextureRenderer> offscreenTextureRenderer;
    Vec2 minPoint{0.0f,0.0f};
    Vec2 maxPoint{1.0f,1.0f};
    compat::Slice<std::uint16_t> indices;
    // Actual consumers of the Source depth-package field share this storage.
    friend class nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor;
    friend class nijigenerate::viewport::depth::tools::directdepth::DepthDirectDepthTool;
    compat::Slice<Vec2> projectedPoints;
    compat::Slice<std::ptrdiff_t> selectedVertices;
    std::ptrdiff_t selectedVertex=-1;
    bool loggedEmptyVertices=false;
    bool loggedInvalidTopology=false;
    bool loggedTextureUnavailable=false;
    bool loggedFramebufferIncomplete=false;
    bool loggedDepthLengthMismatch=false;
    bool loggedFirstDraw=false;
    bool loggedOffscreenDraw=false;
    std::shared_ptr<DepthTargetRenderer> targetRenderer;
    View targetView;
    struct SourceAllocationTag {};
    explicit DepthMeshEditorOne(SourceAllocationTag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(Grid target, bool buildRenderResources = true);

    // The original empty synchronous function never retains its argument.
    void log(std::string_view message);
    compat::Slice<float> sortedUnique(compat::Slice<float> values);
    void rebuildTopology();
    void rebuildBounds();
    void rebuildTexture();
    compat::Slice<Node> vertexModeDrawableChildren();
    void renderTextureLikeVertexMode();

public:
    compat::Slice<float> depths;
    compat::Slice<float> baseDepths;
    explicit DepthMeshEditorOne(Grid target,bool buildRenderResources=true);
    template<class Arguments> static std::shared_ptr<DepthMeshEditorOne> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthMeshEditorOne>(
            nicxlive::core::d_runtime::SourceAllocator<DepthMeshEditorOne>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual ~DepthMeshEditorOne() noexcept(false);
    DepthMeshEditorOne(const DepthMeshEditorOne&)=delete;
    DepthMeshEditorOne& operator=(const DepthMeshEditorOne&)=delete;
    virtual Grid getTarget();
    virtual void bindTargetView(View view);
    virtual void syncFromTargetView();
    virtual void replaceBaseDepths(compat::Slice<float> values);
    virtual compat::Slice<Vec2> getVertices();
    virtual Vec2 localVertex(std::size_t index);
    virtual Vec2 snapLocalPoint(Vec2 point);
    virtual std::ptrdiff_t nearestLocalVertexIndex(Vec2 point);
    virtual Vec2 projectedVertex(std::size_t index);
    virtual float depthDisplayScale();
    virtual Vec2 depthViewToModel(Vec2 point,DepthCamera3D& camera,float depth=0.0f);
    virtual Vec2 modelToDepthView(Vec2 point,float depth,DepthCamera3D& camera);
    virtual Vec2 localToWorld(Vec2 point);
    virtual Vec2 projectLocalPoint(Vec2 point,float depth,DepthCamera3D& camera);
    virtual Vec2 displayWorldToLocal(Vec2 point,DepthCamera3D& camera,float depth=0.0f);
    virtual Vec2 worldToLocal(Vec2 point);
    virtual compat::Slice<float> copyEditorDepths();
    virtual Node targetNode();
    virtual void replaceEditorDepths(compat::Slice<float> values);
    virtual void resetWorkingDepths();
    virtual void clearBaseDepths();
    virtual void dispose();
    virtual void resetFromTarget();
    virtual void applyToTarget();
    virtual float getDepth(std::size_t index);
    virtual float depthAtLocalPoint(Vec2 point);
    virtual void setDepth(std::size_t index,float value);
    virtual void addDepth(std::size_t index,float value);
    virtual std::ptrdiff_t nearestProjectedVertex(Vec2 point,float radius);
    virtual bool isVertexSelected(std::ptrdiff_t index);
    virtual compat::Slice<std::ptrdiff_t> selectedVertexIndices();
    virtual void clearVertexSelection();
    virtual void selectOneVertex(std::ptrdiff_t index);
    virtual void selectVertex(std::ptrdiff_t index);
    virtual void addVertexSelection(std::ptrdiff_t index);
    virtual void deselectVertex(std::ptrdiff_t index);
    virtual void toggleVertexSelection(std::ptrdiff_t index);
    virtual compat::Slice<std::ptrdiff_t> projectedVerticesInRect(Vec2 p0,Vec2 p1);
    virtual void draw(std::shared_ptr<nicxlive::core::math::Camera> viewportCamera,
        DepthCamera3D& camera,std::shared_ptr<DepthTextureMeshRenderer> renderer);
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthMeshEditorOne> other);
    virtual int opCmp(std::shared_ptr<DepthMeshEditorOne> other);

protected:
    virtual void prepareSourceClassInfo();
};

} // namespace nijigenerate::viewport::depth::mesheditor::node

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::mesheditor::node::DepthMeshEditorOne> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.mesheditor.node.DepthMeshEditorOne)";
    inline static constexpr char unqualifiedArgument[] = "(DepthMeshEditorOne)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.mesheditor.node.DepthMeshEditorOne";
    inline static constexpr char unqualifiedElement[] = "DepthMeshEditorOne";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::mesheditor::node::DepthMeshEditorOne> {
    inline static constexpr char className[] = "nijigenerate.viewport.depth.mesheditor.node.DepthMeshEditorOne";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.depth.mesheditor.node.DepthMeshEditorOne)";
};
}
