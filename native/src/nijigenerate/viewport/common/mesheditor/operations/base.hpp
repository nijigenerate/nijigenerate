#pragma once
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/math/mesh_vertices.hpp>
#include <nijigenerate/viewport/common/mesheditor/brushes/base.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/enums.hpp>
#include <math/camera.hpp>
#include <math/mat4.hpp>
#include <math/veca.hpp>
#include <nodes/drawable.hpp>
#include <imgui.h>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>

namespace nijigenerate::actions { class Action; }
namespace nijigenerate::actions::mesh { class MeshDisconnectAction; }
namespace nijigenerate::viewport::common::mesh { class IncMesh; }
namespace nijigenerate::viewport::common::spline { class CatmullSpline; }
namespace nijigenerate::viewport::common::mesheditor::tools::base { class Tool; }

namespace nijigenerate::viewport::common::mesheditor::operations::base {

class IncMeshEditorOne : public std::enable_shared_from_this<IncMeshEditorOne> {
public:
    // Concrete source classes are heap-owned. Their logical constructors must
    // run after shared ownership exists before invoking any callback-bearing method.
    using MeshVertex = core::math::mesh::MeshVertex;
    using Vec2 = nicxlive::core::math::Vec2;
    using Mat4 = nicxlive::core::math::Mat4;
    using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
    using MeshRef = std::shared_ptr<viewport::common::mesh::IncMesh>;
    using PathRef = std::shared_ptr<viewport::common::spline::CatmullSpline>;
    using ToolRef = std::shared_ptr<tools::base::Tool>;
    using CameraRef = std::shared_ptr<nicxlive::core::math::Camera>;
    using BrushRef = std::shared_ptr<brushes::base::Brush>;
    using ActionRef = std::shared_ptr<actions::Action>;
    using Indices = compat::Slice<std::uint64_t>;
    using VertexPointers = core::math::mesh::MeshVertexArray;
    using VertexFilter = std::shared_ptr<const std::function<bool(MeshVertex*)>>;
    using EdgeVisitor = std::shared_ptr<const std::function<void(MeshVertex*, MeshVertex*)>>;
    using MirrorVisitor = std::shared_ptr<const std::function<void(std::uint32_t)>>;
    using VertexToolMode = tools::enums::VertexToolMode;

    explicit IncMeshEditorOne(bool deform) : deformOnly(deform) {}
    virtual ~IncMeshEditorOne() = default;
    IncMeshEditorOne(const IncMeshEditorOne&) = delete;
    IncMeshEditorOne& operator=(const IncMeshEditorOne&) = delete;
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<IncMeshEditorOne> other);
    virtual int opCmp(std::shared_ptr<IncMeshEditorOne> other);

    virtual void substituteMeshVertices(MeshVertex* vertex) = 0;
    virtual std::uint64_t getVertexFromPoint(Vec2 position) = 0;
    virtual compat::Slice<float> getVerticesInBrush(Vec2 position, BrushRef brush) = 0;
    virtual void removeVertexAt(Vec2 vertex) = 0;
    virtual bool removeVertex(ImGuiIO* io, bool selectedOnly) = 0;
    virtual bool addVertex(ImGuiIO* io) = 0;
    virtual bool updateChanged(bool changed) = 0;
    virtual void addMeshVertex(MeshVertex* vertex) = 0;
    virtual void insertMeshVertex(std::int32_t index, MeshVertex* vertex) = 0;
    virtual void removeMeshVertex(MeshVertex* vertex) = 0;
    virtual std::int32_t indexOfMesh(MeshVertex*) { return -1; }
    virtual void moveMeshVertex(MeshVertex* vertex, Vec2 position) = 0;
    virtual bool isPointOver(Vec2 position) = 0;
    virtual Indices getInRect(Vec2 min, Vec2 max, std::uint32_t groupId) = 0;
    virtual Indices filterVertices(VertexFilter filter) = 0;
    virtual VertexPointers getVerticesByIndex(Indices indices, bool removeNull = false) = 0;
    virtual bool hasAction() = 0;
    virtual void updateAddVertexAction(MeshVertex* vertex) = 0;
    virtual void clearAction() = 0;
    virtual void markActionDirty() = 0;

    bool deformOnly = false;
    bool vertexMapDirty = false;
    VertexToolMode toolMode = VertexToolMode::Points;
    Indices selected;
    Indices mirrorSelected;
    Indices newSelected;
    Vec2 lastMousePos{nan(), nan()};
    Vec2 mousePos{nan(), nan()};
    bool isSelecting = false;
    bool mutateSelection = false;
    bool invertSelection = false;
    std::uint64_t maybeSelectOne = 0;
    std::uint64_t vtxAtMouse = 0;
    Vec2 selectOrigin{nan(), nan()};
    MeshRef previewMesh;

