#include "bezierdeform.hpp"
#include <nijigenerate/actions/mesh.hpp>
#include <nijigenerate/actions/vertex.hpp>
#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/core/math/vertex.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/viewport/common/mesheditor/operations/impl.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <bit>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <compat/source_allocator.hpp>

namespace nijigenerate::viewport::common::mesheditor::tools::bezierdeform {
using operations::impl::IncMeshEditorOneDeformable;
using namespace actions::mesh;
using namespace actions::vertex;
using namespace actions::mesheditor;
using base::Vec2;
using nicxlive::core::nodes::PathDeformer;
using nicxlive::core::math::Vec2Array;
namespace {
constexpr auto invalid = std::numeric_limits<std::uint64_t>::max();
// Adapted from Phobos std.math.trigonometry, LDC 1.41, float atan/atan2 paths.
// Copyright The D Language Foundation 2000-2011; CEPHES Copyright 2001
// Stephen L. Moshier. Boost Software License 1.0; see pathdeform_math.LICENSE.
float sourceAtan(float x) {
    constexpr float pi2 = 1.57079632679489661923132169163975144209858469968755f;
    constexpr float pi4 = 0.78539816339744830961566084581987572104929234984378f;
    constexpr float tanPi8 = 0.414213562373095048801688724209698078569672f;
    constexpr float tan3Pi8 = 2.414213562373095048801688724209698078569672f;
    if (x == 0.f) return x;
    if (std::isinf(x)) return std::copysign(pi2, x);
    const bool negative = std::signbit(x);
    if (negative) x = -x;
    float y;
    if (x > tan3Pi8) { y = pi2; x = -(1.f / x); }
    else if (x > tanPi8) { y = pi4; x = (x - 1.f) / (x + 1.f); }
    else y = 0.f;
    const float z = x * x;
    float polynomial = 8.05374449538E-2f;
    polynomial *= z; polynomial += -1.38776856032E-1f;
    polynomial *= z; polynomial += 1.99777106478E-1f;
    polynomial *= z; polynomial += -3.33329491539E-1f;
    y += polynomial * z * x + x;
    return negative ? -y : y;
}
float sourceAtan2(float y, float x) {
    constexpr float pi = 3.1415926535897932384626433832795028841971693993751f;
    constexpr float pi2 = 1.57079632679489661923132169163975144209858469968755f;
    constexpr float pi4 = 0.78539816339744830961566084581987572104929234984378f;
    if (std::isnan(x) || std::isnan(y)) return std::numeric_limits<float>::quiet_NaN();
    if (y == 0.f) return std::copysign(x >= 0.f && !std::signbit(x) ? 0.f : pi, y);
    if (x == 0.f) return std::copysign(pi2, y);
    if (std::isinf(x)) {
        if (std::signbit(x)) return std::copysign(std::isinf(y) ? 3.f * pi4 : pi, y);
        return std::copysign(std::isinf(y) ? pi4 : 0.f, y);
    }
    if (std::isinf(y)) return std::copysign(pi2, y);
    float z = sourceAtan(y / x);
    if (std::signbit(x)) z = std::signbit(y) ? z - pi : z + pi;
    if (z == 0.f) return std::copysign(z, y);
    return z;
}
template<class Array> decltype(auto) sourceAt(Array& values,std::size_t index,std::size_t line) {
    if(index>=values.size())throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/bezierdeform.d"),line);
    return values[index];
}
auto vertexAt(const std::shared_ptr<IncMeshEditorOneDeformable>& editor,std::size_t index,std::size_t line) {
    return compat::ngRequireObject(sourceAt(compat::ngRequireObject(editor)->vertices,index,line));
}
template<class T> constexpr std::int32_t id(T value) { return static_cast<std::int32_t>(value); }
using B = BezierDeformTool::BezierDeformActionID;
using S = BezierDeformTool::SelectActionID;
std::uint64_t point(std::shared_ptr<IncMeshEditorOneDeformable> editor) {
    return static_cast<std::uint64_t>(core::math::vertex::findPoint(compat::ngRequireObject(editor)->vertices,
        compat::ngRequireObject(editor)->mousePos, viewport::base::viewportZoom));
}
Vec2Array positions(std::shared_ptr<IncMeshEditorOneDeformable> editor) {
    const auto vertices=compat::ngRequireObject(editor)->vertices;
    Vec2Array result(vertices.size());
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i].set(compat::ngRequireObject(vertices[i])->position);
    return result;
}
actions::ActionText name(base::EditorRef editor) {
    return actions::ActionText(compat::RetainedString(compat::ngRequireObject(compat::ngRequireObject(editor)->getTarget())->getName()));
}
template<std::size_t N, std::size_t M> void tip(const char (&text)[N], const char (&key)[M]) {
    auto action = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
    auto shortcut = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
    widgets::ngStatusTooltip(std::move(action), std::move(shortcut));
}
void selection(base::EditorRef editor, std::int32_t action) {
    if (action == id(S::ToggleSelect)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != invalid) compat::ngRequireObject(editor)->toggleSelect(compat::ngRequireObject(editor)->vtxAtMouse);
    } else if (action == id(S::SelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != invalid) compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->vtxAtMouse);
        else compat::ngRequireObject(editor)->deselectAll();
    } else if (action == id(S::MaybeSelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != invalid) compat::ngRequireObject(editor)->maybeSelectOne = compat::ngRequireObject(editor)->vtxAtMouse;
    } else if (action == id(S::SelectArea)) {
        compat::ngRequireObject(editor)->selectOrigin = compat::ngRequireObject(editor)->mousePos;
        compat::ngRequireObject(editor)->isSelecting = true;
    }
    if (action == id(S::SelectMaybeSelectOne) && compat::ngRequireObject(editor)->maybeSelectOne != invalid)
        compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->maybeSelectOne);
}
}
bool BezierDeformTool::onDragStart(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (compat::ngRequireObject(editor)->deformOnly) return NodeSelect::onDragStart(mouse, editor);
    if (!compat::ngRequireObject(editor)->isSelecting && !isDragging) {
        isDragging = true;
        action = VertexMoveAction::createSource([&] {
            return std::tuple{name(editor), editor, VertexUpdate{}};
        });
        return true;
    }
    return false;
}
bool BezierDeformTool::onDragEnd(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (!compat::ngRequireObject(editor)->deformOnly && action) {
        if (auto mesh = std::dynamic_pointer_cast<MeshAction>(action)) {
            if (mesh->dirty) { mesh->updateNewState(); core::actionstack::incActionPush(action); }
        } else if (auto vertex = std::dynamic_pointer_cast<VertexAction>(action)) {
            if (compat::ngRequireObject(vertex)->dirty) { compat::ngRequireObject(vertex)->updateNewState(); core::actionstack::incActionPush(action); }
        }
        action.reset();
    }
    preDragging = false;
    return NodeSelect::onDragEnd(mouse, editor);
}
bool BezierDeformTool::onDragUpdate(Vec2 mouse, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    if (compat::ngRequireObject(editor)->deformOnly) return NodeSelect::onDragUpdate(mouse, editor);
    if (!isDragging) return false;
    if (auto move = std::dynamic_pointer_cast<VertexMoveAction>(action)) {
        const auto selected = compat::ngRequireObject(editor)->selected;
        // Escaping delegates read the Source foreach variable's one live location.
        const auto index = std::allocate_shared<std::uint64_t>(
            nicxlive::core::d_runtime::SourceAllocator<std::uint64_t>{});
        for (auto value : selected) {
            *index = value;
            compat::ngRequireObject(editor)->foreachMirror(detail::allocateBezierStorage([&] {
                return std::make_shared<const std::function<void(std::uint32_t)>>(
                    [editor, move, mouse, index](std::uint32_t axis) {
                        const auto held = compat::ngRequireObject(editor)->getVerticesByIndex({
                            compat::ngRequireObject(editor)->mirrorVertex(axis, *index)});
                        auto* vertex = sourceAt(held, 0, 111);
                        if (vertex) {
                            const auto originalPosition = compat::ngRequireObject(vertex)->position;
                            const auto displacement = compat::ngRequireObject(editor)->mirror(
                                axis, mouse - compat::ngRequireObject(editor)->lastMousePos);
                            move->moveVertex(vertex, originalPosition + displacement);
                        }
                    });
            }));
        }
    }
    if (!compat::ngRequireObject(editor)->selected.empty()) compat::ngRequireObject(editor)->maybeSelectOne = invalid;
    compat::ngRequireObject(editor)->refreshMesh();
    return true;
}
std::int32_t BezierDeformTool::peekVertexEdit(ImGuiIO* io, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    auto deform = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor);
    if (core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left)) onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) compat::ngRequireObject(editor)->maybeSelectOne = invalid;
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) return id(point(deform) != invalid ? B::RemovePoint : B::AddPoint);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (!compat::ngRequireObject(editor)->isPointOver(compat::ngRequireObject(editor)->mousePos)) return id(S::SelectArea);
        if (compat::ngRequireObject(io)->KeyShift) return id(S::ToggleSelect);
        return id(!compat::ngRequireObject(editor)->isSelected(compat::ngRequireObject(editor)->vtxAtMouse) ? S::SelectOne : S::MaybeSelectOne);
    }
    if (!isDragging && !compat::ngRequireObject(editor)->isSelecting && core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left) &&
        compat::ngRequireObject(editor)->maybeSelectOne != invalid) return id(S::SelectMaybeSelectOne);
    if (isDragging) return id(B::TranslatePoint);
    if (viewport::base::ngDragStartedInViewport(ImGuiMouseButton_Left) && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        core::input::ngInputIsDragRequested(ImGuiMouseButton_Left) && !compat::ngRequireObject(editor)->isSelecting) return id(S::StartDrag);
    return id(S::None);
}
std::int32_t BezierDeformTool::peekDeformEdit(ImGuiIO* io, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    auto deform = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor);
    if (core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left)) onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) compat::ngRequireObject(editor)->maybeSelectOne = invalid;
    if (!compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->getCleanDeformAction();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        auto target = point(deform);
        if (target == invalid) return id(S::SelectArea);
        if (compat::ngRequireObject(io)->KeyCtrl || _isRotateMode) return id(target == lockedPoint ? B::UnsetRotateCenter : B::SetRotateCenter);
        return id(!compat::ngRequireObject(editor)->isSelected(compat::ngRequireObject(editor)->vtxAtMouse) ? S::SelectOne : S::MaybeSelectOne);
    }
    if (!isDragging && !compat::ngRequireObject(editor)->isSelecting && core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left) &&
        compat::ngRequireObject(editor)->maybeSelectOne != invalid) return id(S::SelectMaybeSelectOne);
    auto result = id(S::None);
    if (isDragging && !compat::ngRequireObject(editor)->selected.empty()) {
        if (compat::ngRequireObject(editor)->selected.size() == 1 && compat::ngRequireObject(editor)->selected[0] != lockedPoint) {
            if (lockedPoint != invalid) result = id(B::Rotate);
            else if (compat::ngRequireObject(io)->KeyShift || _isShiftMode) result = id(isDragging != preDragging ? B::StartShiftTransform : B::Shift);
            else result = id(isDragging != preDragging ? B::StartTransform : B::TranslatePoint);
        }
        preDragging = true;
    }
    if (result != id(S::None)) return result;
    if (viewport::base::ngDragStartedInViewport(ImGuiMouseButton_Left) && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        core::input::ngInputIsDragRequested(ImGuiMouseButton_Left) && !compat::ngRequireObject(editor)->isSelecting) result = id(S::StartDrag);
    return result;
}
bool BezierDeformTool::updateVertexEdit(ImGuiIO*, EditorRef editor, std::int32_t command, bool& changed) {
    const auto keepAlive = shared_from_this();
    changed = false;
    auto deform = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor);
    if (!deform) return false;
    tip("Create/Destroy", "Left Mouse (x2)"); tip("Toggle locked point", "Ctrl");
    tip("Move point along with the path", "Shift");
    if (command == id(B::SwitchMode)) compat::ngRequireObject(editor)->getCleanDeformAction();
    if (command == id(B::RemovePoint) || command == id(B::AddPoint)) {
        if (command == id(B::RemovePoint)) {
            const auto index = point(deform);
            auto remove = VertexRemoveAction::createSource([&] {
                return std::tuple{name(editor), editor, VertexUpdate{}};
            });
            if (index != invalid) {
                const auto vertices=compat::ngRequireObject(editor)->getVerticesByIndex({index});
                remove->removeVertex(sourceAt(vertices,0,319));
            }
            remove->updateNewState(); core::actionstack::incActionPush(remove);
        } else {
            auto insert = VertexInsertAction::createSource([&] {
                return std::tuple{name(editor), editor, VertexUpdate{}};
            });
            bool inserted = false;
            if (auto path = std::dynamic_pointer_cast<PathDeformer>(compat::ngRequireObject(editor)->getTarget())) {
                auto curve = path->createCurve(positions(deform));
                compat::Slice<float> relative;
                relative.resize(compat::ngRequireObject(deform)->vertices.size(),
                    std::numeric_limits<float>::quiet_NaN());
                const auto vertices=compat::ngRequireObject(deform)->vertices;
                for (std::size_t i = 0; i < vertices.size(); ++i)
                    relative[i]=compat::ngRequireObject(curve)->closestPoint(compat::ngRequireObject(vertices[i])->position);
                float relNew = compat::ngRequireObject(curve)->closestPoint(compat::ngRequireObject(editor)->mousePos);
                Vec2 newPos = compat::ngRequireObject(curve)->point(relNew);
                if (core::math::vertex::isOverlapped(newPos, compat::ngRequireObject(editor)->mousePos, viewport::base::viewportZoom)) {
                    for (std::size_t i = 0; i < relative.size(); ++i) if (relNew <= relative[i]) {
                        auto vertex = core::math::mesh::ngCreateMeshVertexSource([&] { return newPos; });
                        insert->insertVertex(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i)), vertex.get());
                        inserted = true; break;
                    }
                } else if (compat::ngRequireObject(deform)->vertices.size() > 1 && relNew < 0.5f) {
                    auto vertex = core::math::mesh::ngCreateMeshVertexSource([&] {
                        return compat::ngRequireObject(editor)->mousePos;
                    });
                    insert->insertVertex(0, vertex.get()); inserted = true;
                }
            }
            if (!inserted) {
                auto vertex = core::math::mesh::ngCreateMeshVertexSource([&] {
                    return compat::ngRequireObject(editor)->mousePos;
                });
                insert->addVertex(vertex.get());
            }
            insert->updateNewState(); core::actionstack::incActionPush(insert);
        }
        compat::ngRequireObject(editor)->deselectAll(); lockedPoint = invalid;
    } else if (command == id(B::TranslatePoint) || command == id(B::StartTransform))
        changed = onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor) || changed;
    selection(editor, command);
    if (command == id(S::StartDrag) && onDragStart(compat::ngRequireObject(editor)->mousePos, editor))
        changed = onDragUpdate(compat::ngRequireObject(editor)->mousePos, editor) || changed;
    if (changed) compat::ngRequireObject(editor)->refreshMesh();
    return changed;
}
bool BezierDeformTool::updateDeformEdit(ImGuiIO* io, EditorRef editor, std::int32_t command, bool& changed) {
    const auto keepAlive = shared_from_this();
    changed = false;
    auto deform = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor);
    const auto target = compat::ngRequireObject(editor)->selected.size() == 1 ? compat::ngRequireObject(editor)->selected[0] : invalid;
    tip("Deform", "Left Mouse"); tip("Switch Mode", "TAB"); tip("Toggle locked point", "Ctrl");
    tip("Move point along with the path", "Shift");
    if (!compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->getCleanDeformAction();
    if (command == id(S::StartDrag) && onDragStart(compat::ngRequireObject(editor)->mousePos, editor)) {
        if (compat::ngRequireObject(editor)->selected.size() == 1 && target != lockedPoint) {
            if (lockedPoint != invalid) command = id(B::Rotate);
            else command = id(compat::ngRequireObject(io)->KeyShift || _isShiftMode ? B::StartShiftTransform : B::StartTransform);
        } else command = id(S::None);
        preDragging = true;
    }
    if (command == id(B::StartTransform) || command == id(B::StartShiftTransform))
        if (auto action = std::dynamic_pointer_cast<MeshEditorAction<DeformationAction>>(compat::ngRequireObject(editor)->getDeformAction())) action->clear();
    if (command == id(B::UnsetRotateCenter) || command == id(B::SetRotateCenter)) {
        lockedPoint = command == id(B::UnsetRotateCenter) ? invalid : point(deform);
        compat::ngRequireObject(editor)->deselectAll(); _isRotateMode = false;
    } else if (command == id(B::Rotate)) {
        const int step = target > lockedPoint ? 1 : -1;
        if (lockedPoint != invalid) {
            const auto previous=compat::ngRequireObject(editor)->lastMousePos-vertexAt(deform,lockedPoint,447)->position;
            const auto current=compat::ngRequireObject(editor)->mousePos-vertexAt(deform,lockedPoint,448)->position;
            const float prevAngle = sourceAtan2(previous.y, previous.x);
            const float angle = sourceAtan2(current.y, current.x);
            const float delta = angle - prevAngle;
            using nicxlive::core::math::Mat4;
            const auto negativeCenter=vertexAt(deform,lockedPoint,452)->position;
            const auto center=vertexAt(deform,lockedPoint,452)->position;
            auto rotation = Mat4::multiply(Mat4::translation({center.x, center.y, 0}),
                Mat4::multiply(Mat4::zRotation(delta), Mat4::multiply(
                    Mat4::translation({-negativeCenter.x, -negativeCenter.y, 0}), Mat4::identity())));
            auto i = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(lockedPoint) + static_cast<std::uint32_t>(step));
            for (; i >= 0 && static_cast<std::size_t>(i) < compat::ngRequireObject(deform)->vertices.size();
                 i = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(i) + static_cast<std::uint32_t>(step))) {
                auto p = vertexAt(deform,static_cast<std::size_t>(i),455)->position;
                auto moved = core::math::ngTransformHomogeneousPoint(rotation, {p.x, p.y, 0});
                vertexAt(deform,static_cast<std::size_t>(i),455)->position = {moved.x, moved.y};
                if (compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->markActionDirty();
                changed = true;
            }
        }
    } else if (command == id(B::Shift) || command == id(B::StartShiftTransform)) {
        if (compat::ngRequireObject(editor)->selected.size() == 1) if (auto path = std::dynamic_pointer_cast<PathDeformer>(compat::ngRequireObject(editor)->getTarget())) {
            const auto closest = [] {
                const auto parameter = project::incArmedParameter();
                return compat::ngRequireObject(parameter)->findClosestKeypoint();
            };
            const auto differs = [&] {
                const auto originalPoint=origCurvePoint;
                const auto key = closest();
                return originalPoint.x != key.x || originalPoint.y != key.y;
            };
            if (!origCurve || differs()) {
                const auto key = closest();
                origCurvePoint = {key.x, key.y}; origCurve = path->createCurve(positions(deform));
            }
            const auto outerCurve=originalCurve();
            const auto innerCurve=originalCurve();
            const auto position=compat::ngRequireObject(outerCurve)->point(
                compat::ngRequireObject(innerCurve)->closestPoint(compat::ngRequireObject(editor)->mousePos));
            auto& vertices=compat::ngRequireObject(deform)->vertices;
            const auto selected=sourceAt(compat::ngRequireObject(editor)->selected,0,471);
            compat::ngRequireObject(sourceAt(vertices,selected,471))->position=position;
            if (compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->markActionDirty();
            changed = true;
        }
    } else if (command == id(B::TranslatePoint) || command == id(B::StartTransform)) {
        if (compat::ngRequireObject(editor)->selected.size() == 1) {
            auto& vertices=compat::ngRequireObject(deform)->vertices;
            const auto selected=sourceAt(compat::ngRequireObject(editor)->selected,0,481);
            auto* vertex = compat::ngRequireObject(sourceAt(vertices,selected,481));
            compat::ngRequireObject(vertex)->position = compat::ngRequireObject(vertex)->position + (compat::ngRequireObject(editor)->mousePos - compat::ngRequireObject(editor)->lastMousePos);
            if (compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->markActionDirty();
            changed = true;
        }
    }
    selection(editor, command);
    if (command == id(S::StartDrag)) onDragStart(compat::ngRequireObject(editor)->mousePos, editor);
    return changed;
}
void BezierDeformTool::draw(base::CameraRef camera, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    auto deform = std::dynamic_pointer_cast<IncMeshEditorOneDeformable>(editor);
    NodeSelect::draw(camera, editor);
    if (lockedPoint != invalid) {
        const auto point = vertexAt(deform,lockedPoint,534)->position;
        nicxlive::core::math::Vec3Array points(1); points[0].set({point.x, point.y, 0});
        core::dbg::inDbgSetBuffer(points); core::dbg::inDbgPointsSize(4);
        core::dbg::inDbgDrawPoints({0, 1, 0, 1}, compat::ngRequireObject(editor)->transform);
    }
}
base::MeshEditorActionRef<base::DeformationAction> BezierDeformTool::editorAction(
    base::NodeRef target, std::shared_ptr<base::DeformationAction> action) {
    const auto keepAlive = weak_from_this().lock();
    return MeshEditorAction<DeformationAction>::create(target, action);
}
base::MeshEditorActionRef<actions::GroupAction> BezierDeformTool::editorAction(
    base::NodeRef target, std::shared_ptr<actions::GroupAction> action) {
    const auto keepAlive = weak_from_this().lock();
    return MeshEditorAction<actions::GroupAction>::create(target, action);
}
base::MeshEditorActionRef<base::DeformationAction> BezierDeformTool::editorAction(
    base::DrawableRef target, std::shared_ptr<base::DeformationAction> action) {
    const auto keepAlive = weak_from_this().lock();
    return MeshEditorAction<DeformationAction>::create(target, action);
}
base::MeshEditorActionRef<actions::GroupAction> BezierDeformTool::editorAction(
    base::DrawableRef target, std::shared_ptr<actions::GroupAction> action) {
    const auto keepAlive = weak_from_this().lock();
    return MeshEditorAction<actions::GroupAction>::create(target, action);
}
}
