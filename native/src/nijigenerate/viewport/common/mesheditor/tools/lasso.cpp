// Copyright 2020-2024 Inochi2D Project; 2025 nijigenerate Project. BSD-2-Clause; see LICENSE.
#include "lasso.hpp"
#include <compat/source_allocator.hpp>
#include <nijigenerate/viewport/base.hpp>
#include <nijigenerate/core/math/triangle_fill.hpp>
#include <nijigenerate/core/dbg.hpp>
#include <nijigenerate/widgets/statusbar.hpp>
#include <cmath>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <common/resource_key_map.hpp>
#include <functional>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::viewport::common::mesheditor::tools::lasso {
namespace {
template<class Operation>
decltype(auto) allocateLassoStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
using Vec2 = base::Vec2;
using Vec3 = nicxlive::core::math::Vec3;
constexpr auto noIndex = std::numeric_limits<std::size_t>::max();
template<std::size_t N> compat::RetainedString text(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
thread_local auto lassoTypeIcons=[] {
    struct LassoTypeHash {
        std::size_t operator()(LassoType value) const noexcept {
            return static_cast<std::size_t>(static_cast<std::int32_t>(value));
        }
    };
    nicxlive::core::common::ResourceKeyMap<LassoType, const char*,
        LassoTypeHash, std::equal_to<LassoType>> icons;
    icons[LassoType::PolyLasso]="\xee\xa4\xa2";
    icons[LassoType::RegularLasso]="\xee\x85\x95";
    return icons;
}();
const char* icon(LassoType type) {
    const auto found=lassoTypeIcons.find(type);
    if(found==lassoTypeIcons.end())throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/lasso.d"),259);
    return found->second;
}
compat::RetainedString getLassoHint(LassoType type) {
    switch (type) {
    case LassoType::PolyLasso: return text("Poly Lasso Selection Mode");
    case LassoType::RegularLasso: return text("Regular Lasso Selection Mode");
    default: throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Invalid LassoType"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/lasso.d"),47);
    }
}
LassoType getNextLassoType(LassoType type) {
    switch (type) {
    case LassoType::PolyLasso: return LassoType::RegularLasso;
    case LassoType::RegularLasso: return LassoType::PolyLasso;
    default: throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral("Invalid LassoType"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/lasso.d"),56);
    }
}
float distance(Vec2 first, Vec2 second) {
    const float x = first.x - second.x, y = first.y - second.y;
#if defined(_WIN32) && defined(_MSC_VER)
    using SourceReal = double;
#else
    using SourceReal = long double;
#endif
    SourceReal sum = 0; sum += x * x; sum += y * y;
    return static_cast<float>(std::sqrt(sum));
}
}
void ngBootstrapLassoThread() {
    [[maybe_unused]] const auto initializedCount=lassoTypeIcons.size();
}
void LassoIO::update() {
    addSelect = ImGui::IsKeyDown(ImGuiMod_Shift); removeSelect = ImGui::IsKeyDown(ImGuiMod_Ctrl);
    undo = ImGui::IsMouseClicked(ImGuiMouseButton_Right); cleanup = ImGui::IsKeyPressed(ImGuiKey_Escape);
}
void LassoTool::setNextMode() {
    const auto keepAlive = shared_from_this(); lassoType = getNextLassoType(lassoType); cleanup();
}
void LassoTool::cleanup() { lassoPoints.resize(0); rollbackCheckpoints.resize(0); }
bool LassoTool::rollbackOnce() {
    if (rollbackCheckpoints.empty()) return false;
    lassoPoints.resize(rollbackCheckpoints[rollbackCheckpoints.size() - 1]);
    rollbackCheckpoints.resize(rollbackCheckpoints.size() - 1); return true;
}
void LassoTool::commitCheckpoint() { rollbackCheckpoints.append(lassoPoints.size()); }
void LassoTool::doSelection(EditorRef editor, std::shared_ptr<LassoIO> io) {
    const auto keepAlive = shared_from_this();
    const bool add = compat::ngRequireObject(io)->addSelect, remove = compat::ngRequireObject(io)->removeSelect;
    if (lassoPoints.size() < 4) return;
    // Move the real AoS allocation into the canonical retained descriptor.
    // The delegate and pointInPolygon both copy descriptors, not polygon elements.
    const compat::Slice<Vec2> points(lassoPoints.toArray());
    const auto indices = compat::ngRequireObject(editor)->filterVertices(allocateLassoStorage([&] {
        return std::make_shared<const std::function<bool(base::Editor::MeshVertex*)>>(
            [points, editor](auto* vertex) {
                return core::math::triangle::pointInPolygon(vertex, points,
                    compat::ngRequireObject(editor)->getGroupId());
            });
    }));
    if (!add && !remove) compat::ngRequireObject(editor)->deselectAll();
    for (auto index : indices) {
        if (add && remove) compat::ngRequireObject(editor)->toggleSelect(index);
        else if (!add && remove) compat::ngRequireObject(editor)->deselect(index);
        else compat::ngRequireObject(editor)->select(index);
    }
    cleanup();
}
bool LassoTool::doSelectionTrigger(EditorRef editor, std::shared_ptr<LassoIO> io) {
    const auto keepAlive = shared_from_this();
    if (lassoPoints.size() > 2) {
        lassoPoints[lassoPoints.size()-1]=lassoPoints[0].toVector(); doSelection(editor, std::move(io)); return true;
    }
    return false;
}
bool LassoTool::update(ImGuiIO* io, EditorRef editor, std::int32_t action, bool& changed) {
    changed = false;
    const auto keepAlive = shared_from_this();
    NodeSelect::update(io, editor, action, changed);
    const auto state = std::allocate_shared<LassoIO>(
        nicxlive::core::d_runtime::SourceAllocator<LassoIO>{});
    state->update();
    const auto status = []<std::size_t N, std::size_t M>(const char (&action)[N], const char (&key)[M]) {
        const auto actionText = text(action);
        const auto keyText = text(key);
        widgets::ngStatusTooltip(actionText, keyText);
    };
    status("Add Lasso Point", "Left Mouse");
    status("Additive Selection", "Shift");
    if (state->addSelect) status("Inverse Selection", "Ctrl");
    else status("Remove Selection", "Ctrl");
    status("Delete Last Lasso Point", "Right Mouse");
    status("Clear All Lasso Points", "ESC");
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) commitCheckpoint();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || (ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        lassoPoints.size() > 0 && distance(lassoPoints[lassoPoints.size()-1].toVector(), compat::ngRequireObject(editor)->mousePos) > 14 / viewport::base::viewportZoom)) {
        if (lassoPoints.size() > 1) lassoPoints.append(lassoPoints[lassoPoints.size()-1].toVector());
        lassoPoints.append(compat::ngRequireObject(editor)->mousePos);
        if (isClosestToStart(compat::ngRequireObject(editor)->mousePos) == 0) doSelectionTrigger(editor, state);
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && lassoType == LassoType::RegularLasso) doSelectionTrigger(editor, state);
    if (state->undo) rollbackOnce(); if (state->cleanup) cleanup();
    return true;
}
bool LassoTool::isCloses(Vec2 first, Vec2 second) { return distance(first, second) < 14 / viewport::base::viewportZoom; }
std::size_t LassoTool::findClosest(Vec2 target) {
    const auto keepAlive = shared_from_this();
    std::size_t result=noIndex;
    lassoPoints.forEachValue(
        [this,target,&result](std::size_t index,Vec2 point) {
            if(!isCloses(point,target))return 0;
            result=index;
            return 1;
        });
    return result;
}
std::size_t LassoTool::isClosestToStart(Vec2 target) {
    const auto keepAlive = shared_from_this();
    if (!lassoPoints.size()) return noIndex;
    return isCloses(lassoPoints[0].toVector(), target) ? 0 : noIndex;
}
void LassoTool::draw(base::CameraRef camera, EditorRef editor) {
    const auto keepAlive = shared_from_this();
    NodeSelect::draw(camera, editor); if (!lassoPoints.size()) return;
    const auto transform = compat::ngRequireObject(editor)->deformOnly ? compat::ngRequireObject(editor)->transform : nicxlive::core::math::Mat4::identity();
    const auto index = isClosestToStart(compat::ngRequireObject(editor)->mousePos);
    if (index != noIndex) {
        const auto point = lassoPoints[index].toVector(); const Vec3 values[]{{point.x, point.y, 0}};
        core::dbg::inDbgSetBuffer(std::span<const Vec3>(values)); core::dbg::inDbgPointsSize(10);
        core::dbg::inDbgDrawPoints({1, 0, 0, 1}, transform);
    } else if (lassoType == LassoType::PolyLasso) {
        const auto point = lassoPoints[0].toVector(); const Vec3 values[]{{point.x, point.y, 0}};
        core::dbg::inDbgSetBuffer(std::span<const Vec3>(values)); core::dbg::inDbgPointsSize(7);
        core::dbg::inDbgDrawPoints({0.6f, 0.6f, 0.6f, 0.6f}, transform);
    }
    const auto lassoAoS = lassoPoints.toArray();
    compat::Slice<Vec2> tail;
    tail.resize(2);
    if(lassoAoS.empty())throw compat::ngStaticError<compat::ArrayIndexError>(noIndex,lassoAoS.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/common/mesheditor/tools/lasso.d"),233);
    tail[0] = lassoAoS.back();
    tail[1] = compat::ngRequireObject(editor)->mousePos;
    compat::Slice<Vec2> preview;
    preview.resize(lassoAoS.size() + tail.size());
    for (std::size_t i = 0; i < lassoAoS.size(); ++i) preview[i] = lassoAoS[i];
    preview[lassoAoS.size()] = tail[0];
    preview[lassoAoS.size() + 1] = tail[1];
    nicxlive::core::math::Vec3Array lines(preview.size());
    for (std::size_t i = 0; i < preview.size(); ++i) lines.set(i, {preview[i].x, preview[i].y, 0});
    core::dbg::inDbgSetBuffer(lines); core::dbg::inDbgLineWidth(3); core::dbg::inDbgDrawLines({0, 0, 0, 1}, transform);
    core::dbg::inDbgLineWidth(1); core::dbg::inDbgDrawLines({1, 1, 1, 1}, transform);
}
namespace detail {
bool displayOptions(base::Editors editors) {
    const auto tool = [&] {
        if (editors.empty()) return std::shared_ptr<LassoTool>{};
        const auto values = allocateLassoStorage([&] { return editors.values(); });
        return std::dynamic_pointer_cast<LassoTool>(compat::ngRequireObject(values[0])->getTool());
    }();
    ImGui::BeginGroup();
    const auto currentIcon = icon(compat::ngRequireObject(tool)->lassoType);
    // D ImVec4.init initializes all four float fields to NaN.
    if (widgets::ngButtonColored(currentIcon, ImVec2(0, 0), widgets::colorUndefined)) {
        const auto entries = editors;
        for (const auto& entry : entries) {
            const auto editor = entry.second;
            if (auto current = std::dynamic_pointer_cast<LassoTool>(compat::ngRequireObject(editor)->getTool())) current->setNextMode();
        }
    }
    widgets::ngTooltip(compat::RetainedStringSlice(getLassoHint(compat::ngRequireObject(tool)->lassoType))); ImGui::EndGroup(); return false;
}
}
}
