#pragma once

#include "base.hpp"
#include "impl_predicates.hpp"
#include <nijigenerate/viewport/common/mesheditor/tools/base.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/catalog.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/vertex.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/ext/nodes/path_vertices.hpp>
#include <concepts>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::viewport::common::mesheditor::operations::impl {

namespace detail {
template<class T> void prepareTargetTypeNames() {
    static const bool registered = [] {
        if constexpr (std::same_as<T, nicxlive::core::nodes::Deformable>)
            nicxlive::core::d_runtime::ngRegisterSourceTypeSpellings<T>({
                compat::RetainedString::fromStaticLiteral("(nijilive.core.nodes.deformable.Deformable)"),
                compat::RetainedString::fromStaticLiteral("(Deformable)"),
                compat::RetainedString::fromStaticLiteral("nijilive.core.nodes.deformable.Deformable"),
                compat::RetainedString::fromStaticLiteral("Deformable")});
        else if constexpr (std::same_as<T, nicxlive::core::nodes::Drawable>)
            nicxlive::core::d_runtime::ngRegisterSourceTypeSpellings<T>({
                compat::RetainedString::fromStaticLiteral("(nijilive.core.nodes.drawable.Drawable)"),
                compat::RetainedString::fromStaticLiteral("(Drawable)"),
                compat::RetainedString::fromStaticLiteral("nijilive.core.nodes.drawable.Drawable"),
                compat::RetainedString::fromStaticLiteral("Drawable")});
        else nicxlive::core::d_runtime::ngPrepareSourceTypeSpellings<T>();
        return true;
    }();
    (void)registered;
    // Other genuine Source declarations use the open typed metadata provider.
}
template<class Class, class Argument, std::size_t P, std::size_t S>
void prepareEditorClassRecord(const char (&prefix)[P], const char (&suffix)[S]) {
    static const bool registered = [&prefix, &suffix] {
        const auto argument = nicxlive::core::d_runtime::ngSourceTypeSpelling<Argument>(
            nicxlive::core::d_runtime::SourceTypeSpelling::QualifiedElement);
        const auto name = [&] {
            try {
                std::string value(prefix, P - 1);
                value.append(argument.view());
                value.append(suffix, S - 1);
                return compat::RetainedString(std::move(value));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Class), name);
        return true;
    }();
    (void)registered;
}
struct ToolModeHash {
    std::size_t operator()(tools::enums::VertexToolMode value) const noexcept {
        return static_cast<std::size_t>(static_cast<std::int32_t>(value));
    }
};
base::IncMeshEditorOne::PathRef getPath(const std::shared_ptr<tools::base::Tool>& tool);
void setPath(const std::shared_ptr<tools::base::Tool>& tool, base::IncMeshEditorOne::PathRef path);
}

