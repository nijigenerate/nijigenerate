#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <tuple>
#include <utility>
#include "package.hpp"
#include <nijigenerate/viewport/common/mesheditor/operations/base.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform_types.hpp>
#include <common/resource_key_map.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <limits>

namespace nijigenerate::actions::vertex {
using EditorRef = std::shared_ptr<viewport::common::mesheditor::operations::base::IncMeshEditorOne>;
using MeshVertex = core::math::mesh::MeshVertex;
using VertexHandle = core::math::mesh::MeshVertexHandle;
using Vec2 = nicxlive::core::math::Vec2;
using SubToolMode = viewport::common::mesheditor::tools::onetimedeform::SubToolMode;
using VertexUpdate = compat::SharedDelegate<void()>;
class VertexAction : public LazyBoundAction, public std::enable_shared_from_this<VertexAction> {
protected:
    VertexAction() = default;
    void initialize(ActionText name, EditorRef editor, VertexUpdate update);
    virtual void prepareSourceClassInfo();
public:
    // The original declares an abstract class even though it supplies its methods.
    ~VertexAction() override = 0;
    VertexAction(const VertexAction&) = delete;
    VertexAction& operator=(const VertexAction&) = delete;
    VertexAction(VertexAction&&) = delete;
    VertexAction& operator=(VertexAction&&) = delete;
    ActionText name;
    bool dirty = false;
    EditorRef editor;
    struct Connection {
        MeshVertex* v1 = nullptr;
        MeshVertex* v2 = nullptr;
        VertexHandle firstOwner, secondOwner;
        Connection(MeshVertex* first, MeshVertex* second)
            : v1(first), v2(second), firstOwner(core::math::mesh::ngTryRetainMeshVertex(first)),
              secondOwner(core::math::mesh::ngTryRetainMeshVertex(second)) {}
        Connection() = default;
    };
    compat::Slice<SubToolMode> oldSubToolMode, newSubToolMode;
    bool undoable = true;
    EditorRef& self() { return editor; }
    virtual void markAsDirty() { dirty = true; }
    void updateNewState() override;
    void rollback() override;
    void redo() override;
    void clear() override { dirty = false; }
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return true; }
    bool canMerge(ActionPtr) override { return false; }
    virtual compat::RetainedString toString();
};
inline VertexAction::~VertexAction() = default;

