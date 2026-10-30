#pragma once
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/deformable.hpp>
#include <nodes/drawable.hpp>
#include <concepts>
namespace nijigenerate::actions::mesheditor {
class DeformationAction;
template<class T> class MeshEditorAction;
}
namespace nijigenerate::viewport::vertex::mesheditor::deformable {
// Each specialization keeps its actual type identity; the original body is
// independent of T/mode beyond the declaration constraint.
template<class T, project::EditMode mode>
requires (mode == project::EditMode::VertexEdit &&
    std::derived_from<T, nicxlive::core::nodes::Deformable> &&
    !std::derived_from<T, nicxlive::core::nodes::Drawable>)
class IncMeshEditorOneFor : public common::mesheditor::operations::impl::IncMeshEditorOneDeformable {
    using Base = common::mesheditor::operations::impl::IncMeshEditorOneDeformable;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        common::mesheditor::operations::impl::detail::prepareEditorClassRecord<IncMeshEditorOneFor<T, mode>, T>(
            "nijigenerate.viewport.vertex.mesheditor.deformable.IncMeshEditorOneFor!(",
            ", EditMode.VertexEdit).IncMeshEditorOneFor");
    }
    void substituteMeshVertices(MeshVertex*) override;
    std::shared_ptr<actions::mesheditor::MeshEditorAction<actions::mesheditor::DeformationAction>> editorAction;
    ActionRef getDeformActionImpl(bool reset);
public:
    using Vec2Array = nicxlive::core::math::Vec2Array;
    IncMeshEditorOneFor() : Base(false) {}
    IncMeshEditorOneFor(const IncMeshEditorOneFor&) = delete;
    IncMeshEditorOneFor& operator=(const IncMeshEditorOneFor&) = delete;
    IncMeshEditorOneFor(IncMeshEditorOneFor&&) = delete;
    IncMeshEditorOneFor& operator=(IncMeshEditorOneFor&&) = delete;
    void setTarget(NodeRef target) override;
    void resetMesh() override;
    void refreshMesh() override;
    void importMesh(nicxlive::core::nodes::MeshData& data) override;
    void mergeMesh(nicxlive::core::nodes::MeshData& data, Mat4 matrix) override;
    void applyOffsets(Vec2Array offsets) override;
    Vec2Array getOffsets() override;
    void applyToTarget() override;
    void applyPreview() override;
    void pushDeformAction() override;
    std::uint64_t getVertexFromPoint(Vec2 position) override;
    compat::Slice<float> getVerticesInBrush(Vec2 position, BrushRef brush) override;
    void removeVertexAt(Vec2) override;
    bool removeVertex(ImGuiIO*, bool) override;
    bool addVertex(ImGuiIO*) override;
    bool updateChanged(bool changed) override;
    void addMeshVertex(MeshVertex*) override;
    std::int32_t indexOfMesh(MeshVertex*) override;
    void insertMeshVertex(std::int32_t, MeshVertex*) override;
    void removeMeshVertex(MeshVertex*) override;
    void moveMeshVertex(MeshVertex*, Vec2) override;
    bool isPointOver(Vec2 position) override;
    Indices getInRect(Vec2 min, Vec2 max, std::uint32_t groupId = 0) override;
    Indices filterVertices(VertexFilter filter) override;
    void createPathTarget() override;
    Mat4 updatePathTarget() override;
    void resetPathTarget() override;
    void remapPathTarget(PathRef& path, Mat4 transform) override;
    bool hasAction() override;
    void updateAddVertexAction(MeshVertex*) override;
    void clearAction() override;
    void markActionDirty() override;
    ActionRef getDeformAction() override;
    ActionRef getCleanDeformAction() override;
    template<bool reset = false> ActionRef getDeformActionImpl() { return getDeformActionImpl(reset); }
    void forceResetAction() override;
    void draw(CameraRef camera) override;
    void adjustPathTransform() override;
};

using VertexDeformableEditor = IncMeshEditorOneFor<nicxlive::core::nodes::Deformable, project::EditMode::VertexEdit>;

std::shared_ptr<VertexDeformableEditor> ngCreateVertexDeformableEditor(
    std::shared_ptr<nicxlive::core::nodes::Node> target);
}

#include "deformable.tpp"