template<class T> requires std::derived_from<T, nicxlive::core::nodes::Node>
class IncMeshEditorOneImpl : public base::IncMeshEditorOne {
protected:
    void prepareSourceClassInfo() override {
        base::IncMeshEditorOne::prepareSourceClassInfo();
        detail::prepareTargetTypeNames<T>();
        detail::prepareEditorClassRecord<IncMeshEditorOneImpl<T>, T>(
            "nijigenerate.viewport.common.mesheditor.operations.impl.IncMeshEditorOneImpl!(",
            ").IncMeshEditorOneImpl");
    }
    std::shared_ptr<T> target;
    std::uint32_t groupId = 0;
    using ToolRef = std::shared_ptr<tools::base::Tool>;
    using ToolMap = nicxlive::core::common::ResourceKeyMap<VertexToolMode, ToolRef,
        detail::ToolModeHash, std::equal_to<VertexToolMode>>;
    ToolMap tools;
    compat::Slice<NodeRef> filterTargets;

public:
    // Constructor requires the actual complete tool catalog. No abbreviated
    // catalog is supplied here.
    explicit IncMeshEditorOneImpl(bool deformOnly) : base::IncMeshEditorOne(deformOnly) {
        const auto infos = tools::incGetToolInfo();
        for (auto info : infos) {
            auto& destination = tools;
            const auto mode = compat::ngRequireObject(info)->mode();
            const auto tool = compat::ngRequireObject(info)->newTool();
            destination[mode] = tool;
        }
    }
    NodeRef getTarget() override { return target; }
    void forEachEdge(EdgeVisitor) override {}
    std::shared_ptr<actions::mesh::MeshDisconnectAction> newMeshDisconnectAction() override {
        compat::ngAssertUnreachable("newMeshDisconnectAction is only supported for drawable editors",
            "source/nijigenerate/viewport/common/mesheditor/operations/impl.d", 53);
    }
    void setTarget(NodeRef value) override { target = std::dynamic_pointer_cast<T>(value); }
    PathRef getPath() override { return detail::getPath(toolAt(VertexToolMode::PathDeform,64)); }
    void setPath(PathRef path) override { detail::setPath(toolAt(VertexToolMode::PathDeform,70), path); }
    std::int32_t peek(ImGuiIO* io, CameraRef) override {
        const auto keepAlive = shared_from_this();
        if (tools.contains(toolMode)) return compat::ngRequireObject(toolAt(toolMode,76))->peek(io, keepAlive);
        compat::ngAssertUnreachable("Assertion failure", "source/nijigenerate/viewport/common/mesheditor/operations/impl.d", 78);
    }
    std::int32_t unify(compat::Slice<std::int32_t> actions) override {
        const auto keepAlive = shared_from_this();
        if (tools.contains(toolMode)) return compat::ngRequireObject(toolAt(toolMode,83))->unify(actions);
        compat::ngAssertUnreachable("Assertion failure", "source/nijigenerate/viewport/common/mesheditor/operations/impl.d", 85);
    }
    bool update(ImGuiIO* io, CameraRef, std::int32_t actions) override {
        const auto keepAlive = shared_from_this();
        bool changed = false;
        if (tools.contains(toolMode)) compat::ngRequireObject(toolAt(toolMode,92))->update(io, keepAlive, actions, changed);
        else compat::ngAssertUnreachable("Assertion failure", "source/nijigenerate/viewport/common/mesheditor/operations/impl.d", 94);
        updateSelectionPreview(io);
        return updateChanged(changed);
    }
    void setToolMode(VertexToolMode mode) override {
        if (toolMode == mode) return;
        if (tools.contains(mode)) {
            abortToolMode();
            toolMode = mode;
            const auto tool = toolAt(mode,109);
            compat::ngRequireObject(tool)->setToolMode(mode, shared_from_this());
        }
    }
    void finalizeToolMode() override {
        if (tools.contains(toolMode)) {
            const auto tool = toolAt(toolMode,116);
            compat::ngRequireObject(tool)->finalizeToolMode(shared_from_this());
        }
    }
    void abortToolMode() override {
        if (tools.contains(toolMode)) {
            const auto tool = toolAt(toolMode,123);
            compat::ngRequireObject(tool)->abortToolMode(shared_from_this());
        }
    }
    void releasePendingActions() override {
        const auto keepAlive = shared_from_this();
        const auto retainedTools = tools;
        for (auto [mode, tool] : retainedTools) {
            if (tool) tool->releasePendingActions();
        }
        forceResetAction();
    }
    std::uint64_t selectOne(std::uint64_t vertexIndex) override {
        const auto keepAlive = shared_from_this();
        const auto vertex = getVerticesByIndex(Indices{vertexIndex});
        if (groupId > 0 && [&] {
            if (vertex.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, vertex.size(),
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/common/mesheditor/operations/impl.d"), 131);
            return vertex[0] && vertex[0]->groupId != groupId;
        }()) {
            selected = {};
            return UINT64_MAX;
        }
        return base::IncMeshEditorOne::selectOne(vertexIndex);
    }
    ToolRef getTool() override { return toolAt(toolMode,140); }
    std::uint32_t getGroupId() override { return groupId; }
    void setGroupId(std::uint32_t value) override { groupId = value; }
    compat::Slice<NodeRef> getFilterTargets() override { return filterTargets; }
    void addFilterTarget(NodeRef parent) override {
        const auto keepAlive = shared_from_this();
        const auto input = filterTargets;
        for (auto existing : input)
            if (nicxlive::core::ngResourceObjectEquals(existing, parent)) return;
        filterTargets.append(std::move(parent));
    }
    void removeFilterTarget(NodeRef parent) override {
        const auto keepAlive = shared_from_this();
        const auto input = filterTargets;
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (!nicxlive::core::ngResourceObjectEquals(input[i], parent)) continue;
            auto current = filterTargets;
            if (i >= current.size()) {
#ifndef NDEBUG
                throw compat::ngStaticError<compat::AssertError>(
                    "range is smaller than amount of items to pop", "std/range/primitives.d", 2137);
#else
                const auto lower = i > current.size() ? i : std::size_t{1};
                const auto extent = i > current.size() ? current.size() : std::size_t{0};
                throw compat::ngStaticError<compat::ArraySliceError>(lower, extent, extent,
                    compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
#endif
            }
            for (std::size_t j = i + 1; j < current.size(); ++j) current[j - 1] = current[j];
            filterTargets = current.slice(0, current.size() - 1);
            break;
        }
    }

private:
    ToolRef toolAt(VertexToolMode mode,std::size_t sourceLine) {
        const auto found=tools.find(mode);
        if(found==tools.end())throw compat::ngStaticError<compat::RangeError>(compat::RetainedString::fromStaticLiteral(
            "source/nijigenerate/viewport/common/mesheditor/operations/impl.d"),sourceLine);
        return found->second;
    }
};

class IncMeshEditorOneDeformable : public IncMeshEditorOneImpl<nicxlive::core::nodes::Deformable> {
protected:
    void prepareSourceClassInfo() override {
        IncMeshEditorOneImpl::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(IncMeshEditorOneDeformable),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.operations.impl.IncMeshEditorOneDeformable"));
            return true;
        }();
        (void)registered;
    }
    bool changed = false;
public:
    VertexPointers vertices;
    explicit IncMeshEditorOneDeformable(bool deformOnly) : IncMeshEditorOneImpl(deformOnly) {}
    virtual std::shared_ptr<nicxlive::core::nodes::Deformable> deformable() {
        return std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(getTarget());
    }
    VertexPointers getVerticesByIndex(Indices indices, bool removeNull = false) override {
        VertexPointers result;
        for (auto index : indices) {
            if (index < vertices.size()) result.append(vertices[static_cast<std::size_t>(index)]);
            else if (!removeNull) result.append(nullptr);
        }
        return result;
    }
};