class VertexMoveAction : public VertexAction {
public:
    struct Translation {
        Vec2 original{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
        Vec2 translated{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
        VertexHandle owner;
    };
    struct PointerHash {
        std::size_t operator()(MeshVertex* pointer) const noexcept {
            const auto address = reinterpret_cast<std::size_t>(pointer);
            return address ^ (address >> 4);
        }
    };
    nicxlive::core::common::ResourceKeyMap<MeshVertex*, Translation,
        PointerHash, std::equal_to<MeshVertex*>> translations;
    static std::shared_ptr<VertexMoveAction> create(ActionText name, EditorRef editor, VertexUpdate update = {});
    template<class Arguments> static std::shared_ptr<VertexMoveAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<VertexMoveAction>(
            nicxlive::core::d_runtime::SourceAllocator<VertexMoveAction>{});
        std::apply([&](auto&&... values) {
            result->initialize(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual void moveVertex(MeshVertex* vertex, Vec2 position);
    void markAsDirty() override { dirty = true; }
    void updateNewState() override { VertexAction::updateNewState(); }
    void clear() override { translations.clear(); VertexAction::clear(); }
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
};
template<bool reverse = false> class VertexInsertRemoveAction : public VertexAction {
    void synchronizeTarget();
public:
    struct Entry {
        std::int32_t index = 0;
        MeshVertex* vertex = nullptr;
        VertexHandle owner;
        Entry() = default;
        Entry(std::int32_t position, MeshVertex* value)
            : index(position), vertex(value), owner(core::math::mesh::ngTryRetainMeshVertex(value)) {}
    };
    compat::Slice<Entry> vertices;
    compat::Slice<Connection> connections;
    static std::shared_ptr<VertexInsertRemoveAction> create(ActionText name, EditorRef editor,
                                                            VertexUpdate update = {});
    template<class Arguments> static std::shared_ptr<VertexInsertRemoveAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<VertexInsertRemoveAction>(
            nicxlive::core::d_runtime::SourceAllocator<VertexInsertRemoveAction>{});
        std::apply([&](auto&&... values) {
            result->initialize(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual void insertVertex(std::int32_t index, MeshVertex* vertex);
    virtual void addVertex(MeshVertex* vertex) { insertVertex(-1, vertex); }
    virtual void removeVertex(MeshVertex* vertex, bool executeAction = true);
    virtual void removeVertices();
    void markAsDirty() override { dirty = true; }
    void updateNewState() override { VertexAction::updateNewState(); }
    void clear() override { vertices.resize(0); VertexAction::clear(); }
    template<bool mode> void action();
    void rollback() override;
    void redo() override;
    template<bool mode> ActionText actionName();
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
};
using VertexInsertAction = VertexInsertRemoveAction<false>;
using VertexAddAction = VertexInsertRemoveAction<false>;
using VertexRemoveAction = VertexInsertRemoveAction<true>;
class VertexReorderAction : public VertexAction {
public:
    core::math::mesh::MeshVertexArray vertices;
    static std::shared_ptr<VertexReorderAction> create(ActionText name, EditorRef editor, VertexUpdate update = {});
    template<class Arguments> static std::shared_ptr<VertexReorderAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<VertexReorderAction>(
            nicxlive::core::d_runtime::SourceAllocator<VertexReorderAction>{});
        std::apply([&](auto&&... values) {
            result->initialize(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    void markAsDirty() override { dirty = true; }
    void updateNewState() override { VertexAction::updateNewState(); }
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
};
extern template class VertexInsertRemoveAction<false>;
extern template class VertexInsertRemoveAction<true>;
} // namespace nijigenerate::actions::vertex

namespace nicxlive::core::d_runtime {
#define NG_VERTEX_ACTION_OBJECT_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::actions::vertex::Type> { \
        static RetainedString className() { \
            return RetainedString::fromStaticLiteral("nijigenerate.actions.vertex." #Type); \
        } \
        static RetainedString templateArgumentName() { \
            return RetainedString::fromStaticLiteral("(nijigenerate.actions.vertex." #Type ")"); \
        } \
    }
NG_VERTEX_ACTION_OBJECT_NAMES(VertexAction);
NG_VERTEX_ACTION_OBJECT_NAMES(VertexMoveAction);
NG_VERTEX_ACTION_OBJECT_NAMES(VertexReorderAction);
#undef NG_VERTEX_ACTION_OBJECT_NAMES

template<bool reverse> struct SourceObjectTypeNames<
        ::nijigenerate::actions::vertex::VertexInsertRemoveAction<reverse>> {
    static RetainedString className() {
        if constexpr (reverse) return RetainedString::fromStaticLiteral(
            "nijigenerate.actions.vertex.VertexInsertRemoveAction!true.VertexInsertRemoveAction");
        else return RetainedString::fromStaticLiteral(
            "nijigenerate.actions.vertex.VertexInsertRemoveAction!false.VertexInsertRemoveAction");
    }
    static RetainedString templateArgumentName() {
        if constexpr (reverse) return RetainedString::fromStaticLiteral(
            "(nijigenerate.actions.vertex.VertexInsertRemoveAction!true)");
        else return RetainedString::fromStaticLiteral(
            "(nijigenerate.actions.vertex.VertexInsertRemoveAction!false)");
    }
};

#define NG_VERTEX_ACTION_TYPE_ORIGIN(Type, Qualified, Unqualified, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::vertex::Type> { \
        inline static constexpr bool isNativeObject=Object; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified),RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_VERTEX_ACTION_TYPE_ORIGIN(VertexAction,"nijigenerate.actions.vertex.VertexAction","VertexAction",true);
NG_VERTEX_ACTION_TYPE_ORIGIN(VertexMoveAction,
    "nijigenerate.actions.vertex.VertexMoveAction","VertexMoveAction",true);
NG_VERTEX_ACTION_TYPE_ORIGIN(VertexReorderAction,
    "nijigenerate.actions.vertex.VertexReorderAction","VertexReorderAction",true);
NG_VERTEX_ACTION_TYPE_ORIGIN(VertexInsertRemoveAction<false>,
    "nijigenerate.actions.vertex.VertexInsertRemoveAction!false","VertexInsertRemoveAction!false",true);
NG_VERTEX_ACTION_TYPE_ORIGIN(VertexInsertRemoveAction<true>,
    "nijigenerate.actions.vertex.VertexInsertRemoveAction!true","VertexInsertRemoveAction!true",true);
NG_VERTEX_ACTION_TYPE_ORIGIN(VertexAction::Connection,
    "nijigenerate.actions.vertex.VertexAction.Connection","Connection",false);
NG_VERTEX_ACTION_TYPE_ORIGIN(VertexMoveAction::Translation,
    "nijigenerate.actions.vertex.VertexMoveAction.Translation","Translation",false);
#undef NG_VERTEX_ACTION_TYPE_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::vertex::VertexAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::vertex::VertexAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::vertex::VertexMoveAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::vertex::VertexMoveAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::vertex::VertexReorderAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::vertex::VertexReorderAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::vertex::VertexInsertRemoveAction<false>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::vertex::VertexInsertRemoveAction<false>>> {};
template<> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::vertex::VertexInsertRemoveAction<true>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::vertex::VertexInsertRemoveAction<true>>> {};

} // namespace nijigenerate::core::translation
