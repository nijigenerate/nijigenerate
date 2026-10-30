#pragma once
#include "operations/package.hpp"
#include "tools/base.hpp"
#include <nijigenerate/compat/retained_string.hpp>
#include <common/object_key_policy.hpp>
#include <compat/object_metadata.hpp>
#include <cstddef>
#include <memory>

namespace nijigenerate::viewport::common::mesheditor {
using operations::IncMeshEditorOne;
using operations::IncMeshEditorOneImpl;
using operations::IncMeshEditorOneDeformable;
using operations::IncMeshEditorOneDrawable;
using operations::IncMeshEditorOneFor;
using operations::getVertices;
using operations::setVertices;
using operations::toVertices;
using operations::toMVertices;
using operations::resize;
using operations::toBool;
using operations::drawPointSubset;
using operations::ngRefineWeldingSeams;
using operations::incUpdateWeldedPoints;
using operations::ngGetEditorFor;

class IncMeshEditor : public std::enable_shared_from_this<IncMeshEditor> {
public:
    using Editor = operations::base::IncMeshEditorOne;
    using EditorRef = std::shared_ptr<Editor>;
    using NodeRef = Editor::NodeRef;
    using Nodes = compat::Slice<NodeRef>;
    using CameraRef = Editor::CameraRef;
    using VertexToolMode = Editor::VertexToolMode;
protected:
    tools::base::Editors editors;
    bool previewTriangulate = false, mirrorHoriz = false, mirrorVert = false;
    VertexToolMode toolMode = VertexToolMode::Points;
public:
    bool deformOnly = false;
    explicit IncMeshEditor(bool deform) : deformOnly(deform) {}
    IncMeshEditor(const IncMeshEditor&) = delete;
    IncMeshEditor& operator=(const IncMeshEditor&) = delete;
    virtual ~IncMeshEditor() = default;
    virtual void setTarget(NodeRef target);
    virtual EditorRef getEditorFor(NodeRef drawing);
    virtual void addTarget(NodeRef target) = 0;
    virtual void setTargets(Nodes targets) = 0;
    virtual void removeTarget(NodeRef target);
    virtual Nodes getTargets();
    virtual void refreshMesh();
    virtual void abortActiveTools();
    virtual void releasePendingActions();
    virtual void resetMesh();
    virtual void applyPreview();
    virtual void applyToTarget();
    virtual bool update(ImGuiIO* io, CameraRef camera);
    virtual void draw(CameraRef camera);
    virtual VertexToolMode getToolMode() { return toolMode; }
    virtual void setToolMode(VertexToolMode mode);
    virtual void viewportTools();
    virtual void displayGroupIds();
    virtual void displayToolOptions();
    virtual void resetSelection();
    virtual void flipSelection();
    virtual void popupMenu();
    virtual void setMirrorHoriz(bool value);
    virtual bool getMirrorHoriz() { return mirrorHoriz; }
    virtual void setMirrorVert(bool value);
    virtual bool getMirrorVert() { return mirrorVert; }
    virtual void setPreviewTriangulate(bool value);
    virtual bool getPreviewTriangulate() { return previewTriangulate; }
    virtual bool previewingTriangulation();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<IncMeshEditor> other);
    virtual int opCmp(std::shared_ptr<IncMeshEditor> other);
protected:
    // Source ClassInfo metadata is supplied by each actual class's own module.
    template<class T, std::size_t N> static void registerSourceClassInfo(const char (&name)[N]) {
        static const bool registered = [&name] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T),
                compat::RetainedString::fromStaticLiteral(name));
            return true;
        }();
        (void)registered;
    }
    virtual void prepareSourceClassInfo();
};
bool ngIncMeshEditorObjectEquals(std::shared_ptr<IncMeshEditor> left,
    std::shared_ptr<IncMeshEditor> right);
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::common::mesheditor::IncMeshEditor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.IncMeshEditor)";
    inline static constexpr char unqualifiedArgument[] = "(IncMeshEditor)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.IncMeshEditor";
    inline static constexpr char unqualifiedElement[] = "IncMeshEditor";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::common::mesheditor::IncMeshEditor> {
    inline static constexpr char className[] = "nijigenerate.viewport.common.mesheditor.IncMeshEditor";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.viewport.common.mesheditor.IncMeshEditor)";
};
}