class IncMeshEditorOneDrawable : public IncMeshEditorOneImpl<nicxlive::core::nodes::Drawable> {
protected:
    void prepareSourceClassInfo() override {
        IncMeshEditorOneImpl::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(IncMeshEditorOneDrawable),
                compat::RetainedString::fromStaticLiteral(
                    "nijigenerate.viewport.common.mesheditor.operations.impl.IncMeshEditorOneDrawable"));
            return true;
        }();
        (void)registered;
    }
public:
    MeshRef mesh;
    explicit IncMeshEditorOneDrawable(bool deformOnly) : IncMeshEditorOneImpl(deformOnly) {}
    virtual MeshRef& getMesh() { return mesh; }
    virtual void setMesh(MeshRef value) { mesh = std::move(value); }
    virtual VertexPointers vertices();
    void forEachEdge(EdgeVisitor visitor) override;
    std::shared_ptr<actions::mesh::MeshDisconnectAction> newMeshDisconnectAction() override;
};

template<class T> nicxlive::core::math::Vec2Array getVertices(std::shared_ptr<T> node) {
    if (auto deform = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node)) return deform->getVertices();
    const auto value = compat::ngRequireObject(node)->transform().translation;
    return nicxlive::core::math::Vec2Array{nicxlive::core::math::Vec2{value.x, value.y}};
}

template<class T, class Values = nicxlive::core::math::Vec2Array>
requires std::same_as<Values, nicxlive::core::math::Vec2Array>
void setVertices(std::shared_ptr<T> node, Values value) {
    if (auto deform = std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node))
        ext::nodes::ngAssignDeformableVertices(*deform, value);
    else {
        // Keep the original generic scalar-conversion requirement; do not
        // silently replace its value[0], value[1] expression with value[0].xy.
        const auto translate = []<class Input>(Input input) {
            return nicxlive::core::math::Vec3{input[0], input[1], 0};
        };
        compat::ngRequireObject(node)->transform().translation = translate(value);
    }
}

inline nicxlive::core::math::Vec2Array toVertices(core::math::mesh::MeshVertexArray array) {
    nicxlive::core::math::Vec2Array result(array.size());
    for (std::size_t i = 0; i < array.size(); ++i)
        result.set(i, compat::ngRequireObject(array[i])->position);
    return result;
}
inline core::math::mesh::MeshVertexArray toMVertices(compat::Slice<nicxlive::core::math::Vec2> array) {
    core::math::mesh::MeshVertexArray result;
    for (auto value : array) result.append(core::math::mesh::ngCreateMeshVertex(value));
    return result;
}
inline core::math::mesh::MeshVertexArray toMVertices(nicxlive::core::math::Vec2Array array) {
    core::math::mesh::MeshVertexArray result;
    for (std::size_t i = 0; i < array.size(); ++i) result.append(core::math::mesh::ngCreateMeshVertexSource([&] {
        return array.at(i);
    }));
    return result;
}
inline void resize(core::math::mesh::MeshVertexArray& array, std::uint64_t size) {
    if (size <= array.size()) array.resize(size);
    else {
        auto missing=size-array.size();
        while(missing!=0) {
            array.append(core::math::mesh::ngCreateMeshVertex());
            --missing;
        }
    }
}
template<class Vertices> void drawPointSubset(Vertices subset, nicxlive::core::math::Vec4 color,
    nicxlive::core::math::Mat4 trans = nicxlive::core::math::Mat4::identity(), float size = 6) {
    if (subset.empty()) return;
    nicxlive::core::math::Vec3Array points;
    for (auto vertex : subset) if (toBool(vertex)) {
        // Source .position also resolves the imported core.math.vertex UFCS
        // overload for Vec2/vecv; use that one canonical provider.
        const auto position = core::math::vertex::position(vertex);
        points.append({position.x, position.y, 0});
    }
    core::dbg::inDbgSetBuffer(points);
    core::dbg::inDbgPointsSize(size);
    core::dbg::inDbgDrawPoints(color, trans);
}
std::size_t ngRefineWeldingSeams(std::shared_ptr<nicxlive::core::nodes::Drawable> first,
    std::shared_ptr<nicxlive::core::nodes::Drawable> second);
void incUpdateWeldedPoints(std::shared_ptr<nicxlive::core::nodes::Drawable> drawable,
    compat::ConstSlice<std::ptrdiff_t> remap);
std::shared_ptr<base::IncMeshEditorOne> ngGetEditorFor(std::shared_ptr<nicxlive::core::nodes::Node> target);

} // namespace nijigenerate::viewport::common::mesheditor::operations::impl