    virtual void updateSelectionPreview(ImGuiIO* io) {
        const auto keepAlive = shared_from_this();
        if (!isSelecting) return;
        const auto origin = selectOrigin;
        const auto position = mousePos;
        const auto group = getGroupId();
        newSelected = getInRect(origin, position, group);
        mutateSelection = compat::ngRequireObject(io)->KeyShift;
        invertSelection = compat::ngRequireObject(io)->KeyCtrl;
    }

    bool deforming = false;
    float meshEditAOE = 4.0f;
    bool previewTriangulate = false;
    bool mirrorHoriz = false;
    bool mirrorVert = false;
    Vec2 mirrorOrigin{0.0f, 0.0f};
    Mat4 transform = Mat4::identity();
    nicxlive::core::math::Vec4 vertexColor{1.0f, 1.0f, 1.0f, 1.0f};
    nicxlive::core::math::Vec4 edgeColor{0.5f, 0.5f, 0.5f, 1.0f};

    virtual bool isSelected(std::uint64_t index) {
        for (auto value : selected) if (value == index) return true;
        return false;
    }
    virtual void toggleSelect(std::uint64_t index) {
        const auto keepAlive = shared_from_this();
        const auto found = findSelected(index);
        if (found < selected.size()) removeSelected(found);
        else selected.append(index);
        updateMirrorSelected();
    }
    virtual void select(std::uint64_t index) {
        const auto keepAlive = shared_from_this();
        if (!isSelected(index)) selected.append(index);
        updateMirrorSelected();
    }
    virtual void deselect(std::uint64_t index) {
        const auto keepAlive = shared_from_this();
        const auto found = findSelected(index);
        if (found < selected.size()) removeSelected(found);
        updateMirrorSelected();
    }
    virtual std::uint64_t selectOne(std::uint64_t index) {
        const auto keepAlive = shared_from_this();
        auto last = std::numeric_limits<std::uint64_t>::max();
        if (!selected.empty()) last = selected[selected.size() - 1];
        selected = Indices{index};
        const auto selectedVertices = getVerticesByIndex(Indices{index});
        if (selectedVertices.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0,
            selectedVertices.size(), compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/common/mesheditor/operations/base.d"), 144);
        auto* vertex = selectedVertices[0];
        updateAddVertexAction(vertex);
        if (last != std::numeric_limits<std::uint64_t>::max()) updateMirrorSelected();
        return last;
    }
    virtual void deselectAll() {
        const auto keepAlive = shared_from_this();
        selected.resize(0);
        clearAction();
        updateMirrorSelected();
    }
    virtual Vec2 mirrorH(Vec2 point) {
        return {2.0f * mirrorOrigin.x + -point.x, 2.0f * 0.0f + point.y};
    }
    // The original uses mirrorOrigin.x for the vertical axis too.
    virtual Vec2 mirrorV(Vec2 point) {
        return {2.0f * 0.0f + point.x, 2.0f * mirrorOrigin.x + -point.y};
    }
    virtual Vec2 mirrorHV(Vec2 point) {
        return {2.0f * mirrorOrigin.x - point.x, 2.0f * mirrorOrigin.y - point.y};
    }
    virtual Vec2 mirror(std::uint32_t axis, Vec2 point) {
        const auto keepAlive = shared_from_this();
        switch (axis) {
        case 0: return point;
        case 1: return mirrorH(point);
        case 2: return mirrorV(point);
        case 3: return mirrorHV(point);
        default: compat::ngAssertUnreachable("bad axis",
            "source/nijigenerate/viewport/common/mesheditor/operations/base.d", 175);
        }
    }
    virtual Vec2 mirrorDelta(std::uint32_t axis, Vec2 point) {
        switch (axis) {
        case 0: return point;
        case 1: return {-point.x, point.y};
        case 2: return {point.x, -point.y};
        case 3: return {-point.x, -point.y};
        default: compat::ngAssertUnreachable("bad axis",
            "source/nijigenerate/viewport/common/mesheditor/operations/base.d", 185);
        }
    }
    virtual std::uint64_t mirrorVertex(std::uint32_t axis, std::uint64_t index);
    virtual MeshVertex* mirrorVertex(std::uint32_t axis, MeshVertex* vertex);
    virtual bool isOnMirror(Vec2 point, float aoe) {
        return (mirrorVert && point.y > -aoe && point.y < aoe) ||
            (mirrorHoriz && point.x > -aoe && point.x < aoe);
    }
    virtual bool isOnMirrorCenter(Vec2 point, float aoe) {
        return (mirrorVert && point.y > -aoe && point.y < aoe) &&
            (mirrorHoriz && point.x > -aoe && point.x < aoe);
    }
    virtual void placeOnMirror(Vec2 point, float aoe);
    virtual void foreachMirror(MirrorVisitor visitor) {
        const auto keepAlive = shared_from_this();
        const auto invoke = [&](std::uint32_t axis) {
            const auto callable = compat::ngRequireObject(visitor);
            if (!*callable) compat::ngFailNullAccess(false);
            (*callable)(axis);
        };
        if (mirrorHoriz) invoke(1);
        if (mirrorVert) invoke(2);
        if (mirrorHoriz && mirrorVert) invoke(3);
        invoke(0);
    }
    virtual void updateMirrorSelected();
    virtual VertexToolMode getToolMode() { return toolMode; }
    virtual void setToolMode(VertexToolMode mode) = 0;
    virtual void finalizeToolMode() = 0;
    virtual void abortToolMode() = 0;
    virtual bool previewingTriangulation() { return previewTriangulate && toolMode == VertexToolMode::Points; }
    virtual NodeRef getTarget() = 0;
    virtual void setTarget(NodeRef target) = 0;
    virtual compat::Slice<NodeRef> getFilterTargets() = 0;
    virtual void addFilterTarget(NodeRef parent) = 0;
    virtual void removeFilterTarget(NodeRef parent) = 0;
    virtual void resetMesh() = 0;
    virtual void refreshMesh() = 0;
    virtual void importMesh(nicxlive::core::nodes::MeshData& data) = 0;
    virtual void mergeMesh(nicxlive::core::nodes::MeshData& data, Mat4 matrix) = 0;
    virtual void applyOffsets(nicxlive::core::math::Vec2Array offsets) = 0;
    virtual nicxlive::core::math::Vec2Array getOffsets() = 0;
    virtual void applyToTarget() = 0;
    virtual void applyPreview() = 0;
    virtual void createPathTarget() = 0;
    virtual Mat4 updatePathTarget() = 0;
    virtual void resetPathTarget() = 0;
    virtual void remapPathTarget(PathRef& path, Mat4 transform) = 0;
    virtual void pushDeformAction() = 0;
    virtual ActionRef getDeformAction() = 0;
    virtual ActionRef getCleanDeformAction() = 0;
    virtual void forceResetAction() = 0;
    // Break only the discarded session's pending-action ownership edges. Undo
    // entries retain their editor references and remain independently usable.
    virtual void releasePendingActions() {
        const auto keepAlive = shared_from_this();
        forceResetAction();
    }
    virtual void forEachEdge(EdgeVisitor visitor) = 0;
    virtual std::shared_ptr<actions::mesh::MeshDisconnectAction> newMeshDisconnectAction() = 0;
    virtual std::int32_t peek(ImGuiIO* io, CameraRef camera) = 0;
    virtual std::int32_t unify(compat::Slice<std::int32_t> actions) = 0;
    virtual bool update(ImGuiIO* io, CameraRef camera, std::int32_t actions) = 0;
    virtual void draw(CameraRef camera) = 0;
    virtual PathRef getPath() = 0;
    virtual void setPath(PathRef path) = 0;
    virtual void adjustPathTransform() = 0;
    virtual ToolRef getTool() = 0;
    virtual std::uint32_t getGroupId() = 0;
    virtual void setGroupId(std::uint32_t groupId) = 0;

protected:
    virtual void prepareSourceClassInfo();
private:
    static float nan() { return std::numeric_limits<float>::quiet_NaN(); }
    std::size_t findSelected(std::uint64_t index) {
        for (std::size_t i = 0; i < selected.size(); ++i) if (selected[i] == index) return i;
        return selected.size();
    }
    void removeSelected(std::size_t index) {
        for (std::size_t i = index + 1; i < selected.size(); ++i) selected[i - 1] = selected[i];
        selected = selected.slice(0, selected.size() - 1);
    }
};

} // namespace nijigenerate::viewport::common::mesheditor::operations::base

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::viewport::common::mesheditor::operations::base::IncMeshEditorOne> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.operations.base.IncMeshEditorOne)";
    inline static constexpr char unqualifiedArgument[] = "(IncMeshEditorOne)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.operations.base.IncMeshEditorOne";
    inline static constexpr char unqualifiedElement[] = "IncMeshEditorOne";
};
template<> struct SourceObjectTypeNames<
        ::nijigenerate::viewport::common::mesheditor::operations::base::IncMeshEditorOne> {
    inline static constexpr char className[] =
        "nijigenerate.viewport.common.mesheditor.operations.base.IncMeshEditorOne";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.operations.base.IncMeshEditorOne)";
};
}
