#pragma once
#include "node.hpp"
#include <nijigenerate/viewport/depth/common/session.hpp>
#include <nijigenerate/viewport/depth/tools/operation.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <imgui.h>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <common/resource_key_map.hpp>
#include <common/object_key_policy.hpp>
#include <nodes/grid_deformer.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <tuple>
#include <utility>

namespace nijigenerate::viewport::depth::mesheditor::action { class DepthOperationListChangeAction; }
namespace nijigenerate::viewport::depth::mesheditor::editor {
class DepthMeshEditor : public std::enable_shared_from_this<DepthMeshEditor> {
public:
    using EditorRef = std::shared_ptr<node::DepthMeshEditorOne>;
    using OperationRef = std::shared_ptr<tools::operation::DepthOperation>;
    using Operations = compat::Slice<OperationRef>;
    using Handle = tools::operation::DepthOperationHandle;
    using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
    using GridRef = std::shared_ptr<nicxlive::core::nodes::GridDeformer>;
    using Vec2 = nicxlive::core::math::Vec2;
    using CameraRef = std::shared_ptr<nicxlive::core::math::Camera>;
    using ViewRef = std::shared_ptr<common::targetview::DepthTargetView>;
    using SessionRef = std::shared_ptr<common::session::DepthViewSession>;
    using ActionRef = std::shared_ptr<action::DepthOperationListChangeAction>;
    using OperationFilter = compat::SharedDelegate<bool(OperationRef)>;
    using JsonValue = core::settings::SettingsJsonValue;
private:
    struct GridHash {
        std::size_t operator()(const GridRef& value) const {
            return nicxlive::core::ngResourceInterfaceHash(value);
        }
    };
    struct GridEqual {
        bool operator()(const GridRef& query, const GridRef& stored) const {
            return nicxlive::core::ngResourceObjectEquals(query, stored);
        }
    };
    using EditorPolicy = nicxlive::core::common::ObjectClassKeyPolicy<EditorRef>;
    using EditorMap = nicxlive::core::common::ResourceKeyMap<GridRef, EditorRef, GridHash, GridEqual>;
    EditorMap editors;
    nicxlive::core::common::ResourceKeyMap<EditorRef, Operations,
        EditorPolicy::Hash, EditorPolicy::Equal> operations;
    EditorRef selectedOperationEditor;
    std::ptrdiff_t selectedOperationIndex = -1;
    Handle selectedOperationHandle = Handle::None;
    ActionRef dragOperationAction;
    OperationRef dragStartOperation;
    Vec2 dragStartLocal{NAN, NAN};
    float dragStartMouseY = NAN;
    bool draggingOperation = false;
    nicxlive::core::common::ResourceKeyMap<EditorRef, bool,
        EditorPolicy::Hash, EditorPolicy::Equal> directDepthDirty;
    std::shared_ptr<DepthTextureMeshRenderer> renderer;
    std::shared_ptr<core::actionstack::ActionStackScope> actionScope;
    bool buildRenderResources = false;
    SessionRef viewSession;
    struct SourceAllocationTag {};
    explicit DepthMeshEditor(SourceAllocationTag) {}
    template<class> friend struct nicxlive::core::d_runtime::SourceAllocator;
    void initializeSource(bool buildRenderResources = true);
    bool editable(NodeRef node);
    void loadOperationsFromTarget(EditorRef editor);
    void syncOperationsFromTarget(EditorRef editor);
    JsonValue operationsToJson(EditorRef editor);
    JsonValue targetOperationsToJson(EditorRef editor);
public:
    explicit DepthMeshEditor(bool buildRenderResources = true);
    template<class Arguments> static std::shared_ptr<DepthMeshEditor> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<DepthMeshEditor>(
            nicxlive::core::d_runtime::SourceAllocator<DepthMeshEditor>{}, SourceAllocationTag{});
        std::apply([&](auto&&... values) {
            result->initializeSource(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual ~DepthMeshEditor() noexcept(false);
    DepthMeshEditor(const DepthMeshEditor&) = delete;
    DepthMeshEditor& operator=(const DepthMeshEditor&) = delete;
    virtual bool commitOperationAdd(EditorRef editor, OperationRef operation, std::int32_t index = -1);
    virtual bool commitOperationUpdate(EditorRef editor, std::ptrdiff_t index, OperationRef operation);
    virtual bool commitOperationRemove(EditorRef editor, std::ptrdiff_t index);
    virtual void dispose();
    virtual void closeStack();
    virtual void setTargets(compat::Slice<NodeRef> targets);
    virtual SessionRef depthViewSession();
    virtual ViewRef targetViewFor(EditorRef editor);
    virtual compat::Slice<GridRef> getTargets();
    virtual EditorRef getEditorFor(GridRef grid);
    virtual compat::Slice<EditorRef> getEditors();
    virtual void resetFromTargets();
    virtual void applyToTargets();
    virtual void draw(CameraRef camera, DepthCamera3D& depthCamera);
    virtual bool update(ImGuiIO*, CameraRef);
    virtual EditorRef findNearestProjectedVertex(Vec2 point, float radius, std::ptrdiff_t& vertexIndex);
    virtual Operations copyOperations(EditorRef editor);
    virtual std::ptrdiff_t indexOfOperationInstance(EditorRef editor, OperationRef operation);
    virtual void selectOperation(EditorRef editor, std::ptrdiff_t index);
    virtual void replaceOperations(EditorRef editor, Operations next);
    virtual void markDirectDepthDirty(EditorRef editor);
    virtual bool isDirectDepthDirty(EditorRef editor);
    virtual void replaceDirectDepths(EditorRef editor, compat::Slice<float> depths);
    virtual void beginDirectDepthEdit(EditorRef editor);
    virtual void replaceDepthState(EditorRef editor, compat::Slice<float> depths, compat::Slice<float> baseDepths,
        Operations nextOperations, bool dirty);
    virtual void appendOperation(EditorRef editor, OperationRef operation);
    virtual void recompute(EditorRef editor);
    virtual OperationRef selectedOperation();
    virtual bool findOperationHit(Vec2 mouse, DepthCamera3D& camera, EditorRef& hitEditor,
        std::ptrdiff_t& hitIndex, Handle& hitHandle, OperationFilter filter = {});
    virtual bool beginOperationDrag(Vec2 mouse, float mouseY, DepthCamera3D& camera, OperationFilter filter = {});
    virtual bool updateOperationDrag(Vec2 mouse, float mouseY, DepthCamera3D& camera, bool snapToGrid);
    virtual bool endOperationDrag();
    virtual void duplicateSelectedOperation();
    virtual void deleteSelectedOperation();
    virtual void editSelectedFloat(compat::RetainedString id, float* value, float speed, float minimum, float maximum,
        compat::RetainedString format = compat::RetainedString::fromStaticLiteral("%.2f"));
    virtual bool drawFloatInput(compat::RetainedString id, float& value, float speed, float minimum, float maximum,
        compat::RetainedString format = compat::RetainedString::fromStaticLiteral("%.2f"));
    virtual void pushSelectedOperationEdit(ActionRef action);
    virtual void drawSelectedOperationEditor();
    virtual void drawOperationOptions();
    virtual compat::RetainedString toString();
    virtual std::size_t toHash();
    virtual bool opEquals(std::shared_ptr<DepthMeshEditor> other);
    virtual int opCmp(std::shared_ptr<DepthMeshEditor> other);

protected:
    virtual void prepareSourceClassInfo();
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.mesheditor.editor.DepthMeshEditor)";
    inline static constexpr char unqualifiedArgument[] = "(DepthMeshEditor)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.mesheditor.editor.DepthMeshEditor";
    inline static constexpr char unqualifiedElement[] = "DepthMeshEditor";
};
template<> struct SourceObjectTypeNames<::nijigenerate::viewport::depth::mesheditor::editor::DepthMeshEditor> {
    inline static constexpr char className[] = "nijigenerate.viewport.depth.mesheditor.editor.DepthMeshEditor";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.viewport.depth.mesheditor.editor.DepthMeshEditor)";
};
}
