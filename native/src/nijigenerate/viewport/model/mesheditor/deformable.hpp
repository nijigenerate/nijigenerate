#pragma once
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nodes/drawable.hpp>
#include <concepts>
#include <type_traits>
#include <limits>
#include <vector>
#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/core/math/source_matrix_inverse.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/package.hpp>
#include <nijigenerate/viewport/common/spline.hpp>
#include <nijigenerate/viewport/common/mesheditor/tools/base.hpp>
#include <param/binding_impl.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/curve.hpp>
#include <nijigenerate/compat/phobos_float_sort.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <algorithm>
#include <cmath>
#include <new>
#include <source_location>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace nijigenerate::viewport::model::mesheditor::deformable {
using namespace core::dbg;
namespace detail {
template<class Operation> decltype(auto) allocateBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
template<class T, project::EditMode mode>
requires (mode == project::EditMode::ModelEdit &&
    std::same_as<T, std::remove_cv_t<T>> &&
    std::derived_from<T, nicxlive::core::nodes::Deformable> &&
    !std::derived_from<T, nicxlive::core::nodes::Drawable>)
class IncMeshEditorOneFor : public common::mesheditor::operations::impl::IncMeshEditorOneDeformable {
    using Base = common::mesheditor::operations::impl::IncMeshEditorOneDeformable;
    using Deformable = nicxlive::core::nodes::Deformable;
    using MeshData = nicxlive::core::nodes::MeshData;
    using Vec3Array = nicxlive::core::math::Vec3Array;
    using Vec4 = nicxlive::core::math::Vec4;
    using DeformationAction = actions::mesheditor::DeformationAction;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        common::mesheditor::operations::impl::detail::prepareEditorClassRecord<IncMeshEditorOneFor<T, mode>, T>(
            "nijigenerate.viewport.model.mesheditor.deformable.IncMeshEditorOneFor!(",
            ", EditMode.ModelEdit).IncMeshEditorOneFor");
    }
    nicxlive::core::math::Vec2Array deformation;
    nicxlive::core::math::Vec2Array inputVertices;
    void substituteMeshVertices(MeshVertex*) override {}
    std::shared_ptr<actions::mesheditor::MeshEditorAction<actions::mesheditor::DeformationAction>> editorAction;
    virtual void updateTarget() {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        transform = compat::ngRequireObject(deformable())->getDynamicMatrix();
        inputVertices.resize(compat::ngRequireObject(deformable())->getVertices().size());
        const auto deformCount = compat::ngRequireObject(deformable())->deformation.size();
        const auto vertexOwner = compat::ngRequireObject(deformable());
        auto& baseVertices = vertexOwner->getVertices();
        baseVertices.forEachValue([&](std::size_t i, Vec2 point) {
            const auto offset = i < deformCount ?
                compat::ngRequireObject(deformable())->deformation[i].toVector() : Vec2{0, 0};
            point.x += offset.x;
            point.y += offset.y;
            inputVertices.set(i, point);
            return 0;
        });
    }
    virtual void importDeformation() {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        const auto drawable = std::dynamic_pointer_cast<Deformable>(target);
        if (!drawable) return;
        const auto parameter = project::incArmedParameter();
        const auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(
            parameter ? parameter->getBinding(drawable,
                nicxlive::core::common::NullableString::fromStaticLiteral("deform")) : nullptr);
        if (!binding) deformation = drawable->deformation.dup();
        else {
            const auto index = compat::ngRequireObject(parameter)->findClosestKeypoint();
            auto deform = binding->getValue(index);
            if (drawable->deformation.size() == deform.vertexOffsets.size()) {
                deformation.resize(drawable->deformation.size());
                drawable->deformation.forEachValue([&](std::size_t i, Vec2 value) {
                    const auto offset = deform.vertexOffsets[i].toVector();
                    deformation.set(i, {value.x - offset.x, value.y - offset.y});
                    return 0;
                });
            }
        }
    }
public:
    using Vec2Array = nicxlive::core::math::Vec2Array;
    IncMeshEditorOneFor() : Base(true) {}
    void setTarget(NodeRef value) override {
        const auto keepAlive = shared_from_this();
        if (!std::dynamic_pointer_cast<Deformable>(value)) return;
        importDeformation();
        Base::setTarget(value);
        inputVertices = compat::ngRequireObject(deformable())->getVertices().dup();
        common::mesheditor::operations::impl::resize(vertices, inputVertices.size());
        const auto count = inputVertices.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto vertex = compat::ngRequireObject(vertices[i]);
            vertex->position = inputVertices[i].toVector();
        }
        updateTarget();
        refreshMesh();
    }
    void resetMesh() override {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        vertices = common::mesheditor::operations::impl::toMVertices(compat::ngRequireObject(target)->getVertices());
    }
    void refreshMesh() override { updateMirrorSelected(); }
    void importMesh(MeshData&) override {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        vertices = common::mesheditor::operations::impl::toMVertices(compat::ngRequireObject(target)->getVertices());
    }
    void mergeMesh(MeshData&, Mat4) override {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        vertices.append(common::mesheditor::operations::impl::toMVertices(compat::ngRequireObject(target)->getVertices()));
    }
    void applyOffsets(Vec2Array offsets) override {
        const auto count = std::min(vertices.size(), offsets.size());
        for (std::size_t i = 0; i < count; ++i) {
            const auto delta = offsets[i].toVector();
            auto* position = &compat::ngRequireObject(vertices[i])->position;
            position->x += delta.x;
            position->y += delta.y;
        }
    }
    Vec2Array getOffsets() override {
        [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        Vec2Array offsets(vertices.size());
        const auto currentVertices = vertices;
        for (std::size_t i = 0; i < currentVertices.size(); ++i) {
            const auto vertex = currentVertices[i];
            if (i < compat::ngRequireObject(target)->getVertices().size()) {
                auto value = compat::ngRequireObject(vertex)->position;
                const auto base = compat::ngRequireObject(target)->getVertices()[i].toVector();
                value.x -= base.x;
                value.y -= base.y;
                offsets.set(i, value);
            } else offsets.set(i, compat::ngRequireObject(vertex)->position);
        }
        return offsets;
    }
    void applyToTarget() override {}
    void applyPreview() override {}
    void pushDeformAction() override {
        const auto keepAlive = shared_from_this();
        if (editorAction && compat::ngRequireObject(editorAction->action)->dirty) {
            compat::ngRequireObject(editorAction)->updateNewState();
            core::actionstack::incActionPush(editorAction);
            editorAction.reset();
        }
    }
    std::uint64_t getVertexFromPoint(Vec2 p) override {
        return core::math::vertex::getVertexFromPoint(inputVertices, p, viewport::base::viewportZoom);
    }
    compat::Slice<float> getVerticesInBrush(Vec2 p, BrushRef brush) override {
        return core::math::vertex::getVerticesInBrush(inputVertices, p, brush);
    }
    void removeVertexAt(Vec2) override {}
    bool removeVertex(ImGuiIO*, bool) override { return false; }
    bool addVertex(ImGuiIO*) override { return false; }
    bool updateChanged(bool changed) override { return changed; }
    void addMeshVertex(MeshVertex*) override {}
    void insertMeshVertex(std::int32_t, MeshVertex*) override {}
    void removeMeshVertex(MeshVertex*) override {}
    void moveMeshVertex(MeshVertex* vertex, Vec2 position) override {
        compat::ngRequireObject(vertex)->position = position;
    }
    bool isPointOver(Vec2 p) override {
        return core::math::vertex::isPointOverVertex(inputVertices, p, viewport::base::viewportZoom);
    }
    Indices getInRect(Vec2 min, Vec2 max, std::uint32_t) override {
        if (min.x > max.x) std::swap(min.x, max.x);
        if (min.y > max.y) std::swap(min.y, max.y);
        Indices matching;
        inputVertices.forEachValue([&](std::size_t i, Vec2 vertex) {
            if (!(min.x > vertex.x || min.y > vertex.y || max.x < vertex.x || max.y < vertex.y))
                matching.append(i);
            return 0;
        });
        return matching;
    }
    Indices filterVertices(VertexFilter filter) override {
        const auto keepAlive = shared_from_this();
        Indices matching;
        MeshVertex vertex;
        inputVertices.forEachValue([&](std::size_t i, Vec2 value) {
            vertex.position = value;
            if ((*compat::ngRequireObject(filter))(&vertex)) matching.append(i);
            return 0;
        });
        return matching;
    }
    void createPathTarget() override {}
    Mat4 updatePathTarget() override { return Mat4::identity(); }
    void resetPathTarget() override {}
    void remapPathTarget(PathRef&, Mat4) override {}
    bool hasAction() override { return editorAction != nullptr; }
    void updateAddVertexAction(MeshVertex* vertex) override {
        if (editorAction) compat::ngRequireObject(editorAction->action)->addVertex(vertex);
    }
    void clearAction() override { if (editorAction) compat::ngRequireObject(editorAction->action)->clear(); }
    void markActionDirty() override { if (editorAction) compat::ngRequireObject(editorAction->action)->markAsDirty(); }
    ActionRef getDeformAction() override { return getDeformActionImpl<false>(); }
    ActionRef getCleanDeformAction() override { return getDeformActionImpl<true>(); }
    template<bool reset = false> ActionRef getDeformActionImpl() {
        const auto keepAlive = shared_from_this();
        if (reset) pushDeformAction();
        if (!editorAction || !compat::ngRequireObject(editorAction->action)->isApplyable()) {
            const auto action = DeformationAction::createSource([&] {
                const actions::ActionText name(compat::RetainedString(compat::ngRequireObject(target)->getName()));
                const auto actionTarget = target;
                return std::tuple{name, actionTarget, compat::SharedDelegate<void()>{}};
            });
            const auto currentTools = tools;
            const auto currentMode = toolMode;
            const auto found = currentTools.find(currentMode);
            if (found == currentTools.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/model/mesheditor/deformable.d"), 268);
            editorAction = compat::ngRequireObject(found->second)->editorAction(target, action);
        } else if (reset) compat::ngRequireObject(editorAction)->clear();
        return editorAction;
    }
    void forceResetAction() override { editorAction.reset(); }
    void draw(CameraRef camera) override {
        const auto keepAlive = shared_from_this();
        [[maybe_unused]] const auto sourceDeformable = std::dynamic_pointer_cast<Deformable>(target);
        updateTarget();
        const auto trans = transform;
        using common::mesheditor::operations::impl::drawPointSubset;
        Vec3Array points;
        points.append({0, 0, 0});
        inDbgSetBuffer(points);
        inDbgPointsSize(10); inDbgDrawPoints({0, 0, 0, 1}, trans);
        inDbgPointsSize(6); inDbgDrawPoints({0.5f, 1, 0.5f, 1}, trans);
        points.resize(inputVertices.size());
        for (std::size_t i = 0; i < inputVertices.size(); ++i) {
            const auto p = inputVertices[i].toVector(); points.set(i, {p.x, p.y, 0});
        }
        if (!points.empty()) {
            if (auto path = std::dynamic_pointer_cast<nicxlive::core::nodes::PathDeformer>(target)) {
                auto curve = path->createCurve(inputVertices.dup());
                const auto curveColor = edgeColor;
                const auto sourcePoint = [curve](float position) {
                    if (auto* source = dynamic_cast<nicxlive::core::nodes::SourceCurveEvaluation*>(curve.get()))
                        return source->pointSource(position);
                    return compat::ngRequireObject(curve)->point(position);
                };
                if (!compat::ngRequireObject(curve)->controlPoints().empty()) {
                    Vec3Array lines;
                    for (int i = 1; i < 100; ++i) {
                        const auto a = sourcePoint(static_cast<float>((i - 1) / 100.0));
                        lines.append({a.x, a.y, 0});
                        const auto b = sourcePoint(static_cast<float>(i / 100.0));
                        lines.append({b.x, b.y, 0});
                    }
                    if (!lines.empty()) { inDbgSetBuffer(lines); inDbgDrawLines(curveColor, trans); }
                }
            } else if (auto grid = std::dynamic_pointer_cast<nicxlive::core::nodes::GridDeformer>(target)) {
                const auto base = grid->getVertices();
                if (base.size() >= 4) {
                    const auto aos = base.toArray();
                    auto xs = detail::allocateBytes([&] { return std::vector<float>(aos.size()); });
                    for (std::size_t i = 0; i < aos.size(); ++i) xs[i] = aos[i].x;
                    auto ys = detail::allocateBytes([&] { return std::vector<float>(aos.size()); });
                    for (std::size_t i = 0; i < aos.size(); ++i) ys[i] = aos[i].y;
                    compat::phobos_float_sort::sort(xs); compat::phobos_float_sort::sort(ys);
                    const auto uniq = [](std::vector<float>& values) {
                        std::vector<float> result;
                        for (float value : values)
                            if (result.empty() || !(result.back() == value))
                                detail::allocateBytes([&] { result.push_back(value); });
                        values = std::move(result);
                    };
                    uniq(xs); uniq(ys);
                    const auto cols = xs.size(), rows = ys.size();
                    if (cols >= 2 && rows >= 2 && cols * rows == base.size()) {
                        Vec3Array lines;
                        const bool haveDeform = grid->deformation.size() == base.size();
                        const auto point = [&](std::size_t i) {
                            auto p = aos[i];
                            if (haveDeform) { auto d = grid->deformation[i].toVector(); p.x += d.x; p.y += d.y; }
                            return nicxlive::core::math::Vec3{p.x, p.y, 0};
                        };
                        for (std::size_t y = 0; y < rows; ++y) for (std::size_t x = 0; x < cols; ++x) {
                            const auto i = y * cols + x;
                            const auto start = point(i);
                            if (x + 1 < cols) {
                                const auto right = point(i + 1);
                                lines.append(start); lines.append(right);
                            }
                            if (y + 1 < rows) {
                                const auto down = point(i + cols);
                                lines.append(start); lines.append(down);
                            }
                        }
                        if (!lines.empty()) { inDbgSetBuffer(lines); inDbgDrawLines(edgeColor, trans); }
                    }
                }
            }
            inDbgSetBuffer(points);
            inDbgPointsSize(10); inDbgDrawPoints({0, 0, 0, 1}, trans);
            inDbgPointsSize(6); inDbgDrawPoints(vertexColor, trans);
        }
        const auto subset = [this](Indices indices) {
            compat::Slice<Vec2> result;
            constexpr auto nan = std::numeric_limits<float>::quiet_NaN();
            for (auto i : indices)
                result.append(i < inputVertices.size() ? inputVertices[i].toVector() : Vec2{nan, nan});
            return result;
        };
        if (vtxAtMouse != UINT64_MAX && !isSelecting && vtxAtMouse < inputVertices.size()) {
            const compat::Slice<Vec2> hovered{inputVertices[vtxAtMouse].toVector()};
            drawPointSubset(hovered, {1, 1, 1, 0.3f}, trans, 15);
        }
        if (!selected.empty()) {
            if (isSelecting && !mutateSelection) {
                const auto selectedVertices = subset(selected);
                if (!selectedVertices.empty()) drawPointSubset(selectedVertices, {0.6f, 0, 0, 1}, trans);
            } else {
                const auto selectedVertices = subset(selected);
                if (!selectedVertices.empty()) drawPointSubset(selectedVertices, {1, 0, 0, 1}, trans);
            }
        }
        if (!mirrorSelected.empty()) drawPointSubset(subset(mirrorSelected), {1, 0, 1, 1}, trans);
        if (isSelecting) {
            const auto rectangle = common::ngCreateRectBuffer(selectOrigin, mousePos);
            inDbgSetBuffer(rectangle);
            inDbgDrawLines(!mutateSelection ? Vec4{1, 0, 0, 1} :
                invertSelection ? Vec4{0, 1, 1, 0.8f} : Vec4{0, 1, 0, 0.8f}, trans);
            if (!newSelected.empty()) {
                const auto newSelectedVertices = subset(newSelected);
                if (mutateSelection && invertSelection)
                    drawPointSubset(newSelectedVertices, {1, 0, 1, 1}, trans);
                else drawPointSubset(newSelectedVertices, {1, 0, 0, 1}, trans);
            }
        }
        const auto size = compat::ngRequireObject(camera)->getRealSize();
        const auto position = compat::ngRequireObject(camera)->position;
        Vec3Array axes;
        if (mirrorHoriz) axes.append(common::ngCreateLineBuffer(
            {mirrorOrigin.x, -size.y - position.y}, {mirrorOrigin.x, size.y - position.y}));
        if (mirrorVert) axes.append(common::ngCreateLineBuffer(
            {-size.x - position.x, mirrorOrigin.y}, {size.x - position.x, mirrorOrigin.y}));
        if (!axes.empty()) { inDbgSetBuffer(axes); inDbgDrawLines(edgeColor, trans); }
        if (tools.contains(toolMode)) {
            const auto currentTools = tools;
            const auto currentMode = toolMode;
            const auto found = currentTools.find(currentMode);
            if (found == currentTools.end()) throw compat::ngStaticError<compat::RangeError>(
                compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/model/mesheditor/deformable.d"), 443);
            compat::ngRequireObject(found->second)->draw(camera, keepAlive);
        }
    }
    void adjustPathTransform() override {}
};
// The native spelling denotes the actual source specialization, not another base.
using ModelDeformableEditor = IncMeshEditorOneFor<nicxlive::core::nodes::Deformable,
    project::EditMode::ModelEdit>;
std::shared_ptr<ModelDeformableEditor> ngCreateModelDeformableEditor(
    std::shared_ptr<nicxlive::core::nodes::Node>);
} // namespace nijigenerate::viewport::model::mesheditor::deformable
