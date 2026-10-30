#include "vertex.hpp"
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/onetimedeform.hpp>
#include <nijigenerate/viewport/common/mesh.hpp>
#include <nijigenerate/core/math/mesh.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>

namespace nijigenerate::actions::vertex {
namespace one = viewport::common::mesheditor::tools::onetimedeform;
namespace impl = viewport::common::mesheditor::operations::impl;
namespace {
template<class T> void prepareVertexActionOrigin() {
    using namespace nicxlive::core::d_runtime;
    using Names=SourceObjectTypeNames<T>;
    ngPrepareSourceTypeSpellings<T>();
    ngRegisterObjectTypeNames<T>(Names::className(),Names::templateArgumentName());
}

void prepareVertexActionClassInfo() {
    static const bool registered = [] {
        prepareVertexActionOrigin<VertexAction>();
        prepareVertexActionOrigin<VertexMoveAction>();
        prepareVertexActionOrigin<VertexReorderAction>();
        prepareVertexActionOrigin<VertexInsertRemoveAction<false>>();
        prepareVertexActionOrigin<VertexInsertRemoveAction<true>>();
        using nicxlive::core::d_runtime::ngRegisterObjectClassName;
        ngRegisterObjectClassName(typeid(VertexAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.vertex.VertexAction"));
        ngRegisterObjectClassName(typeid(VertexMoveAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.vertex.VertexMoveAction"));
        ngRegisterObjectClassName(typeid(VertexReorderAction),
            compat::RetainedString::fromStaticLiteral("nijigenerate.actions.vertex.VertexReorderAction"));
        ngRegisterObjectClassName(typeid(VertexInsertRemoveAction<false>),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.vertex.VertexInsertRemoveAction!false.VertexInsertRemoveAction"));
        ngRegisterObjectClassName(typeid(VertexInsertRemoveAction<true>),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.vertex.VertexInsertRemoveAction!true.VertexInsertRemoveAction"));
        return true;
    }();
    (void)registered;
}
[[maybe_unused]] const bool vertexActionClassInfo = [] {
    prepareVertexActionClassInfo();
    return true;
}();
}
// Object.toString, object.d (Digital Mars 2000-2011), BSL-1.0.
// Complete notice: nicxlive/core/licenses/druntime-associative-array.txt.
void VertexAction::prepareSourceClassInfo() {
    prepareVertexActionClassInfo();
}
compat::RetainedString VertexAction::toString() {
    const auto keepAlive = weak_from_this().lock();
    prepareSourceClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
namespace {
SubToolMode savedModeAt(compat::Slice<SubToolMode> modes, std::size_t index, std::size_t line) {
    if (index >= modes.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, modes.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/vertex.d"), line);
    return modes[index];
}
ActionText retainFormatted(std::string value) {
    try {
        return compat::RetainedString(std::move(value));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N, class... T> ActionText text(const char (&key)[N], const T&... values) {
    const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
    const std::array<core::translation::TypedFormatArgument, sizeof...(T)> arguments{
        core::translation::ngFormatArgument(values)...};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}
std::shared_ptr<one::OneTimeDeformBase> tool(std::shared_ptr<nicxlive::core::nodes::Node> target) {
    const auto editor = impl::ngGetEditorFor(target);
    return std::dynamic_pointer_cast<one::OneTimeDeformBase>(compat::ngRequireObject(editor)->getTool());
}
// Phobos stable remove/popFrontExactly lowering, BSL-1.0.
// Copyright Andrei Alexandrescu 2008-; range authors Andrei Alexandrescu,
// David Simcha and Jonathan M Davis. Full license: port/licenses/phobos-source-c-string.txt.
template<class T> compat::Slice<T> removeAt(compat::Slice<T> values, std::size_t index) {
    // Original stable remove pops the prefix, then exactly one element, and
    // publishes the shorter descriptor only after moving the remaining cells.
    // Native assertion selection stays separate from an unverified D profile.
    const auto length = values.size();
#ifndef NDEBUG
    if (index > length)
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
    if (index > length)
        throw compat::ngStaticError<compat::ArraySliceError>(index, length, length,
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
    auto tail = values.slice(index, length);
#ifndef NDEBUG
    if (tail.empty())
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
    if (tail.empty())
        throw compat::ngStaticError<compat::ArraySliceError>(1, 0, 0,
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
    const auto source = tail.slice(1, tail.size());
    const auto result = values.slice(0, length - 1);
    for (std::size_t i = 0; i < source.size(); ++i) tail[i] = source[i];
    return result;
}
}
void VertexAction::initialize(ActionText value, EditorRef receiver, VertexUpdate update) {
    name = std::move(value); editor = std::move(receiver); clear();
    const auto targets = editor ? compat::ngRequireObject(editor)->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    if (!targets.empty()) {
        compat::Slice<SubToolMode> modes;
        modes.resize(targets.size());
        for (std::size_t i = 0; i < targets.size(); ++i)
            modes[i] = compat::ngRequireObject(tool(targets[i]))->mode;
        oldSubToolMode = modes;
    }
    if (update) { update(); updateNewState(); }
}
void VertexAction::updateNewState() {
    const auto keepAlive = shared_from_this();
    const auto targets = editor ? compat::ngRequireObject(editor)->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    if (!targets.empty()) {
        compat::Slice<SubToolMode> modes;
        modes.resize(targets.size());
        for (std::size_t i = 0; i < targets.size(); ++i)
            modes[i] = compat::ngRequireObject(tool(targets[i]))->mode;
        newSubToolMode = modes;
    }
}
void VertexAction::rollback() {
    const auto keepAlive = shared_from_this();
    const auto targets = editor ? compat::ngRequireObject(editor)->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto selected = tool(targets[i]);
        const auto mode = savedModeAt(oldSubToolMode, i, 67);
        if (!selected) compat::ngFailNullAccess(true);
        selected->mode = mode;
    }
}
void VertexAction::redo() {
    const auto keepAlive = shared_from_this();
    const auto targets = editor ? compat::ngRequireObject(editor)->getFilterTargets()
        : compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>>{};
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto selected = tool(targets[i]);
        const auto mode = savedModeAt(newSubToolMode, i, 77);
        if (!selected) compat::ngFailNullAccess(true);
        selected->mode = mode;
    }
}
ActionText VertexAction::describe() {
    const auto keepAlive = shared_from_this();
    return text("Mesh %s change is restored.", name);
}
ActionText VertexAction::describeUndo() {
    const auto keepAlive = shared_from_this();
    return text("Mesh %s was edited.", name);
}
ActionText VertexAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
std::shared_ptr<VertexMoveAction> VertexMoveAction::create(ActionText name, EditorRef editor, VertexUpdate update) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(VertexMoveAction),
        compat::RetainedString::fromStaticLiteral("nijigenerate.actions.vertex.VertexMoveAction"));
    auto result = std::allocate_shared<VertexMoveAction>(
        nicxlive::core::d_runtime::SourceAllocator<VertexMoveAction>{});
    result->initialize(std::move(name), std::move(editor), std::move(update));
    return result;
}
void VertexMoveAction::moveVertex(MeshVertex* vertex, Vec2 position) {
    const auto keepAlive = shared_from_this();
    auto found = translations.find(vertex);
    if (found != translations.end()) found->second.translated = position;
    else translations[vertex] = Translation{compat::ngRequireObject(vertex)->position, position,
        core::math::mesh::ngTryRetainMeshVertex(vertex)};
    compat::ngRequireObject(editor)->moveMeshVertex(vertex, position); dirty = true;
}
void VertexMoveAction::rollback() {
    const auto keepAlive = shared_from_this();
    if (undoable) {
        VertexAction::rollback();
        const auto entries = translations;
        for (const auto& [vertex, translation] : entries) {
            if (!vertex) compat::ngFailNullAccess(true);
            vertex->position = translation.original;
        }
        undoable = false; compat::ngRequireObject(editor)->refreshMesh();
    }
}
void VertexMoveAction::redo() {
    const auto keepAlive = shared_from_this();
    if (!undoable) {
        VertexAction::redo();
        const auto entries = translations;
        for (const auto& [vertex, translation] : entries) {
            if (!vertex) compat::ngFailNullAccess(true);
            vertex->position = translation.translated;
        }
        undoable = true; compat::ngRequireObject(editor)->refreshMesh();
    }
}
ActionText VertexMoveAction::describe() {
    const auto keepAlive = shared_from_this();
    return text("%s: vertex was translated.", name);
}
ActionText VertexMoveAction::describeUndo() {
    const auto keepAlive = shared_from_this();
    return text("%s: vertex was translated.", name);
}
ActionText VertexMoveAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
template<bool reverse> std::shared_ptr<VertexInsertRemoveAction<reverse>> VertexInsertRemoveAction<reverse>::create(
    ActionText name, EditorRef editor, VertexUpdate update) {
    if constexpr (reverse) {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(VertexInsertRemoveAction<reverse>),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.vertex.VertexInsertRemoveAction!true.VertexInsertRemoveAction"));
    } else {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(VertexInsertRemoveAction<reverse>),
            compat::RetainedString::fromStaticLiteral(
                "nijigenerate.actions.vertex.VertexInsertRemoveAction!false.VertexInsertRemoveAction"));
    }
    auto result = std::allocate_shared<VertexInsertRemoveAction>(
        nicxlive::core::d_runtime::SourceAllocator<VertexInsertRemoveAction>{});
    result->initialize(std::move(name), std::move(editor), std::move(update)); return result;
}
template<bool reverse> void VertexInsertRemoveAction<reverse>::insertVertex(std::int32_t index, MeshVertex* vertex) {
    const auto keepAlive = shared_from_this();
    if constexpr (reverse) {
        const auto entries = vertices;
        for (std::size_t i = 0; i < entries.size(); ++i) if (entries[i].vertex == vertex) {
            const auto entry = entries[i];
            compat::ngRequireObject(editor)->insertMeshVertex(entry.index, entry.vertex);
            vertices = removeAt(vertices, i); dirty = !vertices.empty(); break;
        }
    } else {
        vertices.append({index, vertex});
        if (index >= 0) compat::ngRequireObject(editor)->insertMeshVertex(index, vertex);
        else compat::ngRequireObject(editor)->addMeshVertex(vertex);
        dirty = true;
    }
}
template<bool reverse> void VertexInsertRemoveAction<reverse>::removeVertex(MeshVertex* vertex, bool execute) {
    const auto keepAlive = shared_from_this();
    if constexpr (!reverse) {
        const auto entries = vertices;
        for (std::size_t i = 0; i < entries.size(); ++i) if (entries[i].vertex == vertex) {
            vertices = removeAt(vertices, i);
            if (execute) compat::ngRequireObject(editor)->removeMeshVertex(vertex);
            dirty = !vertices.empty(); break;
        }
    } else {
        auto index = compat::ngRequireObject(editor)->indexOfMesh(vertex);
        vertices.append({index, vertex});
        const auto neighbors = compat::ngRequireObject(vertex)->connections;
        for (auto neighbor : neighbors) connections.append(Connection(vertex, neighbor));
        if (execute) compat::ngRequireObject(editor)->removeMeshVertex(vertex);
        dirty = true;
    }
}
template<bool reverse> void VertexInsertRemoveAction<reverse>::removeVertices() {
    const auto keepAlive = shared_from_this();
    if constexpr (reverse) {
        const auto entries = vertices;
        for (auto entry : entries) compat::ngRequireObject(editor)->removeMeshVertex(entry.vertex);
    }
}
template<bool reverse> void VertexInsertRemoveAction<reverse>::synchronizeTarget() {
    if (auto receiver = std::dynamic_pointer_cast<impl::IncMeshEditorOneDeformable>(editor)) {
        if (auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(receiver->getTarget())) {
            const auto points = receiver->vertices;
            compat::Slice<Vec2> positions;
            positions.resize(points.size(), Vec2{std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::quiet_NaN()});
            for (std::size_t i = 0; i < points.size(); ++i)
                positions[i] = compat::ngRequireObject(points[i])->position;
            core::math::mesh::applyMeshToTargetNoRecord(target, positions,
                static_cast<viewport::common::mesh::IncMesh*>(nullptr));
            const auto current = editor;
            if (!current) compat::ngFailNullAccess(true);
            current->vertexMapDirty = false;
        }
    }
}
template<bool reverse> template<bool mode> void VertexInsertRemoveAction<reverse>::action() {
    const auto keepAlive = shared_from_this();
    if constexpr (!mode) {
        if (undoable != reverse) {
            const auto entries = vertices;
            for (auto entry : entries) compat::ngRequireObject(editor)->removeMeshVertex(entry.vertex);
            synchronizeTarget(); undoable = reverse; compat::ngRequireObject(editor)->refreshMesh();
        }
    } else {
        if (undoable == reverse) {
            const auto entries = vertices;
            for (auto entry : entries) {
                if (entry.index >= 0) compat::ngRequireObject(editor)->insertMeshVertex(entry.index, entry.vertex);
                else compat::ngRequireObject(editor)->addMeshVertex(entry.vertex);
            }
            const auto edges = connections;
            for (auto edge : edges) core::math::mesh::connect(edge.v1, edge.v2);
            synchronizeTarget(); undoable = !reverse; compat::ngRequireObject(editor)->refreshMesh();
        }
    }
}
template<bool reverse> void VertexInsertRemoveAction<reverse>::rollback() {
    const auto keepAlive = shared_from_this(); VertexAction::rollback(); action<reverse>();
}
template<bool reverse> void VertexInsertRemoveAction<reverse>::redo() {
    const auto keepAlive = shared_from_this(); VertexAction::redo(); action<!reverse>();
}
template<bool reverse> template<bool mode> ActionText VertexInsertRemoveAction<reverse>::actionName() {
    if constexpr (mode == reverse)
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("removed"));
    else
        return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("inserted"));
}
template<bool reverse> ActionText VertexInsertRemoveAction<reverse>::describe() {
    const auto keepAlive = shared_from_this();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("%s: vertex was %s."));
    const auto currentName = name;
    const auto operation = actionName<true>();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(currentName), core::translation::ngFormatArgument(operation)};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}
template<bool reverse> ActionText VertexInsertRemoveAction<reverse>::describeUndo() {
    const auto keepAlive = shared_from_this();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("%s: vertex was %s."));
    const auto currentName = name;
    const auto operation = actionName<false>();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(currentName), core::translation::ngFormatArgument(operation)};
    return retainFormatted(core::translation::ngFormatTyped(pattern.view(), arguments));
}
template<bool reverse> ActionText VertexInsertRemoveAction<reverse>::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}
std::shared_ptr<VertexReorderAction> VertexReorderAction::create(ActionText name, EditorRef editor,
    VertexUpdate update) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(VertexReorderAction),
        compat::RetainedString::fromStaticLiteral("nijigenerate.actions.vertex.VertexReorderAction"));
    auto result = std::allocate_shared<VertexReorderAction>(
        nicxlive::core::d_runtime::SourceAllocator<VertexReorderAction>{});
    result->initialize(std::move(name), std::move(editor), std::move(update));
    return result;
}
void VertexReorderAction::rollback() {
    const auto keepAlive = shared_from_this();
    if (undoable) { VertexAction::rollback(); undoable = false; compat::ngRequireObject(editor)->refreshMesh(); }
}
void VertexReorderAction::redo() {
    const auto keepAlive = shared_from_this();
    if (!undoable) { VertexAction::redo(); undoable = true; compat::ngRequireObject(editor)->refreshMesh(); }
}
ActionText VertexReorderAction::describe() {
    const auto keepAlive = shared_from_this();
    return text("%s: vertex was translated.", name);
}
ActionText VertexReorderAction::describeUndo() {
    const auto keepAlive = shared_from_this();
    return text("%s: vertex was translated.", name);
}
ActionText VertexReorderAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
template class VertexInsertRemoveAction<false>;
template class VertexInsertRemoveAction<true>;
}
