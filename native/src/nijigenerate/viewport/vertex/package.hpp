#pragma once
#include <memory>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/vertex/automesh/package.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <math/veca.hpp>
#include <compat/object_metadata.hpp>
namespace nicxlive::core::nodes { class Deformable; class Drawable; }
namespace nijigenerate::viewport::common::mesh { class IncMesh; }
namespace nijigenerate::viewport::common::mesheditor { class IncMeshEditor; }
namespace nijigenerate::viewport::vertex {
namespace automesh { class AutoMeshProcessor; }
using namespace automesh;
using DeformableRef = std::shared_ptr<nicxlive::core::nodes::Deformable>;
class VertexViewport : public base::Viewport, public std::enable_shared_from_this<VertexViewport> {
protected:
    std::shared_ptr<common::mesheditor::IncMeshEditor> editor;
    friend std::shared_ptr<common::mesheditor::IncMeshEditor> ngVertexViewportGetEditor();
    friend void incVertexEditSetTarget(DeformableRef);
public:
    VertexViewport() = default;
    VertexViewport(const VertexViewport&) = delete;
    VertexViewport& operator=(const VertexViewport&) = delete;
    std::uint32_t asyncDerivedUpdateViewportChannel() override;
    void draw(std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void drawTools() override;
    void drawOptions() override;
    void drawConfirmBar() override;
    void update(ImGuiIO* io, std::shared_ptr<nicxlive::core::math::Camera> camera) override;
    void withdraw() override;
    void present() override;
    virtual void copyMeshDataToTarget(DeformableRef target, DeformableRef source);
    virtual void mergeMeshDataToTarget(DeformableRef target, DeformableRef source);
    virtual void apply();
    virtual void clear();
    virtual void reset();
private:
    void prepareSourceClassInfo() override {
        base::Viewport::prepareSourceClassInfo();
        base::Viewport::registerSourceClassInfo<VertexViewport>("nijigenerate.viewport.vertex.VertexViewport");
    }
};
std::shared_ptr<VertexViewport> incVertexViewport();
void incVertexEditStartEditing(DeformableRef target);
void incVertexEditSetTarget(DeformableRef target);
void incVertexEditCopyMeshDataToTarget(DeformableRef target, DeformableRef source);
void incVertexEditMergeMeshDataToTarget(DeformableRef target, DeformableRef source);
bool incMeshEditGetIsApplySafe();
void incMeshEditApply();
void incMeshEditClear();
void incMeshEditReset();
std::shared_ptr<automesh::AutoMeshProcessor> ngActiveAutoMeshProcessor();
void ngActiveAutoMeshProcessor(std::shared_ptr<automesh::AutoMeshProcessor> processor);
compat::Slice<std::shared_ptr<automesh::AutoMeshProcessor>> ngAutoMeshProcessors();
void ngInitializeVertexViewportModule();
std::shared_ptr<common::mesheditor::IncMeshEditor> ngVertexViewportGetEditor();
inline auto incVertexViewportGetEditor() { return ngVertexViewportGetEditor(); }
bool ngApplyDrawableMeshFromCommand(std::shared_ptr<nicxlive::core::nodes::Drawable> target,
    std::shared_ptr<common::mesh::IncMesh> mesh, compat::RetainedString& message);
bool ngApplyDeformableVerticesFromCommand(DeformableRef target,
    nicxlive::core::math::Vec2Array positions, compat::RetainedString& message);
void ngRefreshDeformableCommandEditors(DeformableRef target);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::vertex::VertexViewport> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.vertex.VertexViewport)";
    inline static constexpr char unqualifiedArgument[] = "(VertexViewport)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.vertex.VertexViewport";
    inline static constexpr char unqualifiedElement[] = "VertexViewport";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::vertex::VertexViewport> {
    inline static constexpr char className[] = "nijigenerate.viewport.vertex.VertexViewport";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.vertex.VertexViewport)";
};
}
