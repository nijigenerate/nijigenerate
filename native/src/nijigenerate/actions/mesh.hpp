#pragma once
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/actions/package.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/base.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform_types.hpp>
#include <common/resource_key_map.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <memory>
#include <tuple>
#include <utility>

namespace nijigenerate::actions::mesh {
using EditorRef = std::shared_ptr<viewport::common::mesheditor::operations::base::IncMeshEditorOne>;
using MeshRef = std::shared_ptr<viewport::common::mesh::IncMesh>;
using MeshVertex = core::math::mesh::MeshVertex;
using Vertices = core::math::mesh::MeshVertexArray;
using SubToolMode = viewport::common::mesheditor::tools::onetimedeform::SubToolMode;
using MeshUpdate = compat::SharedDelegate<void()>;

class MeshAction : public LazyBoundAction, public std::enable_shared_from_this<MeshAction> {
protected:
    MeshAction() = default;
    void initialize(ActionText name, EditorRef editor, MeshRef mesh, MeshUpdate update);
    virtual void prepareSourceClassInfo();
public:
    // Preserve the original abstract class independently of constructor visibility.
    ~MeshAction() override = 0;
    MeshAction(const MeshAction&) = delete;
    MeshAction& operator=(const MeshAction&) = delete;
    MeshAction(MeshAction&&) = delete;
    MeshAction& operator=(MeshAction&&) = delete;
    ActionText name;
    bool dirty = false;
    EditorRef editor;
    MeshRef mesh;
    compat::Slice<SubToolMode> oldSubToolMode, newSubToolMode;
    struct Connection { MeshVertex* v1 = nullptr; MeshVertex* v2 = nullptr; };
    bool undoable = true;
    EditorRef& self() { return editor; }
    virtual void markAsDirty() { dirty = true; }
    void updateNewState() override;
    void clear() override { dirty = false; }
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    void rollback() override;
    void redo() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
    virtual compat::RetainedString toString();
};
inline MeshAction::~MeshAction() = default;

// Actual pointer identity is the AA key. Its existing source-vertex handle is
// retained in that same entry, not a separately appended/merged owner array.
struct MeshConnectionTable {
    struct Key {
        // Lookup borrows only the real address; inserted keys retain its actual
        // existing vertex owner. Hash/equality never dereference the query.
        MeshVertex* pointer = nullptr;
        core::math::mesh::MeshVertexHandle owner;
        MeshVertex* get() const noexcept { return pointer; }
    };
    struct Hash {
        // Selected object.d TypeInfo_Pointer.getHash, rather than std::hash.
        std::size_t operator()(const Key& key) const noexcept {
            const auto address = reinterpret_cast<std::size_t>(key.get());
            return address ^ (address >> 4);
        }
    };
    struct Equal {
        bool operator()(const Key& left, const Key& right) const noexcept { return left.get() == right.get(); }
    };
    nicxlive::core::common::ResourceKeyMap<Key, Vertices, Hash, Equal> entries;
    void clear() { entries.clear(); }
};

class MeshConnectAction : public MeshAction {
public:
    std::shared_ptr<MeshConnectionTable> connected;
    static std::shared_ptr<MeshConnectAction> create(ActionText name, EditorRef editor, MeshRef mesh,
                                                   MeshUpdate update = {});
    template<class Arguments> static std::shared_ptr<MeshConnectAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<MeshConnectAction>(
            nicxlive::core::d_runtime::SourceAllocator<MeshConnectAction>{});
        std::apply([&](auto&&... values) {
            result->initialize(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual void connect(MeshVertex* vertex, MeshVertex* other);
    void markAsDirty() override { dirty = true; }
    void updateNewState() override { MeshAction::updateNewState(); }
    void clear() override { if (connected) connected->clear(); MeshAction::clear(); }
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
};
class MeshDisconnectAction : public MeshAction {
public:
    std::shared_ptr<MeshConnectionTable> connected;
    static std::shared_ptr<MeshDisconnectAction> create(ActionText name, EditorRef editor, MeshRef mesh,
                                                      MeshUpdate update = {});
    template<class Arguments> static std::shared_ptr<MeshDisconnectAction> createSource(Arguments&& arguments) {
        auto result = std::allocate_shared<MeshDisconnectAction>(
            nicxlive::core::d_runtime::SourceAllocator<MeshDisconnectAction>{});
        std::apply([&](auto&&... values) {
            result->initialize(std::forward<decltype(values)>(values)...);
        }, std::forward<Arguments>(arguments)());
        return result;
    }
    virtual void disconnect(MeshVertex* vertex, MeshVertex* other);
    void markAsDirty() override { dirty = true; }
    void updateNewState() override { MeshAction::updateNewState(); }
    void clear() override { if (connected) connected->clear(); MeshAction::clear(); }
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
};
} // namespace nijigenerate::actions::mesh

namespace nicxlive::core::d_runtime {
#define NG_MESH_ACTION_OBJECT_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::actions::mesh::Type> { \
        inline static constexpr char className[]="nijigenerate.actions.mesh." #Type; \
        inline static constexpr char templateArgumentName[]="(nijigenerate.actions.mesh." #Type ")"; \
    }
NG_MESH_ACTION_OBJECT_NAMES(MeshAction);
NG_MESH_ACTION_OBJECT_NAMES(MeshConnectAction);
NG_MESH_ACTION_OBJECT_NAMES(MeshDisconnectAction);
#undef NG_MESH_ACTION_OBJECT_NAMES

#define NG_MESH_ACTION_TYPE_ORIGIN(Type, Qualified, Unqualified, Object) \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::mesh::Type> { \
        inline static constexpr bool isNativeObject=Object; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified),RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_MESH_ACTION_TYPE_ORIGIN(MeshAction,"nijigenerate.actions.mesh.MeshAction","MeshAction",true);
NG_MESH_ACTION_TYPE_ORIGIN(MeshConnectAction,
    "nijigenerate.actions.mesh.MeshConnectAction","MeshConnectAction",true);
NG_MESH_ACTION_TYPE_ORIGIN(MeshDisconnectAction,
    "nijigenerate.actions.mesh.MeshDisconnectAction","MeshDisconnectAction",true);
NG_MESH_ACTION_TYPE_ORIGIN(MeshAction::Connection,
    "nijigenerate.actions.mesh.MeshAction.Connection","Connection",false);
#undef NG_MESH_ACTION_TYPE_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::core::translation {

template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::mesh::MeshAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::mesh::MeshAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::mesh::MeshConnectAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::mesh::MeshConnectAction>> {};
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::mesh::MeshDisconnectAction>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::mesh::MeshDisconnectAction>> {};

} // namespace nijigenerate::core::translation
