#include "pathdeform.hpp"
#include <nijigenerate/actions/mesheditor.hpp>
#include <nijigenerate/core/input.hpp>
#include <nijigenerate/core/math/homogeneous.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <bit>
#include <cmath>
#include <limits>
#include <common/resource_key_map.hpp>
#include <functional>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

namespace nijigenerate::viewport::common::mesheditor::tools::pathdeform {
using namespace actions::mesheditor;
namespace {
template<class T> constexpr std::int32_t id(T value) { return static_cast<std::int32_t>(value); }
using P = PathDeformTool::PathDeformActionID;
using S = PathDeformTool::SelectActionID;
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
bool dragging() {
    return viewport::base::ngDragStartedInViewport(ImGuiMouseButton_Left) && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        core::input::ngInputIsDragRequested(ImGuiMouseButton_Left);
}
template<std::size_t N, std::size_t M> void tip(const char (&action)[N], const char (&key)[M]) {
    const auto actionText = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(action));
    const auto keyText = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral(key));
    widgets::ngStatusTooltip(actionText, keyText);
}
spline::SplinePoint& pointAt(const PathDeformTool::PathRef& path, std::size_t index, std::size_t sourceLine) {
    if (index >= compat::ngRequireObject(path)->points.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, compat::ngRequireObject(path)->points.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/pathdeform.d"), sourceLine);
    return compat::ngRequireObject(path)->points[index];
}
}
std::int32_t PathDeformTool::peek(ImGuiIO* io, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    NodeSelect::peek(io, editor);
    if (core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left)) {
        if (compat::ngRequireObject(editor)->isSelecting) compat::ngRequireObject(editor)->adjustPathTransform();
        onDragEnd(compat::ngRequireObject(editor)->mousePos, editor);
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) compat::ngRequireObject(editor)->maybeSelectOne = UINT64_MAX;
    if (mode != prevMode || core::input::ngInputIsKeyPressed(ImGuiKey_Tab)) return id(P::SwitchMode);
    auto editPath = path;
    if (compat::ngRequireObject(editor)->deforming) {
        if (!compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->getCleanDeformAction();
        editPath = compat::ngRequireObject(path)->target;
    }
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !compat::ngRequireObject(editor)->deforming) {
        const auto receiver = path;
        return id(compat::ngRequireObject(receiver)->findPoint(compat::ngRequireObject(editor)->mousePos, viewport::base::viewportZoom) != -1 ? P::RemovePoint : P::AddPoint);
    }
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto target = compat::ngRequireObject(editPath)->findPoint(compat::ngRequireObject(editor)->mousePos, viewport::base::viewportZoom);
        if (target != -1 && (compat::ngRequireObject(io)->KeyCtrl || _isRotateMode))
            return id(static_cast<std::uint32_t>(target) == lockedPoint ? P::UnsetRotateCenter : P::SetRotateCenter);
        pathDragTarget = static_cast<std::uint32_t>(target);
    }
    auto action = id(S::None);
    const bool preDragging = isDragging;
    if (dragging() && pathDragTarget != UINT32_MAX) isDragging = true;
    if (isDragging && pathDragTarget != UINT32_MAX && pathDragTarget != lockedPoint) {
        if (lockedPoint != UINT32_MAX) action = id(P::Rotate);
        else if (compat::ngRequireObject(io)->KeyShift || _isShiftMode) action = id(isDragging != preDragging ? P::StartShiftTransform : P::Shift);
        else action = id(isDragging != preDragging ? P::StartTransform : P::Transform);
    }
    if (action != id(S::None)) return action;
    if (pathDragTarget == UINT32_MAX && compat::ngRequireObject(io)->KeyAlt) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (!compat::ngRequireObject(editor)->isPointOver(compat::ngRequireObject(editor)->mousePos)) return id(S::SelectArea);
            if (compat::ngRequireObject(io)->KeyShift) return id(S::ToggleSelect);
            return id(!compat::ngRequireObject(editor)->isSelected(compat::ngRequireObject(editor)->vtxAtMouse) ? S::SelectOne : S::MaybeSelectOne);
        }
        if (!isDragging && !compat::ngRequireObject(editor)->isSelecting && core::input::ngInputIsMouseReleased(ImGuiMouseButton_Left) &&
            compat::ngRequireObject(editor)->maybeSelectOne != UINT64_MAX) return id(S::SelectMaybeSelectOne);
        if (dragging() && !compat::ngRequireObject(editor)->isSelecting) return id(S::StartDrag);
    }
    return id(S::None);
}
std::int32_t PathDeformTool::unify(compat::Slice<std::int32_t> actions) {
    struct PriorityHash {
        std::size_t operator()(std::int32_t value) const noexcept { return static_cast<std::size_t>(value); }
    };
    nicxlive::core::common::ResourceKeyMap<std::int32_t, std::int32_t,
        PriorityHash, std::equal_to<std::int32_t>> priorities;
    priorities[id(P::SwitchMode)] = 2;
    priorities[id(P::RemovePoint)] = 1;
    priorities[id(P::AddPoint)] = 1;
    priorities[id(P::TranslatePoint)] = 0;
    priorities[id(P::StartTransform)] = 0;
    priorities[id(P::StartShiftTransform)] = 0;
    priorities[id(P::Shift)] = 0;
    priorities[id(P::Transform)] = 0;
    priorities[id(P::Rotate)] = 0;
    priorities[id(P::SetRotateCenter)] = 0;
    priorities[id(P::UnsetRotateCenter)] = 0;
    priorities[id(S::None)] = 10;
    priorities[id(S::SelectArea)] = 5;
    priorities[id(S::ToggleSelect)] = 2;
    priorities[id(S::SelectOne)] = 2;
    priorities[id(S::MaybeSelectOne)] = 2;
    priorities[id(S::StartDrag)] = 2;
    priorities[id(S::SelectMaybeSelectOne)] = 2;
    auto action = id(S::None);
    auto priority = priorities.at(action);
    for (auto candidate : actions) {
        const auto found = priorities.find(candidate);
        if (found == priorities.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/pathdeform.d"), 202);
        const auto next = found->second;
        if (next < priority) { priority = next; action = candidate; }
    }
    return action;
}
bool PathDeformTool::update(ImGuiIO*, EditorRef editor, std::int32_t action, bool& changed) {
    const auto keepAlive = shared_from_this();
    changed = false;
    if (compat::ngRequireObject(editor)->deforming) tip("Deform", "Left Mouse"); else tip("Create/Destroy", "Left Mouse (x2)");
    tip("Switch Mode", "TAB"); tip("Toggle locked point", "Ctrl"); tip("Move point along with the path", "Shift");
    if (action == id(P::SwitchMode)) {
        if (!compat::ngRequireObject(path)->target) { compat::ngRequireObject(editor)->createPathTarget(); compat::ngRequireObject(editor)->getCleanDeformAction(); }
        else if (compat::ngRequireObject(editor)->hasAction()) { compat::ngRequireObject(editor)->pushDeformAction(); compat::ngRequireObject(editor)->getCleanDeformAction(); }
        compat::ngRequireObject(editor)->deforming = !compat::ngRequireObject(editor)->deforming;
        mode = compat::ngRequireObject(editor)->deforming ? Mode::Transform : Mode::Define;
        if (compat::ngRequireObject(editor)->deforming) { compat::ngRequireObject(editor)->getCleanDeformAction(); compat::ngRequireObject(editor)->updatePathTarget(); }
        else compat::ngRequireObject(editor)->resetPathTarget();
        changed = true;
    }
    auto editPath = path;
    if (compat::ngRequireObject(editor)->deforming) {
        if (!compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->getCleanDeformAction();
        editPath = compat::ngRequireObject(path)->target;
    }
    if (action == id(P::StartTransform) || action == id(P::StartShiftTransform))
        if (auto receiver = std::dynamic_pointer_cast<MeshEditorAction<DeformationAction>>(compat::ngRequireObject(editor)->getDeformAction())) compat::ngRequireObject(receiver)->clear();
    if (action == id(P::RemovePoint) || action == id(P::AddPoint)) {
        if (action == id(P::RemovePoint)) {
            const auto index = [&] {
                const auto receiver = path;
                return compat::ngRequireObject(receiver)->findPoint(compat::ngRequireObject(editor)->mousePos, viewport::base::viewportZoom);
            }();
            if (index != -1) {
                const auto receiver = path;
                compat::ngRequireObject(receiver)->removePoint(index);
            }
        } else {
            const auto receiver = path;
            compat::ngRequireObject(receiver)->addPoint(compat::ngRequireObject(editor)->mousePos, viewport::base::viewportZoom);
        }
        pathDragTarget = lockedPoint = UINT32_MAX;
        { const auto receiver = path; compat::ngRequireObject(receiver)->mapReference(); }
    } else if (action == id(P::UnsetRotateCenter)) {
        lockedPoint = pathDragTarget = UINT32_MAX; _isRotateMode = false;
    } else if (action == id(P::SetRotateCenter)) {
        lockedPoint = static_cast<std::uint32_t>(compat::ngRequireObject(editPath)->findPoint(compat::ngRequireObject(editor)->mousePos, viewport::base::viewportZoom));
        pathDragTarget = UINT32_MAX; _isRotateMode = false;
    } else if (action == id(P::Rotate)) {
        const int step = pathDragTarget > lockedPoint ? 1 : -1;
        const auto previousMouse = compat::ngRequireObject(editor)->lastMousePos;
        const auto previous = previousMouse - pointAt(editPath, lockedPoint, 281).position;
        const auto currentMouse = compat::ngRequireObject(editor)->mousePos;
        const auto current = currentMouse - pointAt(editPath, lockedPoint, 282).position;
        const float previousAngle = sourceAtan2(previous.y, previous.x), angle = sourceAtan2(current.y, current.x);
        const float delta = angle - previousAngle;
        using nicxlive::core::math::Mat4;
        const auto center = pointAt(editPath, lockedPoint, 286).position;
        const auto rotation = Mat4::multiply(Mat4::translation({center.x, center.y, 0}),
            Mat4::multiply(Mat4::zRotation(delta), Mat4::multiply(Mat4::translation({-center.x, -center.y, 0}), Mat4::identity())));
        auto index = std::bit_cast<std::int32_t>(lockedPoint + static_cast<std::uint32_t>(step));
        for (; index >= 0 && static_cast<std::size_t>(index) < compat::ngRequireObject(editPath)->points.size();
             index = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index) + static_cast<std::uint32_t>(step))) {
            const auto point = pointAt(editPath, static_cast<std::size_t>(index), 289).position;
            const auto transformed = core::math::ngTransformHomogeneousPoint(rotation, {point.x, point.y, 0});
            pointAt(editPath, static_cast<std::size_t>(index), 289).position = {transformed.x, transformed.y};
        }
    } else if (action == id(P::Shift) || action == id(P::StartShiftTransform)) {
        if (pathDragTarget != UINT32_MAX) {
            const float offset = [&] {
                const auto receiver = path;
                return compat::ngRequireObject(receiver)->findClosestPointOffset(compat::ngRequireObject(editor)->mousePos);
            }();
            const auto position = [&] {
                const auto receiver = path;
                return compat::ngRequireObject(receiver)->eval(offset);
            }();
            pointAt(editPath, pathDragTarget, 296).position = position;
        }
    } else if (action == id(P::Transform) || action == id(P::StartTransform)) {
        if (pathDragTarget != UINT32_MAX) {
            const auto translation = compat::ngRequireObject(editor)->mousePos - compat::ngRequireObject(editor)->lastMousePos;
            auto& point = pointAt(editPath, pathDragTarget, 302);
            point.position = point.position + translation;
        }
    }
    compat::ngRequireObject(editPath)->update();
    if (compat::ngRequireObject(editor)->deforming) {
        compat::ngRequireObject(editor)->updatePathTarget();
        if (compat::ngRequireObject(editor)->hasAction()) compat::ngRequireObject(editor)->markActionDirty();
        changed = true;
    } else { const auto receiver = path; compat::ngRequireObject(receiver)->mapReference(); }
    prevMode = mode;
    if (action == id(S::ToggleSelect)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX) compat::ngRequireObject(editor)->toggleSelect(compat::ngRequireObject(editor)->vtxAtMouse);
    } else if (action == id(S::SelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX) compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->vtxAtMouse); else compat::ngRequireObject(editor)->deselectAll();
    } else if (action == id(S::MaybeSelectOne)) {
        if (compat::ngRequireObject(editor)->vtxAtMouse != UINT64_MAX) compat::ngRequireObject(editor)->maybeSelectOne = compat::ngRequireObject(editor)->vtxAtMouse;
    } else if (action == id(S::SelectArea)) {
        compat::ngRequireObject(editor)->selectOrigin = compat::ngRequireObject(editor)->mousePos; compat::ngRequireObject(editor)->isSelecting = true;
    }
    if (action == id(S::SelectMaybeSelectOne) && compat::ngRequireObject(editor)->maybeSelectOne != UINT64_MAX)
        compat::ngRequireObject(editor)->selectOne(compat::ngRequireObject(editor)->maybeSelectOne);
    if (action == id(S::StartDrag)) onDragStart(compat::ngRequireObject(editor)->mousePos, editor);
    prevMode = mode;
    if (changed) compat::ngRequireObject(editor)->refreshMesh();
    return changed;
}
void PathDeformTool::draw(base::CameraRef camera, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    NodeSelect::draw(camera, editor);
    if (path && compat::ngRequireObject(path)->target && compat::ngRequireObject(editor)->deforming) {
        { const auto receiver = path; compat::ngRequireObject(receiver)->draw(compat::ngRequireObject(editor)->transform, {0, 0.6f, 0.6f, 1}, lockedPoint); }
        {
            const auto receiver = path;
            const auto target = compat::ngRequireObject(receiver)->target;
            compat::ngRequireObject(target)->draw(compat::ngRequireObject(editor)->transform, {0, 1, 0, 1}, lockedPoint);
        }
    } else if (path) {
        if (compat::ngRequireObject(path)->target) {
            const auto receiver = path;
            const auto target = compat::ngRequireObject(receiver)->target;
            compat::ngRequireObject(target)->draw(compat::ngRequireObject(editor)->transform, {0, 0.6f, 0, 1}, lockedPoint);
        }
        { const auto receiver = path; compat::ngRequireObject(receiver)->draw(compat::ngRequireObject(editor)->transform, {0, 1, 1, 1}, lockedPoint); }
    }
}
base::MeshEditorActionRef<base::DeformationAction> PathDeformTool::editorAction(
    base::NodeRef target, std::shared_ptr<base::DeformationAction> action) {
    const auto keepAlive = shared_from_this();
    return MeshEditorPathDeformAction<DeformationAction>::create(target, action);
}
base::MeshEditorActionRef<actions::GroupAction> PathDeformTool::editorAction(
    base::NodeRef target, std::shared_ptr<actions::GroupAction> action) {
    const auto keepAlive = shared_from_this();
    return MeshEditorPathDeformAction<actions::GroupAction>::create(target, action);
}
base::MeshEditorActionRef<base::DeformationAction> PathDeformTool::editorAction(
    base::DrawableRef target, std::shared_ptr<base::DeformationAction> action) {
    const auto keepAlive = shared_from_this();
    return MeshEditorPathDeformAction<DeformationAction>::create(target, action);
}
base::MeshEditorActionRef<actions::GroupAction> PathDeformTool::editorAction(
    base::DrawableRef target, std::shared_ptr<actions::GroupAction> action) {
    const auto keepAlive = shared_from_this();
    return MeshEditorPathDeformAction<actions::GroupAction>::create(target, action);
}
}
