#include "mesh.hpp"
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/core/math/mesh_declarations.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::actions::mesh {
namespace {
template<class T> void prepareMeshActionOrigin() {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<T>;
    ngPrepareSourceTypeSpellings<T>();
    ngRegisterObjectTypeNames<T>(compat::RetainedString::fromStaticLiteral(Names::className),
        compat::RetainedString::fromStaticLiteral(Names::templateArgumentName));
}

void prepareMeshActionClassInfo() {
    static const bool registered = [] {
        prepareMeshActionOrigin<MeshAction>();
        prepareMeshActionOrigin<MeshConnectAction>();
        prepareMeshActionOrigin<MeshDisconnectAction>();
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(MeshAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.mesh.MeshAction"));
        ngRegisterObjectClassName(typeid(MeshConnectAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.mesh.MeshConnectAction"));
        ngRegisterObjectClassName(typeid(MeshDisconnectAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.mesh.MeshDisconnectAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool meshActionClassInfo = [] {
    prepareMeshActionClassInfo();
    return true;
}();
}
// Object.toString, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void MeshAction::prepareSourceClassInfo() {
    prepareMeshActionClassInfo();
}
compat::RetainedString MeshAction::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
namespace {
namespace operation = viewport::common::mesheditor::operations::impl;
namespace one = viewport::common::mesheditor::tools::onetimedeform;
std::shared_ptr<one::OneTimeDeformBase> tool(std::shared_ptr<nicxlive::core::nodes::Node> target) {
    const auto editor = operation::ngGetEditorFor(std::move(target));
    const auto actualTool = compat::ngRequireObject(editor)->getTool();
    return std::dynamic_pointer_cast<one::OneTimeDeformBase>(actualTool);
}
template<std::size_t N> ActionText formatted(const char (&pattern)[N], const ActionText& name) {
    const auto translated = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(pattern));
    const std::array<core::translation::TypedFormatArgument, 1> arguments{core::translation::ngFormatArgument(name)};
    auto text = core::translation::ngFormatTyped(translated.view(), arguments);
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
void remember(std::shared_ptr<MeshConnectionTable>& descriptor, MeshVertex* vertex, MeshVertex* other) {
    if (!descriptor) descriptor = std::allocate_shared<MeshConnectionTable>(
        nicxlive::core::d_runtime::SourceAllocator<MeshConnectionTable>{});
    const MeshConnectionTable::Key query{vertex, {}};
    auto found = descriptor->entries.find(query);
    if (found == descriptor->entries.end()) {
        const auto key = core::math::mesh::ngTryRetainMeshVertex(vertex);
        descriptor->entries[MeshConnectionTable::Key{vertex, key}] = Vertices{};
        found = descriptor->entries.find(query);
    }
    for (auto* existing : found->second) if (existing == other) return;
    found->second.append(other);
}
}
void MeshAction::initialize(ActionText value, EditorRef receiver, MeshRef targetMesh, MeshUpdate update) {
    name = std::move(value); editor = std::move(receiver); mesh = std::move(targetMesh);
    clear();
    const auto evaluatedEditor = editor;
    const auto targets = evaluatedEditor ? evaluatedEditor->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    if (!targets.empty()) {
        compat::Slice<SubToolMode> captured;
        captured.resize(targets.size());
        for (std::size_t i = 0; i < targets.size(); ++i) {
            const auto target = targets[i];
            captured[i] = compat::ngRequireObject(tool(target))->mode;
        }
        oldSubToolMode = captured;
    }
    if (update) { update(); updateNewState(); }
}
void MeshAction::updateNewState() {
    const auto keepAlive = shared_from_this();
    const auto evaluatedEditor = editor;
    const auto targets = evaluatedEditor ? evaluatedEditor->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    if (!targets.empty()) {
        compat::Slice<SubToolMode> captured;
        captured.resize(targets.size());
        for (std::size_t i = 0; i < targets.size(); ++i) {
            const auto target = targets[i];
            captured[i] = compat::ngRequireObject(tool(target))->mode;
        }
        newSubToolMode = captured;
    }
}
ActionText MeshAction::describe() {
    const auto receiver = weak_from_this().lock();
    return formatted("Mesh %s change is restored.", name);
}
ActionText MeshAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    return formatted("Mesh %s was edited.", name);
}
ActionText MeshAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
void MeshAction::rollback() {
    const auto keepAlive = shared_from_this();
    const auto evaluatedEditor = editor;
    const auto targets = evaluatedEditor ? evaluatedEditor->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto receiver = tool(targets[i]);
        if (i >= oldSubToolMode.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            i, oldSubToolMode.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/mesh.d"), 93);
        const auto mode = oldSubToolMode[i];
        if (!receiver) compat::ngFailNullAccess(true);
        receiver->mode = mode;
    }
}
void MeshAction::redo() {
    const auto keepAlive = shared_from_this();
    const auto evaluatedEditor = editor;
    const auto targets = evaluatedEditor ? evaluatedEditor->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto receiver = tool(targets[i]);
        if (i >= newSubToolMode.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
            i, newSubToolMode.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/mesh.d"), 103);
        const auto mode = newSubToolMode[i];
        if (!receiver) compat::ngFailNullAccess(true);
        receiver->mode = mode;
    }
}
std::shared_ptr<MeshConnectAction> MeshConnectAction::create(ActionText name, EditorRef editor, MeshRef mesh,
    MeshUpdate update) {
    auto result = std::allocate_shared<MeshConnectAction>(
        nicxlive::core::d_runtime::SourceAllocator<MeshConnectAction>{});
    result->initialize(std::move(name), std::move(editor), std::move(mesh), std::move(update));
    return result;
}
void MeshConnectAction::connect(MeshVertex* vertex, MeshVertex* other) {
    const auto keepAlive = shared_from_this();
    remember(connected, vertex, other);
    core::math::mesh::connect(vertex, other); dirty = true;
}
void MeshConnectAction::rollback() {
    const auto keepAlive = shared_from_this();
    if (undoable) {
        MeshAction::rollback();
        const auto table = connected;
        if (table) for (auto [key, values] : table->entries) {
            auto* vertex = key.get();
            const auto snapshot = values;
            for (auto* other : snapshot) core::math::mesh::disconnect(vertex, other);
        }
        undoable = false; compat::ngRequireObject(editor)->refreshMesh();
    }
}
void MeshConnectAction::redo() {
    const auto keepAlive = shared_from_this();
    if (!undoable) {
        MeshAction::redo();
        const auto table = connected;
        if (table) for (auto [key, values] : table->entries) {
            auto* vertex = key.get();
            const auto snapshot = values;
            for (auto* other : snapshot) core::math::mesh::connect(vertex, other);
        }
        undoable = true; compat::ngRequireObject(editor)->refreshMesh();
    }
}
ActionText MeshConnectAction::describe() {
    const auto receiver = weak_from_this().lock();
    return formatted("%s: vertex was translated.", name);
}
ActionText MeshConnectAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    return formatted("%s: vertex was translated.", name);
}
ActionText MeshConnectAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
std::shared_ptr<MeshDisconnectAction> MeshDisconnectAction::create(ActionText name, EditorRef editor, MeshRef mesh,
    MeshUpdate update) {
    auto result = std::allocate_shared<MeshDisconnectAction>(
        nicxlive::core::d_runtime::SourceAllocator<MeshDisconnectAction>{});
    result->initialize(std::move(name), std::move(editor), std::move(mesh), std::move(update));
    return result;
}
void MeshDisconnectAction::disconnect(MeshVertex* vertex, MeshVertex* other) {
    const auto keepAlive = shared_from_this();
    remember(connected, vertex, other);
    core::math::mesh::disconnect(vertex, other); dirty = true;
}
void MeshDisconnectAction::rollback() {
    const auto keepAlive = shared_from_this();
    if (undoable) {
        // The original intentionally does not restore MeshAction sub-tool modes here.
        const auto table = connected;
        if (table) for (auto [key, values] : table->entries) {
            auto* vertex = key.get();
            const auto snapshot = values;
            for (auto* other : snapshot) core::math::mesh::connect(vertex, other);
        }
        undoable = false; compat::ngRequireObject(editor)->refreshMesh();
    }
}
void MeshDisconnectAction::redo() {
    const auto keepAlive = shared_from_this();
    if (!undoable) {
        const auto table = connected;
        if (table) for (auto [key, values] : table->entries) {
            auto* vertex = key.get();
            const auto snapshot = values;
            for (auto* other : snapshot) core::math::mesh::disconnect(vertex, other);
        }
        undoable = true; compat::ngRequireObject(editor)->refreshMesh();
    }
}
ActionText MeshDisconnectAction::describe() {
    const auto receiver = weak_from_this().lock();
    return formatted("%s: vertex was translated.", name);
}
ActionText MeshDisconnectAction::describeUndo() {
    const auto receiver = weak_from_this().lock();
    return formatted("%s: vertex was translated.", name);
}
ActionText MeshDisconnectAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
}
