// Derived from source/nijigenerate/viewport/common/mesheditor/operations/node.d.
// Copyright (c) 2022 Inochi2D Project, 2024 nijigenerate Project. BSD-2-Clause.
#pragma once
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/actions/binding_value_action.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <common/nullable_string.hpp>
#include <compat/source_allocator.hpp>
#include <bit>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <tuple>

namespace nijigenerate::viewport::common::mesheditor::operations::node {
using Node = nicxlive::core::nodes::Node;
using Deformable = nicxlive::core::nodes::Deformable;
using ValueBinding = nicxlive::core::param::ValueParameterBinding;
using Binding = nicxlive::core::param::ParameterBinding;
using Group = actions::GroupAction;
using Change = actions::binding::ParameterBindingValueChangeAction<float, ValueBinding>;
using BindingName = nicxlive::core::common::NullableString;

namespace detail {
template<class Array> decltype(auto) nodeActionAt(Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size())
        throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/common/mesheditor/operations/node.d"), line);
    return values[index];
}
} // namespace detail

template<class T, project::EditMode mode = project::EditMode::ModelEdit>
requires (std::derived_from<T, Node> && !std::derived_from<T, Deformable>)
class NodeEditorImpl : public impl::IncMeshEditorOneImpl<Node> {
    using Base = impl::IncMeshEditorOneImpl<Node>;
    using Vec2Array = nicxlive::core::math::Vec2Array;
    using MeshData = nicxlive::core::nodes::MeshData;
    using EditorAction = actions::mesheditor::MeshEditorAction<Group>;
    Vec2 translation{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    std::shared_ptr<EditorAction> editorAction;

    static float distance(Vec2 a, Vec2 b) {
        // inmath distance returns float after Source real accumulation and sqrt.
        const float x = a.x - b.x, y = a.y - b.y;
#if defined(_MSC_VER)
        // LDC's Windows MSVC target maps D real to binary64.
        using SourceReal = double;
#else
        using SourceReal = long double;
#endif
        SourceReal sum = 0;
        sum += x * x;
        sum += y * y;
        return static_cast<float>(std::sqrt(sum));
    }
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        impl::detail::prepareTargetTypeNames<T>();
        static const bool registered = [] {
            const auto argument = nicxlive::core::d_runtime::ngSourceTypeSpelling<T>(
                nicxlive::core::d_runtime::SourceTypeSpelling::QualifiedElement);
            const auto name = [&] {
                try {
                    const auto modeName = []() -> std::string {
                        switch (mode) {
                        case project::EditMode::ModelEdit: return "EditMode.ModelEdit";
                        case project::EditMode::VertexEdit: return "EditMode.VertexEdit";
                        case project::EditMode::AnimEdit: return "EditMode.AnimEdit";
                        case project::EditMode::ModelTest: return "EditMode.ModelTest";
                        case project::EditMode::DepthEdit: return "EditMode.DepthEdit";
                        case project::EditMode::ALL: return "EditMode.ALL";
                        default: return "cast(EditMode)" + std::to_string(static_cast<std::int32_t>(mode));
                        }
                    }();
                    std::string value = "nijigenerate.viewport.common.mesheditor.operations.node.IncMeshEditorOneFor!(";
                    value.append(argument.view());
                    value += ", ";
                    value += modeName;
                    value += ").IncMeshEditorOneFor";
                    return compat::RetainedString(std::move(value));
                } catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(NodeEditorImpl<T, mode>),
                name);
            return true;
        }();
        (void)registered;
    }
    void substituteMeshVertices(MeshVertex* vertex) override {
        const auto keepAlive = shared_from_this();
        translation = compat::ngRequireObject(vertex)->position;
        compat::ngRequireObject(target)->setValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.x"), translation.x);
        compat::ngRequireObject(target)->setValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.y"), translation.y);
    }
public:
    float selectRadius = 16;
    explicit NodeEditorImpl(bool value) : Base(value) { deformOnly = value; }
    void setTarget(NodeRef value) override {
        const auto keepAlive = shared_from_this();
        Base::setTarget(value);
        transform = value ? value->transform().matrix() : Mat4::identity();
        refreshMesh();
    }
    void resetMesh() override {
        const auto keepAlive = shared_from_this();
        const float x = compat::ngRequireObject(target)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.x"));
        const float y = compat::ngRequireObject(target)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.y"));
        translation = {x, y};
    }
    void refreshMesh() override {
        const auto keepAlive = shared_from_this();
        const float x = compat::ngRequireObject(target)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.x"));
        const float y = compat::ngRequireObject(target)->getValueSource(
            compat::RetainedString::fromStaticLiteral("transform.t.y"));
        translation = {x, y};
    }
    void importMesh(MeshData&) override {}
    void mergeMesh(MeshData&, Mat4) override {}
    void applyOffsets(Vec2Array offsets) override {
#ifndef NDEBUG
        if (!deformOnly) throw compat::ngStaticError<compat::AssertError>("", "source/nijigenerate/viewport/common/mesheditor/operations/node.d", 77);
#endif
        translation = static_cast<Vec2>(offsets[0]);
    }
    Vec2Array getOffsets() override {
#ifndef NDEBUG
        if (!deformOnly) throw compat::ngStaticError<compat::AssertError>("", "source/nijigenerate/viewport/common/mesheditor/operations/node.d", 84);
#endif
        return Vec2Array{translation};
    }
    void applyToTarget() override {
        const auto keepAlive = shared_from_this();
        auto action = std::allocate_shared<Group>(nicxlive::core::d_runtime::SourceAllocator<Group>{});
        if (vertexMapDirty) {
            const auto alterDeform = [](std::shared_ptr<Binding> binding, float value) {
                const auto deform = std::dynamic_pointer_cast<ValueBinding>(binding);
                if (!deform) return;
                const auto nx = static_cast<std::uint32_t>(deform->values.size());
                for (std::uint32_t x = 0; x < nx; ++x) {
                    const auto ny = static_cast<std::uint32_t>(detail::nodeActionAt(deform->values, x, 95).size());
                    for (std::uint32_t y = 0; y < ny; ++y) {
                        const auto previous = detail::nodeActionAt(detail::nodeActionAt(deform->values, x, 96), y, 96);
                        (void)previous;
                        if (deform->isSet({x, y}))
                            detail::nodeActionAt(detail::nodeActionAt(deform->values, x, 98), y, 98) = value;
                    }
                }
                deform->reInterpolate();
            };
            const auto visit = [&](const std::shared_ptr<nicxlive::core::param::Parameter>& parameter) {
                auto binding = compat::ngRequireObject(parameter)->getBinding(target,
                    BindingName::fromStaticLiteral("transform.t.x"));
                if (binding) action->addAction(actions::parameter::ngCreateParameterBindingsAction<
                    actions::parameter::ParameterChangeBindingsAction>(
                    compat::RetainedString::fromStaticLiteral("Deformation of translation"), parameter,
                    actions::parameter::Bindings{}));
                alterDeform(binding, translation.x);
                binding = compat::ngRequireObject(parameter)->getBinding(target,
                    BindingName::fromStaticLiteral("transform.t.y"));
                if (binding) action->addAction(actions::parameter::ngCreateParameterBindingsAction<
                    actions::parameter::ParameterChangeBindingsAction>(
                    compat::RetainedString::fromStaticLiteral("Deformation of translation"), parameter,
                    actions::parameter::Bindings{}));
                alterDeform(binding, translation.y);
            };
            const auto parameters = compat::ngRequireObject(project::incActivePuppet())->parameters;
            for (auto parameter : parameters) {
                if (auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(parameter)) {
                    const auto children = group->children;
                    for (const auto& child : children) visit(child);
                } else visit(parameter);
            }
            vertexMapDirty = false;
        }
        core::actionstack::incActionPush(action);
    }
    void applyPreview() override {}
    template<bool reset = false> ActionRef getDeformActionImpl() {
        const auto keepAlive = shared_from_this();
        auto parameter = project::incArmedParameter();
        const auto index = compat::ngRequireObject(parameter)->findClosestKeypoint();
        const auto registerBinding = [&](BindingName name, std::shared_ptr<Group> group) {
            auto binding = std::dynamic_pointer_cast<ValueBinding>(
                compat::ngRequireObject(parameter)->getBinding(target, name));
            if (!binding) {
                binding = std::dynamic_pointer_cast<ValueBinding>(parameter->createBinding(target, name));
                parameter->addBinding(binding);
                compat::ngRequireObject(group)->addAction(actions::binding::ngCreateBindingAddRemoveAction<
                    actions::binding::ParameterBindingAddAction>(parameter, binding));
            }
            const auto transAction = actions::binding::ngCreateBindingValueActionDeferred<Change>([&] {
                const auto label = compat::RetainedString(compat::ngRequireObject(binding)->getNameDescriptor());
                return std::tuple{label, binding, static_cast<int>(index.x), static_cast<int>(index.y)};
            });
            compat::ngRequireObject(group)->addAction(transAction);
        };
        if constexpr (reset) pushDeformAction();
        if (!editorAction) {
            auto group = std::allocate_shared<Group>(nicxlive::core::d_runtime::SourceAllocator<Group>{});
            registerBinding(BindingName::fromStaticLiteral("transform.t.x"), group);
            registerBinding(BindingName::fromStaticLiteral("transform.t.y"), group);
            registerBinding(BindingName::fromStaticLiteral("transform.r.z"), group);
            const auto registry = tools;
            if (registry.isNull())
                throw compat::ngStaticError<compat::RangeError>(compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/common/mesheditor/operations/node.d"), 158);
            const auto found = registry.find(toolMode);
            if (found == registry.end())
                throw compat::ngStaticError<compat::RangeError>(compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/common/mesheditor/operations/node.d"), 158);
            const auto selected = found->second;
            editorAction = compat::ngRequireObject(selected)->editorAction(target, group);
        } else if constexpr (reset) {
            compat::ngRequireObject(editorAction)->clear();
            compat::ngRequireObject(compat::ngRequireObject(editorAction)->action)->actions.resize(0);
            registerBinding(BindingName::fromStaticLiteral("transform.t.x"),
                compat::ngRequireObject(editorAction)->action);
            registerBinding(BindingName::fromStaticLiteral("transform.t.y"),
                compat::ngRequireObject(editorAction)->action);
            registerBinding(BindingName::fromStaticLiteral("transform.r.z"),
                compat::ngRequireObject(editorAction)->action);
        }
        return editorAction;
    }
    ActionRef getDeformAction() override { return getDeformActionImpl<false>(); }
    ActionRef getCleanDeformAction() override { return getDeformActionImpl<true>(); }
    void forceResetAction() override { editorAction.reset(); }
    void pushDeformAction() override {
        const auto keepAlive = shared_from_this();
        if (!editorAction) return;
        bool dirty = false;
        const auto children = compat::ngRequireObject(editorAction->action)->actions;
        for (auto action : children) if (auto change = std::dynamic_pointer_cast<Change>(action)) {
            dirty = dirty || change->dirty();
            if (dirty) break;
        }
        if (dirty) {
            compat::ngRequireObject(editorAction)->updateNewState();
            const auto updatedChildren = compat::ngRequireObject(
                compat::ngRequireObject(editorAction)->action)->actions;
            for (auto action : updatedChildren)
                if (auto lazy = std::dynamic_pointer_cast<actions::LazyBoundAction>(action)) lazy->updateNewState();
            core::actionstack::incActionPush(editorAction);
            editorAction.reset();
        }
    }
    std::uint64_t getVertexFromPoint(Vec2 position) override {
        if (std::abs(distance(translation, position)) < selectRadius / viewport::base::viewportZoom) return 0;
        return UINT64_MAX;
    }
    compat::Slice<float> getVerticesInBrush(Vec2 position, BrushRef brush) override {
        const auto keepAlive = shared_from_this();
        compat::Slice<float> result;
        result.resize(1);
        result[0] = compat::ngRequireObject(brush)->weightAt(position, translation);
        return result;
    }
    void removeVertexAt(Vec2) override {}
    bool removeVertex(ImGuiIO*, bool) override { return false; }
    bool addVertex(ImGuiIO*) override { return false; }
    bool updateChanged(bool changed) override { return changed; }
    void addMeshVertex(MeshVertex*) override {}
    void insertMeshVertex(std::int32_t, MeshVertex*) override {}
    void removeMeshVertex(MeshVertex*) override {}
    void moveMeshVertex(MeshVertex*, Vec2) override {}
    bool isPointOver(Vec2 position) override {
        return std::abs(distance(translation, position)) < selectRadius / viewport::base::viewportZoom;
    }
    Indices getInRect(Vec2 min, Vec2 max, std::uint32_t) override {
        if (min.x > max.x) std::swap(min.x, max.x);
        if (min.y > max.y) std::swap(min.y, max.y);
        if (min.x > translation.x || min.y > translation.y || max.x < translation.x || max.y < translation.y)
            return {};
        return {0};
    }
    Indices filterVertices(VertexFilter filter) override {
        const auto keepAlive = shared_from_this();
        const auto vertex = core::math::mesh::ngCreateMeshVertex(translation);
        if (!filter || !*filter) compat::ngFailNullAccess(false);
        if ((*filter)(vertex.get())) return {0};
        return {};
    }
    VertexPointers getVerticesByIndex(Indices indices, bool removeNull = false) override {
        const auto keepAlive = shared_from_this();
        VertexPointers result;
        for (auto index : indices) {
            if (index == 0) result.append(core::math::mesh::ngCreateMeshVertex(translation));
            else if (!removeNull) result.append(nullptr);
        }
        return result;
    }
    void createPathTarget() override {
        const auto keepAlive = shared_from_this();
        compat::ngRequireObject(getPath())->createTarget(target, Mat4::identity());
    }
    Mat4 updatePathTarget() override {
        const auto keepAlive = shared_from_this();
        return compat::ngRequireObject(getPath())->updateTarget(target);
    }
    void resetPathTarget() override {
        const auto keepAlive = shared_from_this();
        compat::ngRequireObject(getPath())->resetTarget(target);
    }
    void remapPathTarget(PathRef& path, Mat4 value) override {
        const auto keepAlive = shared_from_this();
        compat::ngRequireObject(path)->remapTarget(target, value);
    }
    bool hasAction() override { return static_cast<bool>(editorAction); }
    void updateAddVertexAction(MeshVertex*) override {}
    void clearAction() override {
        const auto keepAlive = shared_from_this();
        if (editorAction) {
            const auto children = compat::ngRequireObject(
                compat::ngRequireObject(editorAction)->action)->actions;
            for (auto action : children)
                if (auto change = std::dynamic_pointer_cast<Change>(action)) change->clear();
        }
    }
    void markActionDirty() override {
        const auto keepAlive = shared_from_this();
        const auto children = compat::ngRequireObject(
            compat::ngRequireObject(editorAction)->action)->actions;
        for (auto action : children)
            if (auto change = std::dynamic_pointer_cast<Change>(action)) change->markAsDirty();
    }
    void draw(CameraRef camera) override {
        const auto keepAlive = shared_from_this();
        if (isSelecting) {
            const auto lines = viewport::common::ngCreateRectBuffer(selectOrigin, mousePos);
            core::dbg::inDbgSetBuffer(lines);
        }
        if (tools.contains(toolMode)) {
            const auto tool = tools.at(toolMode);
            compat::ngRequireObject(tool)->draw(camera, shared_from_this());
        }
    }
    void adjustPathTransform() override {
        const auto keepAlive = shared_from_this();
        const Mat4 trans = Mat4::multiply(core::math::source_matrix::inverse(
            target ? compat::ngRequireObject(target)->transform().matrix() : transform), transform);
        const auto adjust = [&](PathRef& path) -> PathRef& {
            for (std::uint32_t bits = 0;
                static_cast<std::size_t>(std::bit_cast<std::int32_t>(bits)) <
                    compat::ngRequireObject(path)->points.size(); ++bits) {
                const auto i = static_cast<std::size_t>(std::bit_cast<std::int32_t>(bits));
                const auto old = detail::nodeActionAt(compat::ngRequireObject(path)->points, i, 334).position;
                const auto value = core::math::ngTransformHomogeneousPoint(trans, {old.x, old.y, 0});
                detail::nodeActionAt(compat::ngRequireObject(path)->points, i, 334).position = {value.x, value.y};
            }
            compat::ngRequireObject(path)->update();
            remapPathTarget(path, Mat4::identity());
            return path;
        };
        if (getPath()) {
            if (compat::ngRequireObject(getPath())->target) {
                const auto receiver = getPath();
                // Keep the actual parent of the ref argument alive through
                // adjustment and final assignment, including reentrant remaps.
                const auto argumentReceiver = compat::ngRequireObject(getPath());
                auto& adjusted = adjust(argumentReceiver->target);
                if (!receiver) compat::ngFailNullAccess(true);
                receiver->target = adjusted;
            }
            auto path = getPath();
            setPath(adjust(path));
        }
        const auto position = core::math::ngTransformHomogeneousPoint(trans, {lastMousePos.x, lastMousePos.y, 0});
        lastMousePos = {position.x, position.y};
        transform = compat::ngRequireObject(target)->transform().matrix();
        forceResetAction();
    }
};
using NodeEditor = NodeEditorImpl<Node>;
template<class T, project::EditMode mode = project::EditMode::ModelEdit>
requires (std::derived_from<T, Node> && !std::derived_from<T, Deformable>)
using IncMeshEditorOneFor = NodeEditorImpl<T, mode>;
} // namespace nijigenerate::viewport::common::mesheditor::operations::node


